/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Implementation

    This file implements the CyberGraphics backend using the new unified
    backend interface. It provides hardware-accelerated rendering with
    full antialiasing support for both screen and DrawingBoard targets
    via RenderPort.
*/

#include "graphics/rastport.h"
#include "graphics/view.h"
#include "libraries/zunerenderer.h"
#define DEBUG 0
#include <aros/debug.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <math.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <stdlib.h>

#include "../backend_interface.h"
#include "clib/cybergraphics_protos.h"
#include "cybergfx_antialiasing.h"
#include "cybergfx_backend.h"
#include "inline/cybergraphics.h"

/*****************************************************************************/
/* CyberGfx Backend Context */
/*****************************************************************************/

typedef struct CybergfxPrivateData {
    struct Library *CyberGfxBase;
    BOOL hardware_available;
    ULONG supported_formats;
    ULONG max_width;
    ULONG max_height;

} CybergfxPrivateData;

/*****************************************************************************/
/* Forward Declarations */
/*****************************************************************************/

/* Backend functions */
static BOOL CybergfxInitBackend(ZuneBackendContext *ctx);
static void CybergfxCleanupBackend(ZuneBackendContext *ctx);
static BOOL CybergfxIsAvailable(void);
static BOOL CybergfxIsCompatible(struct RenderPort *rp);
static ULONG CybergfxGetCapabilities(void);
static ULONG CybergfxGetPixelFormat(struct BitMap *bitmap);

static BOOL CybergfxInitRenderPort(struct RenderPort *rp);
static void CybergfxCleanupRenderPort(struct RenderPort *rp);

static BOOL CybergfxPrepareColor(struct RenderPort *rp, struct InternalColor *color);
static void CybergfxReleaseColor(struct RenderPort *rp, struct InternalColor *color);

void CybergfxDrawPixel(struct RenderPort *rp, WORD x, WORD y, struct InternalColor *color, BOOL antialias);

static void CybergfxClearRenderPort(struct RenderPort *rp, struct InternalColor *color);

static void CybergfxBeginBatch(struct RenderPort *rp);
static void CybergfxEndBatch(struct RenderPort *rp);
static void CybergfxFlushBatch(struct RenderPort *rp);
static BOOL CybergfxIsBatching(struct RenderPort *rp);

static void CybergfxBlitRenderPorts(struct RenderPort *source, struct RenderPort *dest, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                    UWORD height);
static void CybergfxBlitToScreen(struct RenderPort *source, struct RastPort *screen_rp, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                 UWORD height);

static BOOL CybergfxInitDrawingBoard(struct DrawingBoard *board);
static void CybergfxCleanupDrawingBoard(struct DrawingBoard *board);

/*****************************************************************************/
/* Backend Operations Table */
/*****************************************************************************/

ZuneBackendOps cybergfx_backend_ops = {.name = "CyberGraphics",
                                       .type = BACKEND_CYBERGFX,
                                       .GetCapabilities = CybergfxGetCapabilities,

                                       .InitBackend = CybergfxInitBackend,
                                       .CleanupBackend = CybergfxCleanupBackend,
                                       .IsAvailable = CybergfxIsAvailable,
                                       .IsCompatible = CybergfxIsCompatible,
                                       .GetPixelFormat = CybergfxGetPixelFormat,

                                       .InitRenderPort = CybergfxInitRenderPort,
                                       .CleanupRenderPort = CybergfxCleanupRenderPort,

                                       .PrepareColor = CybergfxPrepareColor,
                                       .ReleaseColor = CybergfxReleaseColor,

                                       .DrawPixel = CybergfxDrawPixel,
                                       .DrawLine = CybergfxDrawLine,
                                       .DrawRectangle = CybergfxDrawRectangle,
                                       .DrawCircle = CybergfxDrawCircle,

                                       .ClearRenderPort = CybergfxClearRenderPort,

                                       .LockPixels = CybergfxLockPixels,
                                       .UnlockPixels = CybergfxUnlockPixels,
                                       .GetPixel = CybergfxGetPixel,
                                       .SetPixel = CybergfxSetPixel,

                                       .BeginBatch = CybergfxBeginBatch,
                                       .EndBatch = CybergfxEndBatch,
                                       .FlushBatch = CybergfxFlushBatch,
                                       .IsBatching = CybergfxIsBatching,

                                       .BlitRenderPorts = CybergfxBlitRenderPorts,
                                       .BlitToScreen = CybergfxBlitToScreen,

                                       .InitDrawingBoard = CybergfxInitDrawingBoard,
                                       .CleanupDrawingBoard = CybergfxCleanupDrawingBoard,

                                       /* Texture operations */
                                       .InitTexture = CybergfxInitTexture,
                                       .CleanupTexture = CybergfxCleanupTexture,
                                       .UpdateTexture = CybergfxUpdateTexture,
                                       .DrawTexture = CybergfxDrawTexture,
                                       .LockTexturePixels = CybergfxLockTexturePixels,
                                       .UnlockTexturePixels = CybergfxUnlockTexturePixels,
                                       .GetTexturePixel = CybergfxGetTexturePixel,
                                       .SetTexturePixel = CybergfxSetTexturePixel,

                                       /* Texture capabilities */
                                       .GetMaxTextureSize = CybergfxGetMaxTextureSize,
                                       .SupportsTextureFormat = CybergfxSupportsTextureFormat,

                                       .SetupClipping = CybergfxSetupClipping,
                                       .ClearClipping = CybergfxClearClipping,

                                       .reserved = {NULL, NULL}};

/*****************************************************************************/
/* DrawingBoard Management */
/*****************************************************************************/

static BOOL CybergfxInitDrawingBoard(struct DrawingBoard *board) {
    if (!board) {
        return FALSE;
    }

    D(bug("CybergfxInitDrawingBoard: Initializing DrawingBoard %p with "
          "CyberGraphics\n",
          board));

    /* CyberGraphics DrawingBoard can be hardware-accelerated */
    board->hardware_surface = TRUE;

    return TRUE;
}

static void CybergfxCleanupDrawingBoard(struct DrawingBoard *board) {
    if (!board) {
        return;
    }

    D(bug("CybergfxCleanupDrawingBoard: Cleaning up DrawingBoard %p\n", board));

    /* Nothing specific to cleanup for CyberGraphics */
}

/*****************************************************************************/
/* Global Backend Instance */
/*****************************************************************************/

static ZuneBackend cybergfx_backend = {
    .ops = &cybergfx_backend_ops, .context = NULL, .available = TRUE, .priority = 100 /* High priority - prefer CyberGfx */
};

/*****************************************************************************/
/* Backend Registration */
/*****************************************************************************/

void CleanupCybergfxBackend(void) {
    ENTER_FUNCTION("CleanupCybergfxBackend");

    if (cybergfx_backend.context) {
        CybergfxCleanupBackend(cybergfx_backend.context);
        FreeVec(cybergfx_backend.context);
        cybergfx_backend.context = NULL;
    }

    ZuneUnregisterBackend(&cybergfx_backend);

    EXIT_FUNCTION("CleanupCybergfxBackend");
}

/*****************************************************************************/
/* Backend Implementation */
/*****************************************************************************/

static BOOL CybergfxInitBackend(ZuneBackendContext *ctx) {
    ENTER_FUNCTION("CybergfxInitBackend");

    if (!ctx) {
        EXIT_FUNCTION("CybergfxInitBackend");
        return FALSE;
    }

    /* Allocate private data */
    CybergfxPrivateData *priv = AllocVec(sizeof(CybergfxPrivateData), MEMF_CLEAR);
    if (!priv) {
        D(bug("CybergfxInitBackend: Failed to allocate private data\n"));
        EXIT_FUNCTION("CybergfxInitBackend");
        return FALSE;
    }

    /* Try to open CyberGraphics library */
    priv->CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
    if (!priv->CyberGfxBase) {
        D(bug("CybergfxInitBackend: Failed to open cybergraphics.library\n"));
        FreeVec(priv);
        EXIT_FUNCTION("CybergfxInitBackend");
        return FALSE;
    }

    /* Check hardware capabilities */
    priv->hardware_available = TRUE;
    priv->supported_formats = PIXFMT_ARGB32 | PIXFMT_RGB24 | PIXFMT_RGB16;
    priv->max_width = 4096;
    priv->max_height = 4096;

    /* Set capabilities */
    ctx->capabilities = BACKEND_CAP_BASIC | BACKEND_CAP_CYBERGFX | BACKEND_CAP_DIRECTPIXEL | BACKEND_CAP_BLENDING | BACKEND_CAP_BATCHING |
                        BACKEND_CAP_ANTIALIASING | BACKEND_CAP_TEXTURES | BACKEND_CAP_TEXTURE_SCALING;

    if (priv->hardware_available) {
        ctx->capabilities |= BACKEND_CAP_HARDWARE;
    }

    ctx->private_data = priv;
    ctx->initialized = TRUE;
    cybergfx_backend.available = TRUE;

    /* Initialize corner distance cache for fast AA rendering */
    cybergfx_init_corner_cache();

    D(bug("CybergfxInitBackend: Backend initialized successfully (caps=0x%08x)\n", ctx->capabilities));
    EXIT_FUNCTION("CybergfxInitBackend");
    return TRUE;
}

static void CybergfxCleanupBackend(ZuneBackendContext *ctx) {
    ENTER_FUNCTION("CybergfxCleanupBackend");

    if (!ctx) {
        EXIT_FUNCTION("CybergfxCleanupBackend");
        return;
    }

    CybergfxPrivateData *priv = (CybergfxPrivateData *)ctx->private_data;
    if (priv) {
        if (priv->CyberGfxBase) {
            CloseLibrary(priv->CyberGfxBase);
        }
        FreeVec(priv);
    }

    ctx->private_data = NULL;
    ctx->initialized = FALSE;
    cybergfx_backend.available = FALSE;

    EXIT_FUNCTION("CybergfxCleanupBackend");
}

static ULONG CybergfxGetPixelFormat(struct BitMap *bitmap) { return GetCyberMapAttr(bitmap, CYBRMATTR_PIXFMT); }

static BOOL CybergfxIsAvailable(void) { return (CyberGfxBase != NULL); }

static BOOL CybergfxIsCompatible(struct RenderPort *rp) {
    ENTER_FUNCTION("CybergfxIsCompatible");

    /* If no RenderPort provided, assume compatible (for DrawingBoard creation) */
    if (!rp) {
        EXIT_FUNCTION("CybergfxIsCompatible");
        return TRUE;
    }

    /* Check if we have a target RastPort with a bitmap */
    if (!rp->target_rp || !rp->target_rp->BitMap) {
        D(bug("CybergfxIsCompatible: No target RastPort or BitMap\n"));
        EXIT_FUNCTION("CybergfxIsCompatible");
        return FALSE;
    }

    /* Check if CyberGraphics library is available */
    if (!CyberGfxBase) {
        D(bug("CybergfxIsCompatible: CyberGraphics library not available\n"));
        EXIT_FUNCTION("CybergfxIsCompatible");
        return FALSE;
    }

    /* Check if the bitmap is CyberGraphics compatible */
    BOOL is_compatible = GetCyberMapAttr(rp->target_rp->BitMap, CYBRMATTR_ISCYBERGFX);
    D(bug("CybergfxIsCompatible: Bitmap %s CyberGraphics compatible\n", is_compatible ? "IS" : "IS NOT"));

    EXIT_FUNCTION("CybergfxIsCompatible");
    return is_compatible;
}

static ULONG CybergfxGetCapabilities(void) {
    if (cybergfx_backend.context) {
        return cybergfx_backend.context->capabilities;
    }
    return 0;
}

static BOOL CybergfxInitRenderPort(struct RenderPort *rp) {
    ENTER_FUNCTION("CybergfxInitRenderPort");

    if (!rp) {
        EXIT_FUNCTION("CybergfxInitRenderPort");
        return FALSE;
    }

    /* No special RenderPort initialization needed for CyberGfx */
    D(bug("CybergfxInitRenderPort: RenderPort initialized %p\n", rp));

    EXIT_FUNCTION("CybergfxInitRenderPort");
    return TRUE;
}

static void CybergfxCleanupRenderPort(struct RenderPort *rp) {
    ENTER_FUNCTION("CybergfxCleanupRenderPort");

    if (!rp) {
        EXIT_FUNCTION("CybergfxCleanupRenderPort");
        return;
    }

    EXIT_FUNCTION("CybergfxCleanupRenderPort");
}

/*****************************************************************************/
/* Color Management */
/*****************************************************************************/

static BOOL CybergfxPrepareColor(struct RenderPort *rp, struct InternalColor *color) {
    if (!rp || !color) {
        return FALSE;
    }

    color->pen = -1; /* Not using pens for CyberGfx */
    color->pen_allocated = FALSE;

    return TRUE;
}

static void CybergfxReleaseColor(struct RenderPort *rp, struct InternalColor *color) {
    /* Nothing to release for CyberGfx colors */
    (void)rp;
    (void)color;
}

/*****************************************************************************/
/* Drawing Primitives */
/*****************************************************************************/

void CybergfxDrawPixel(struct RenderPort *rp, WORD x, WORD y, struct InternalColor *color, BOOL antialias) {
    ENTER_FUNCTION("CybergfxDrawPixel");

    if (!rp || !color) {
        EXIT_FUNCTION("CybergfxDrawPixel");
        return;
    }

    /* Check clipping first - early exit if pixel is clipped */
    if (!CybergfxClipPixel(rp, x, y)) {
        EXIT_FUNCTION("CybergfxDrawPixel");
        return;
    }

    /* Check what the RenderPort is targeting - check target_board first */
    if (rp->target_board) {
        /* Rendering to DrawingBoard */
        struct DrawingBoard *board = rp->target_board;
        if (board->pixels_locked) {
            /* Direct pixel access for locked boards */
            ULONG *pixels = (ULONG *)board->pixels;
            if (pixels) {
                /* Use pack_argb32 for correct format when writing directly to memory */
                ULONG pixel = pack_argb32(color->a, color->r, color->g, color->b);
                CybergfxWritePixelClamped(pixels, board->pitch / 4, board->width, board->height, x, y, pixel);
            }
        } else {
            /* Use CyberGraphics functions for unlocked boards */
            WriteRGBPixel(board->rastport, x, y, color->original_pixel);
        }
    } else if (rp->target_rp) {
        /* Rendering to screen via RastPort */
        WriteRGBPixel(rp->target_rp, x, y, color->original_pixel);
    }

    EXIT_FUNCTION("CybergfxDrawPixel");
}

/*****************************************************************************/
/* Surface Operations */
/*****************************************************************************/

static void CybergfxClearRenderPort(struct RenderPort *rp, struct InternalColor *fill_color) {
    ENTER_FUNCTION("CybergfxClearRenderPort");

    if (!rp || !fill_color) {
        EXIT_FUNCTION("CybergfxClearRenderPort");
        return;
    }

    UWORD width, height;

    /* Get dimensions based on what RenderPort is targeting */
    if (rp->target_rp) {
        width = GetCyberMapAttr(rp->target_rp->BitMap, CYBRMATTR_WIDTH);
        height = GetCyberMapAttr(rp->target_rp->BitMap, CYBRMATTR_HEIGHT);
    } else if (rp->target_board) {
        width = rp->target_board->width;
        height = rp->target_board->height;
    } else {
        EXIT_FUNCTION("CybergfxClearRenderPort");
        return;
    }

    D(bug("Found Width and height: %d, %d\n", width, height));

    struct ZuneBrush fill_brush = ZUNE_BRUSH_LITERAL_SOLID(fill_color->original_pixel);

    if (width > 0 && height > 0) {
        CybergfxDrawRectangle(rp, 0, 0, width, height, 0.0f, 0.0f, &fill_brush, NULL, TRUE, FALSE);
    }

    EXIT_FUNCTION("CybergfxClearRenderPort");
}

/*****************************************************************************/
/* Direct Pixel Access */
/*****************************************************************************/

APTR CybergfxLockPixels(struct DrawingBoard *board, ULONG *pitch_out) {
    ENTER_FUNCTION("CybergfxLockPixels");

    if (!board) {
        EXIT_FUNCTION("CybergfxLockPixels");
        return NULL;
    }

    if (board) {
        /* Lock bitmap for direct pixel access */
        board->lock_handle = LockBitMapTags(board->bitmap, LBMI_BASEADDRESS, &board->pixels, LBMI_BYTESPERROW, &board->pitch, TAG_DONE);

        if (board->lock_handle) {
            board->pixels_locked = TRUE;
            if (pitch_out)
                *pitch_out = board->pitch;

            D(bug("ZuneRenderer: Pixels locked - address: %p, pitch: %u\n", board->pixels, board->pitch));

            EXIT_FUNCTION("CybergfxLockPixels");
            return board->pixels;
        } else {
            D(bug("ZuneRenderer: Pixel lock failed\n"));
            EXIT_FUNCTION("CybergfxLockPixels");
            return NULL;
        }
    }

    EXIT_FUNCTION("CybergfxLockPixels");
    return NULL;
}

void CybergfxUnlockPixels(struct DrawingBoard *board) {
    ENTER_FUNCTION("CybergfxUnlockPixels");

    if (!board) {
        EXIT_FUNCTION("CybergfxUnlockPixels");
        return;
    }

    if (board && board->pixels_locked) {
        UnLockBitMap(board->lock_handle);
        board->lock_handle = NULL;
        board->pixels_locked = FALSE;
        board->pixels = NULL;
    }

    EXIT_FUNCTION("CybergfxUnlockPixels");
}

ULONG CybergfxGetPixel(struct DrawingBoard *board, WORD x, WORD y) {
    if (!board || !board->pixels_locked) {
        D(bug("CybergfxGetPixel: Invalid board or pixels not locked\n"));
        return 0;
    }

    if (!board || !board->pixels_locked || !board->pixels) {
        D(bug("CybergfxGetPixel: Invalid board or pixels not locked\n"));
        return 0;
    }
    if (x < 0 || y < 0 || x >= board->width || y >= board->height) {
        D(bug("CybergfxGetPixel: Coordinates out of bounds (%d,%d) for %dx%d\n", x, y, board->width, board->height));
        return 0;
    }

    if (board->pixel_format == PIXFMT_ARGB32 || board->pixel_format == PIXFMT_RGBA32) {
        ULONG *pixels = (ULONG *)board->pixels;
        ULONG pitch_pixels = board->pitch / 4;
        return pixels[y * pitch_pixels + x];
    }

    D(bug("CybergfxGetPixel: Unsupported pixel format: %d\n", board->pixel_format));
    return 0;
}

void CybergfxSetPixel(struct DrawingBoard *board, WORD x, WORD y, struct InternalColor *color) {
    if (!board || !color) {
        return;
    }

    if (!board || !board->pixels_locked || !board->pixels) {
        D(bug("CybergfxSetPixel: Invalid board or pixels not locked\n"));
        return;
    }
    if (x < 0 || y < 0 || x >= board->width || y >= board->height) {
        D(bug("CybergfxSetPixel: Coordinates out of bounds (%d,%d) for %dx%d\n", x, y, board->width, board->height));
        return;
    }

    if (board->pixel_format == PIXFMT_ARGB32 || board->pixel_format == PIXFMT_RGBA32) {
        ULONG *pixels = (ULONG *)board->pixels;
        ULONG pitch_pixels = board->pitch / 4;
        /* Use pack_argb32 for correct format when writing directly to memory */
        ULONG pixel = pack_argb32(color->a, color->r, color->g, color->b);
        CybergfxWritePixelClamped(pixels, pitch_pixels, board->width, board->height, x, y, pixel);
        return;
    }

    D(bug("CybergfxSetPixel: Unsupported pixel format: %d\n", board->pixel_format));
}

/*****************************************************************************/
/* Batching Support */
/*****************************************************************************/

static void CybergfxBeginBatch(struct RenderPort *rp) { /* TODO: Implement batching */ }

static void CybergfxEndBatch(struct RenderPort *rp) { /* TODO: Implement batching */ }

static void CybergfxFlushBatch(struct RenderPort *rp) { /* TODO: Implement batching */ }

static BOOL CybergfxIsBatching(struct RenderPort *rp) { return FALSE; /* No batching yet */ }

/*****************************************************************************/
/* Blitting Operations */
/*****************************************************************************/

static void CybergfxBlitRenderPorts(struct RenderPort *source, struct RenderPort *dest, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                    UWORD height) {
    ENTER_FUNCTION("CybergfxBlitRenderPorts");

    if (!source || !dest) {
        EXIT_FUNCTION("CybergfxBlitRenderPorts");
        return;
    }

    /* TODO: Implement RenderPort-to-RenderPort blitting */
    D(bug("CybergfxBlitRenderPorts: Blitting %dx%d from (%d,%d) to (%d,%d)\n", width, height, src_x, src_y, dest_x, dest_y));

    EXIT_FUNCTION("CybergfxBlitRenderPorts");
}

static void CybergfxBlitToScreen(struct RenderPort *source, struct RastPort *screen_rp, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                 UWORD height) {
    ENTER_FUNCTION("CybergfxBlitToScreen");

    if (!source || !screen_rp) {
        EXIT_FUNCTION("CybergfxBlitToScreen");
        return;
    }

    /* Use existing blitting functions */
    if (source->target_board) {
        // BlitDrawingBoardToScreen(source->target_board, screen_rp, src_x, src_y,
        //                          dest_x, dest_y, width, height);
    }

    EXIT_FUNCTION("CybergfxBlitToScreen");
}
