/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Implementation

    This file implements the CyberGraphics backend using the new unified
    backend interface. It provides hardware-accelerated rendering with
    full antialiasing support for both screen and DrawingBoard targets
    via RenderContext.
*/

#include "graphics/rastport.h"
#include "graphics/view.h"
#include "libraries/zunegfx.h"
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
static BOOL CybergfxIsCompatible(struct RenderContext *rctx);
static ULONG CybergfxGetCapabilities(void);
static ULONG CybergfxGetPixelFormat(struct BitMap *bitmap);

static BOOL CybergfxInitRenderContext(struct RenderContext *rctx);
static void CybergfxCleanupRenderContext(struct RenderContext *rctx);

static BOOL CybergfxPrepareColor(struct RenderContext *rctx, struct InternalColor *color);
static void CybergfxReleaseColor(struct RenderContext *rctx, struct InternalColor *color);

void CybergfxDrawPixel(struct RenderContext *rctx, WORD x, WORD y, struct InternalColor *color, BOOL antialias);

static void CybergfxClearRenderContext(struct RenderContext *rctx, struct InternalColor *color);

static void CybergfxBeginBatch(struct RenderContext *rctx);
static void CybergfxEndBatch(struct RenderContext *rctx);
static void CybergfxFlushBatch(struct RenderContext *rctx);
static BOOL CybergfxIsBatching(struct RenderContext *rctx);

static void CybergfxBlitRenderContexts(struct RenderContext *source, struct RenderContext *dest, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                    UWORD height);
static void CybergfxBlitToScreen(struct RenderContext *source, struct RastPort *screen_rp, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                 UWORD height);

static BOOL CybergfxInitDrawingBoard(struct DrawingBoard *board);
static void CybergfxCleanupDrawingBoard(struct DrawingBoard *board);

static BOOL CybergfxCopyFromDrawingBoard(struct RenderContext *rctx);
static BOOL CybergfxCopyRegionFromDrawingBoard(struct RenderContext *rctx,
                                               WORD x, WORD y, UWORD width, UWORD height);

static void CybergfxCopyFromRastPort(struct RenderContext *rctx, struct RastPort *src_rp,
                                     WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                                     UWORD width, UWORD height);

/* Text rendering (cybergfx_text.c) */
void CybergfxDrawText(struct RenderContext *rctx, WORD x, WORD y,
                      CONST_STRPTR string, UWORD count,
                      struct InternalColor *fg_color,
                      struct InternalColor *bg_color);

/* Polygon fill (cybergfx_polygon.c) */
void CybergfxFillPolygon(struct RenderContext *rctx, struct ZunePoint *points,
                         UWORD count, struct ZuneBrush *brush, BOOL antialias);

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

                                       .InitRenderContext = CybergfxInitRenderContext,
                                       .CleanupRenderContext = CybergfxCleanupRenderContext,

                                       .PrepareColor = CybergfxPrepareColor,
                                       .ReleaseColor = CybergfxReleaseColor,

                                       .DrawPixel = CybergfxDrawPixel,
                                       .DrawLine = CybergfxDrawLine,
                                       .DrawRectangle = CybergfxDrawRectangle,
                                       .DrawCircle = CybergfxDrawCircle,

                                       .ClearRenderContext = CybergfxClearRenderContext,

                                       .LockPixels = CybergfxLockPixels,
                                       .UnlockPixels = CybergfxUnlockPixels,
                                       .GetPixel = CybergfxGetPixel,
                                       .SetPixel = CybergfxSetPixel,

                                       .BeginBatch = CybergfxBeginBatch,
                                       .EndBatch = CybergfxEndBatch,
                                       .FlushBatch = CybergfxFlushBatch,
                                       .IsBatching = CybergfxIsBatching,

                                       .BlitRenderContexts = CybergfxBlitRenderContexts,
                                       .BlitToScreen = CybergfxBlitToScreen,

                                       .InitDrawingBoard = CybergfxInitDrawingBoard,
                                       .CleanupDrawingBoard = CybergfxCleanupDrawingBoard,
                                       .CopyFromDrawingBoard = CybergfxCopyFromDrawingBoard,
                                       .CopyRegionFromDrawingBoard = CybergfxCopyRegionFromDrawingBoard,

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

                                       .CopyFromRastPort = CybergfxCopyFromRastPort,

                                       /* Text rendering */
                                       .DrawText = CybergfxDrawText,

                                       /* Polygon fill */
                                       .FillPolygon = CybergfxFillPolygon,
};

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

/*
 * CybergfxCopyFromDrawingBoard - Sync DrawingBoard buffer to bitmap
 *
 * For CyberGraphics backend, the bitmap IS the render target, so no
 * synchronization is needed. This is a no-op that always returns TRUE.
 */
static BOOL CybergfxCopyFromDrawingBoard(struct RenderContext *rctx) {
    (void)rctx; /* Unused - no sync needed for CyberGfx */
    return TRUE;
}

/*
 * CybergfxCopyRegionFromDrawingBoard - Sync a region of DrawingBoard buffer to bitmap
 *
 * For CyberGraphics backend, this is also a no-op since the bitmap IS the
 * render target directly.
 */
static BOOL CybergfxCopyRegionFromDrawingBoard(struct RenderContext *rctx,
                                               WORD x, WORD y, UWORD width, UWORD height) {
    (void)rctx;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return TRUE;
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

    /* CyberGfxBase is opened centrally in DetectLibraries() - just check it */
    if (!CyberGfxBase) {
        D(bug("CybergfxInitBackend: cybergraphics.library not available\n"));
        FreeVec(priv);
        EXIT_FUNCTION("CybergfxInitBackend");
        return FALSE;
    }
    priv->CyberGfxBase = CyberGfxBase;

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
        /* CyberGfxBase is closed centrally in CleanupZuneRenderer() */
        priv->CyberGfxBase = NULL;
        FreeVec(priv);
    }

    ctx->private_data = NULL;
    ctx->initialized = FALSE;
    cybergfx_backend.available = FALSE;

    EXIT_FUNCTION("CybergfxCleanupBackend");
}

static ULONG CybergfxGetPixelFormat(struct BitMap *bitmap) { return GetCyberMapAttr(bitmap, CYBRMATTR_PIXFMT); }

static BOOL CybergfxIsAvailable(void) { return (CyberGfxBase != NULL); }

static BOOL CybergfxIsCompatible(struct RenderContext *rctx) {
    ENTER_FUNCTION("CybergfxIsCompatible");

    /* Check if CyberGraphics library is available first */
    if (!CyberGfxBase) {
        D(bug("CybergfxIsCompatible: CyberGraphics library not available\n"));
        EXIT_FUNCTION("CybergfxIsCompatible");
        return FALSE;
    }

    /* If no RenderContext provided, assume compatible (for general availability check) */
    if (!rctx) {
        EXIT_FUNCTION("CybergfxIsCompatible");
        return TRUE;
    }

    /*
     * DrawingBoard target: Always compatible if CyberGfxBase is available.
     *
     * This is called BEFORE bitmap allocation (lazy allocation), so we can't
     * check the bitmap yet. CyberGfx can create bitmaps for any DrawingBoard,
     * so we report compatible here. The actual bitmap will be allocated later
     * in CreateRenderContextWithDrawingBoard().
     */
    if (rctx->target_board) {
        D(bug("CybergfxIsCompatible: DrawingBoard target - compatible (will allocate bitmap)\n"));
        EXIT_FUNCTION("CybergfxIsCompatible");
        return TRUE;
    }

    /*
     * Direct RastPort target: Check if bitmap exists and is CyberGfx compatible.
     */
    if (!rctx->target_rastport || !rctx->target_rastport->BitMap) {
        D(bug("CybergfxIsCompatible: No target RastPort or BitMap\n"));
        EXIT_FUNCTION("CybergfxIsCompatible");
        return FALSE;
    }

    /* Check if the bitmap is CyberGraphics compatible */
    BOOL is_compatible = GetCyberMapAttr(rctx->target_rastport->BitMap, CYBRMATTR_ISCYBERGFX);
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

static BOOL CybergfxInitRenderContext(struct RenderContext *rctx) {
    ENTER_FUNCTION("CybergfxInitRenderContext");

    if (!rctx) {
        EXIT_FUNCTION("CybergfxInitRenderContext");
        return FALSE;
    }

    /* No special RenderContext initialization needed for CyberGfx */
    D(bug("CybergfxInitRenderContext: RenderContext initialized %p\n", rctx));

    EXIT_FUNCTION("CybergfxInitRenderContext");
    return TRUE;
}

static void CybergfxCleanupRenderContext(struct RenderContext *rctx) {
    ENTER_FUNCTION("CybergfxCleanupRenderContext");

    if (!rctx) {
        EXIT_FUNCTION("CybergfxCleanupRenderContext");
        return;
    }

    EXIT_FUNCTION("CybergfxCleanupRenderContext");
}

/*****************************************************************************/
/* Color Management */
/*****************************************************************************/

static BOOL CybergfxPrepareColor(struct RenderContext *rctx, struct InternalColor *color) {
    if (!rctx || !color) {
        return FALSE;
    }

    color->pen = -1; /* Not using pens for CyberGfx */
    color->pen_allocated = FALSE;

    return TRUE;
}

static void CybergfxReleaseColor(struct RenderContext *rctx, struct InternalColor *color) {
    /* Nothing to release for CyberGfx colors */
    (void)rctx;
    (void)color;
}

/*****************************************************************************/
/* Drawing Primitives */
/*****************************************************************************/

void CybergfxDrawPixel(struct RenderContext *rctx, WORD x, WORD y, struct InternalColor *color, BOOL antialias) {
    ENTER_FUNCTION("CybergfxDrawPixel");

    if (!rctx || !color) {
        EXIT_FUNCTION("CybergfxDrawPixel");
        return;
    }

    /* Check clipping first - early exit if pixel is clipped */
    if (!CybergfxClipPixel(rctx, x, y)) {
        EXIT_FUNCTION("CybergfxDrawPixel");
        return;
    }

    /* Check what the RenderContext is targeting - check target_board first */
    if (rctx->target_board) {
        /* Rendering to DrawingBoard */
        struct DrawingBoard *board = rctx->target_board;
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
    } else if (rctx->target_rastport) {
        /* Rendering to screen via RastPort */
        WriteRGBPixel(rctx->target_rastport, x, y, color->original_pixel);
    }

    EXIT_FUNCTION("CybergfxDrawPixel");
}

/*****************************************************************************/
/* Surface Operations */
/*****************************************************************************/

static void CybergfxClearRenderContext(struct RenderContext *rctx, struct InternalColor *fill_color) {
    ENTER_FUNCTION("CybergfxClearRenderContext");

    if (!rctx || !fill_color) {
        EXIT_FUNCTION("CybergfxClearRenderContext");
        return;
    }

    UWORD width, height;

    /* Get dimensions based on what RenderContext is targeting */
    if (rctx->target_rastport) {
        width = GetCyberMapAttr(rctx->target_rastport->BitMap, CYBRMATTR_WIDTH);
        height = GetCyberMapAttr(rctx->target_rastport->BitMap, CYBRMATTR_HEIGHT);
    } else if (rctx->target_board) {
        width = rctx->target_board->width;
        height = rctx->target_board->height;
    } else {
        EXIT_FUNCTION("CybergfxClearRenderContext");
        return;
    }

    D(bug("Found Width and height: %d, %d\n", width, height));

    struct ZuneBrush fill_brush = ZUNE_BRUSH_LITERAL_SOLID(fill_color->original_pixel);

    if (width > 0 && height > 0) {
        CybergfxDrawRectangle(rctx, 0, 0, width, height, 0.0f, 0.0f, &fill_brush, NULL, TRUE, FALSE);
    }

    EXIT_FUNCTION("CybergfxClearRenderContext");
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

static void CybergfxBeginBatch(struct RenderContext *rctx) {
    if (rctx) rctx->batching_enabled = TRUE;
}

static void CybergfxEndBatch(struct RenderContext *rctx) {
    if (rctx) rctx->batching_enabled = FALSE;
}

static void CybergfxFlushBatch(struct RenderContext *rctx) {
    /* Command execution is handled by the library layer (ExecuteBatchCommands) */
}

static BOOL CybergfxIsBatching(struct RenderContext *rctx) {
    return rctx ? rctx->batching_enabled : FALSE;
}

/*****************************************************************************/
/* Blitting Operations */
/*****************************************************************************/

static void CybergfxBlitRenderContexts(struct RenderContext *source, struct RenderContext *dest, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
                                    UWORD height) {
    ENTER_FUNCTION("CybergfxBlitRenderContexts");

    if (!source || !dest) {
        EXIT_FUNCTION("CybergfxBlitRenderContexts");
        return;
    }

    /* TODO: Implement RenderContext-to-RenderContext blitting */
    D(bug("CybergfxBlitRenderContexts: Blitting %dx%d from (%d,%d) to (%d,%d)\n", width, height, src_x, src_y, dest_x, dest_y));

    EXIT_FUNCTION("CybergfxBlitRenderContexts");
}

static void CybergfxBlitToScreen(struct RenderContext *source, struct RastPort *screen_rp, WORD src_x, WORD src_y, WORD dest_x, WORD dest_y, UWORD width,
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

/*****************************************************************************/
/* RastPort Copy Operations */
/*****************************************************************************/

/*
 * CybergfxCopyFromRastPort - Copy pixels from a RastPort into DrawingBoard
 *
 * This function reads pixels from a source RastPort (e.g., window background)
 * and writes them into the DrawingBoard's pixel buffer. This is used for
 * proper alpha blending when drawing antialiased content over existing
 * background.
 *
 * For CyberGfx backend, this uses ReadPixelArray directly since we have
 * direct pixel access to the DrawingBoard's bitmap.
 */
static void CybergfxCopyFromRastPort(struct RenderContext *rctx, struct RastPort *src_rp,
                                     WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                                     UWORD width, UWORD height)
{
    struct DrawingBoard *board;

    ENTER_FUNCTION("CybergfxCopyFromRastPort");

    if (!rctx || !src_rp) {
        D(bug("CybergfxCopyFromRastPort: Invalid parameters (rctx=%p, src_rp=%p)\n", rctx, src_rp));
        EXIT_FUNCTION("CybergfxCopyFromRastPort");
        return;
    }

    board = rctx->target_board;
    if (!board) {
        D(bug("CybergfxCopyFromRastPort: RenderContext has no DrawingBoard\n"));
        EXIT_FUNCTION("CybergfxCopyFromRastPort");
        return;
    }

    /* Validate dimensions */
    if (width == 0 || height == 0) {
        EXIT_FUNCTION("CybergfxCopyFromRastPort");
        return;
    }

    /* Clamp to DrawingBoard bounds */
    if (dst_x < 0) {
        width += dst_x;
        src_x -= dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        height += dst_y;
        src_y -= dst_y;
        dst_y = 0;
    }
    if (dst_x + width > board->width) {
        width = board->width - dst_x;
    }
    if (dst_y + height > board->height) {
        height = board->height - dst_y;
    }

    if (width <= 0 || height <= 0) {
        EXIT_FUNCTION("CybergfxCopyFromRastPort");
        return;
    }

    D(bug("CybergfxCopyFromRastPort: Copying %dx%d from src(%d,%d) to dst(%d,%d)\n",
          width, height, src_x, src_y, dst_x, dst_y));

    /*
     * If the DrawingBoard is locked, write directly to the pixel buffer.
     * Otherwise, use the bitmap's RastPort.
     */
    if (board->pixels_locked && board->pixels) {
        /* Direct pixel buffer access */
        UBYTE *dst_pixels = (UBYTE *)board->pixels;
        ULONG dst_offset = dst_y * board->pitch + dst_x * 4; /* Assuming 32-bit ARGB */

        ReadPixelArray(dst_pixels + dst_offset, 0, 0, board->pitch,
                       src_rp, src_x, src_y, width, height, RECTFMT_ARGB);
    } else if (board->rastport && board->bitmap) {
        /* Use bitmap - need to lock temporarily */
        APTR lock_handle;
        APTR pixels;
        ULONG pitch;

        lock_handle = LockBitMapTags(board->bitmap,
                                     LBMI_BASEADDRESS, &pixels,
                                     LBMI_BYTESPERROW, &pitch,
                                     TAG_DONE);
        if (lock_handle) {
            UBYTE *dst_pixels = (UBYTE *)pixels;
            ULONG dst_offset = dst_y * pitch + dst_x * 4;

            ReadPixelArray(dst_pixels + dst_offset, 0, 0, pitch,
                           src_rp, src_x, src_y, width, height, RECTFMT_ARGB);

            UnLockBitMap(lock_handle);
        } else {
            D(bug("CybergfxCopyFromRastPort: Failed to lock bitmap\n"));
        }
    } else {
        D(bug("CybergfxCopyFromRastPort: No valid pixel target available\n"));
    }

    EXIT_FUNCTION("CybergfxCopyFromRastPort");
}
