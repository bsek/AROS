/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - DrawingBoard Internal Functions
*/

#include <aros/libcall.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <string.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************/
/* DrawingBoard Internal Functions */
/*****************************************************************************/

void SetPixelInternal(struct RenderContext *rctx, WORD x, WORD y, ULONG color) {
  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);

  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->SetPixel) {
    backend->ops->SetPixel(rctx->target_board, x, y, &internal_color);
  } else {
    ZuneFallback_SetPixel(rctx->target_board, x, y, &internal_color);
  }
}

ULONG GetPixelInternal(struct RenderContext *rctx, WORD x, WORD y) {
  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->GetPixel) {
    return backend->ops->GetPixel(rctx->target_board, x, y);
  }
  return ZuneFallback_GetPixel(rctx->target_board, x, y);
}

BOOL AllocateDrawingBoardBitmap(struct DrawingBoard *board,
                                ZuneBackendType backend_type,
                                struct BitMap *friend_bitmap) {
  if (!board)
    return FALSE;

  ENTER_FUNCTION("AllocateDrawingBoardBitmap");

  D(bug("ZuneRenderer: Allocating bitmap %dx%dx%d, flags=0x%08x, friend=%p\n",
        board->width, board->height, board->depth, board->flags, friend_bitmap));

  /* Detect CyberGraphics availability */
  board->hardware_surface = FALSE;

  if (backend_type == BACKEND_CYBERGFX) {
    /* CyberGraphics path */
    D(bug("ZuneRenderer: Using CyberGraphics backend\n"));

    /* Try hardware surface first if requested */
    if (board->flags & ZUNE_DRAWINGBOARD_HARDWARE) {
      D(bug("ZuneRenderer: Attempting hardware surface allocation\n"));
      board->bitmap = AllocBitMap(board->width, board->height, board->depth,
                                  BMF_DISPLAYABLE | BMF_CLEAR, friend_bitmap);
      if (board->bitmap) {
        /* Check if it's really a CyberGraphics bitmap */
        if (GetCyberMapAttr(board->bitmap, CYBRMATTR_ISCYBERGFX)) {
          board->hardware_surface = TRUE;
          D(bug("ZuneRenderer: Hardware surface allocated successfully\n"));
        } else {
          /* Not a CyberGraphics bitmap, free and try software */
          D(bug("ZuneRenderer: Allocated bitmap is not CyberGraphics "
                "compatible\n"));
          FreeBitMap(board->bitmap);
          board->bitmap = NULL;
        }
      } else {
        D(bug("ZuneRenderer: Hardware surface allocation failed\n"));
      }
    }

    /* Software surface fallback */
    if (!board->bitmap) {
      if ((board->flags & ZUNE_DRAWINGBOARD_LINEARMEM) || (board->flags & ZUNE_DRAWINGBOARD_ALPHA)) {
        /* LINEARMEM or ALPHA flag: Force ARGB32 format for direct pixel access
         * or alpha channel support. This gives us linear memory that can be 
         * locked, but loses colormap inheritance from friend_bitmap. */
        D(bug("ZuneRenderer: Allocating ARGB32 surface (LINEARMEM or ALPHA flag)\n"));
        board->bitmap = AllocBitMap(board->width, board->height, 32,
                                    BMF_CLEAR | BMF_SPECIALFMT | SHIFT_PIXFMT(PIXFMT_ARGB32),
                                    NULL);
        board->pixel_format = PIXFMT_ARGB32;
      } else {
        /* Default: Use friend_bitmap to inherit colormap for legacy pen drawing.
         * The bitmap may not support linear memory access (can't be locked),
         * but AA rendering will fall back to RasterPort path which still works. */
        D(bug("ZuneRenderer: Allocating software surface with friend_bitmap\n"));
        board->bitmap = AllocBitMap(board->width, board->height, board->depth,
                                    BMF_CLEAR, friend_bitmap);
      }
      board->hardware_surface = FALSE;
    }

    /* Get CyberGraphics attributes */
    if (board->bitmap) {
      board->pixel_format = GetCyberMapAttr(board->bitmap, CYBRMATTR_PIXFMT);
      D(bug("ZuneRenderer: Pixel format: %u\n", board->pixel_format));
    }
  } else if (backend_type == BACKEND_OPENGL) {
    /*
     * OpenGL backend requires ARGB32 format to preserve alpha channel.
     * We cannot use friend_bitmap here because it would inherit the screen's
     * pixel format (typically 24-bit RGB without alpha), which causes
     * WritePixelArray to discard the alpha channel when syncing FBO to bitmap.
     */
    D(bug("ZuneRenderer: Creating ARGB32 bitmap for OpenGL backend\n"));
    board->bitmap = AllocBitMap(board->width, board->height, 32,
                                BMF_CLEAR | BMF_SPECIALFMT | SHIFT_PIXFMT(PIXFMT_ARGB32),
                                NULL);
    board->hardware_surface = FALSE;
    if (board->bitmap) {
      board->pixel_format = PIXFMT_ARGB32;
      D(bug("ZuneRenderer: ARGB32 bitmap allocated for OpenGL\n"));
    }
  } else {
    /* Standard graphics.library path */
    D(bug("ZuneRenderer: Creating graphics.library bitmap\n"));
    board->bitmap =
        AllocBitMap(board->width, board->height, board->depth, BMF_CLEAR, friend_bitmap);
    board->hardware_surface = FALSE;
    board->pixel_format = 0; /* Not applicable for graphics.library */
  }

  if (!board->bitmap) {
    D(bug("ZuneRenderer: Bitmap allocation failed\n"));
    EXIT_FUNCTION("AllocateDrawingBoardBitmap");
    return FALSE;
  }

  /* Allocate RastPort for the DrawingBoard */
  if (!board->rastport) {
    board->rastport =
        AllocVec(sizeof(struct RastPort), MEMF_CLEAR | MEMF_PUBLIC);
    if (!board->rastport) {
      D(bug("ZuneRenderer: RastPort allocation failed\n"));
      FreeBitMap(board->bitmap);
      board->bitmap = NULL;
      EXIT_FUNCTION("AllocateDrawingBoardBitmap");
      return FALSE;
    }
    D(bug("ZuneRenderer: RastPort allocated for DrawingBoard\n"));
  }

  /* Initialize the RastPort and associate it with the bitmap */
  InitRastPort(board->rastport);
  board->rastport->BitMap = board->bitmap;
  D(bug("ZuneRenderer: RastPort initialized with BitMap %p\n", board->bitmap));

  D(bug("ZuneRenderer: Bitmap allocated successfully (%s surface)\n",
        board->hardware_surface ? "hardware" : "software"));

  EXIT_FUNCTION("AllocateDrawingBoardBitmap");
  return TRUE;
}

void FreeDrawingBoardBitmap(struct DrawingBoard *board) {
  if (!board)
    return;

  ENTER_FUNCTION("FreeDrawingBoardBitmap");

  /* Clear pixel access fields */
  board->pixels = NULL;
  board->lock_handle = NULL;
  board->pitch = 0;
  board->pixels_locked = FALSE;

  if (board->bitmap) {
    FreeBitMap(board->bitmap);
    board->bitmap = NULL;
  }

  /* Free RastPort if allocated */
  if (board->rastport) {
    FreeVec(board->rastport);
    board->rastport = NULL;
  }

  EXIT_FUNCTION("FreeDrawingBoardBitmap");
}

void InitDrawingBoard(struct DrawingBoard *board) {
  if (!board)
    return;

  ENTER_FUNCTION("InitDrawingBoard");

  /* Initialize basic fields */
  board->pixels = NULL;
  board->lock_handle = NULL;
  board->pitch = 0;
  board->pixel_format = 0;
  board->pixels_locked = FALSE;
  board->hardware_surface = FALSE;
  board->rastport = NULL;
  board->valid = TRUE;
  board->colormap = NULL;

  EXIT_FUNCTION("InitDrawingBoard");
}

void CleanupDrawingBoard(struct RenderContext *rctx, struct DrawingBoard *board) {
  ZuneBackend *backend;

  if (!board)
    return;

  ENTER_FUNCTION("CleanupDrawingBoard");

  /* Mark as invalid */
  board->valid = FALSE;

  /*
   * Cleanup backend-specific data (e.g., OpenGL FBO)
   * This must be done before freeing the bitmap.
   * Use the backend interface for proper abstraction.
   */
  if (board->backend_data) {
    backend = rctx ? ZuneGetRenderContextBackend(rctx) : ZuneFindBackendByType(BACKEND_OPENGL);
    if (backend && backend->ops && backend->ops->CleanupDrawingBoard) {
      backend->ops->CleanupDrawingBoard(board);
    }
    board->backend_data = NULL;
  }

  /* Free bitmap and associated resources */
  /* Note: FreeDrawingBoardBitmap() already frees the rastport */
  FreeDrawingBoardBitmap(board);

  /* Clear all fields */
  board->width = 0;
  board->height = 0;
  board->depth = 0;
  board->flags = 0;
  board->colormap = NULL;

  EXIT_FUNCTION("CleanupDrawingBoard");
}

APTR LockDrawingBoardPixelsInternal(struct RenderContext *rctx, ULONG *pitch) {
  if (!ValidateDrawingBoard(rctx->target_board)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard\n"));
    return NULL;
  }

  if (rctx->target_board->pixels_locked) {
    D(bug("ZuneRenderer: DrawingBoard already locked\n"));
    return NULL;
  }

  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->LockPixels) {
    return backend->ops->LockPixels(rctx->target_board, pitch);
  }

  return ZuneFallback_LockPixels(rctx->target_board, pitch);
}

void UnlockDrawingBoardPixelsInternal(struct RenderContext *rctx) {
  if (!rctx) {
    D(bug("ZuneRenderer: ZuneUnlockDrawingBoardPixels called with NULL RenderContext\n"));
    return;
  }
  if (!ValidateDrawingBoard(rctx->target_board)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard\n"));
    return;
  }
  if (!rctx->target_board->pixels_locked) {
    D(bug("ZuneRenderer: DrawingBoard already UNlocked\n"));
    return;
  }

  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->UnlockPixels) {
    backend->ops->UnlockPixels(rctx->target_board);
    return;
  }

  ZuneFallback_UnlockPixels(rctx->target_board);
}

/*****************************************************************************/
/* Resource Tracking */
/*****************************************************************************/

void AddDrawingBoardToList(struct IntZuneGfxBase *base,
                           struct DrawingBoard *board) {
  if (!base || !board)
    return;

  ObtainSemaphore(&base->lock);
  AddTail((struct List *)&base->drawingboards, (struct Node *)&board->node);
  ReleaseSemaphore(&base->lock);
}

void RemoveDrawingBoardFromList(struct IntZuneGfxBase *base,
                                struct DrawingBoard *board) {
  if (!base || !board)
    return;

  ObtainSemaphore(&base->lock);
  Remove((struct Node *)&board->node);
  ReleaseSemaphore(&base->lock);
}

/*****************************************************************************/
/* Internal DrawingBoard Creation */
/*****************************************************************************/

struct DrawingBoard *CreateDrawingBoardForRenderContextInternal(
    struct IntZuneGfxBase *base,
    struct RenderContext *rctx,
    UWORD width, UWORD height, ULONG flags)
{
  struct DrawingBoard *board;
  UBYTE depth;

  ENTER_FUNCTION("CreateDrawingBoardForRenderContextInternal");

  D(bug("ZuneRenderer: CreateDrawingBoardForRenderContextInternal(rctx=%p, %dx%d, flags=0x%08x)\n",
        rctx, width, height, flags));

  /* Validate parameters */
  if (!rctx || !rctx->valid) {
    D(bug("ZuneRenderer: Invalid RenderContext\n"));
    EXIT_FUNCTION("CreateDrawingBoardForRenderContextInternal");
    return NULL;
  }

  if (width == 0 || height == 0) {
    D(bug("ZuneRenderer: Invalid dimensions\n"));
    EXIT_FUNCTION("CreateDrawingBoardForRenderContextInternal");
    return NULL;
  }

  /* Determine depth from window bitmap */
  if (rctx->window && rctx->window->RPort && rctx->window->RPort->BitMap) {
    depth = GetBitMapAttr(rctx->window->RPort->BitMap, BMA_DEPTH);
  } else {
    depth = 32;  /* Default to 32-bit */
  }

  /*
   * For OpenGL backend, we MUST use 32-bit depth to preserve alpha channel.
   * The FBO renders with RGBA, and we need the alpha to survive when syncing
   * from FBO to bitmap via WritePixelArray. A 24-bit bitmap would discard alpha.
   *
   * Also force 32-bit if ZUNE_DRAWINGBOARD_ALPHA flag is set, for compositing.
   */
  if ((rctx->backend_type == BACKEND_OPENGL || (flags & ZUNE_DRAWINGBOARD_ALPHA)) && depth < 32) {
    D(bug("ZuneRenderer: Forcing 32-bit depth for alpha support (was %d)\n", depth));
    depth = 32;
  }

  /* Allocate DrawingBoard structure */
  board = AllocVec(sizeof(struct DrawingBoard), MEMF_CLEAR | MEMF_PUBLIC);
  if (!board) {
    D(bug("ZuneRenderer: Failed to allocate DrawingBoard\n"));
    EXIT_FUNCTION("CreateDrawingBoardForRenderContextInternal");
    return NULL;
  }

  /* Set basic properties */
  board->width = width;
  board->height = height;
  board->depth = depth;
  board->flags = flags | ZUNE_DRAWINGBOARD_CACHED;

  /* Store parent window for OpenGL FBO support */
  if (rctx->window) {
    board->parent_window = rctx->window;
  }

  /* Initialize DrawingBoard */
  InitDrawingBoard(board);

  /*
   * Allocate BitMap. If LINEARMEM flag is set, we skip friend_bitmap
   * to get a bitmap with linear memory access. Otherwise, we use
   * friend_bitmap for colormap inheritance.
   */
  struct BitMap *friend_bitmap = NULL;
  if (!(flags & ZUNE_DRAWINGBOARD_LINEARMEM)) {
    if (rctx->window && rctx->window->RPort && rctx->window->RPort->BitMap) {
      friend_bitmap = rctx->window->RPort->BitMap;
    }
  }

  if (!AllocateDrawingBoardBitmap(board, rctx->backend_type, friend_bitmap)) {
    D(bug("ZuneRenderer: Failed to allocate DrawingBoard bitmap\n"));
    FreeVec(board);
    EXIT_FUNCTION("CreateDrawingBoardForRenderContextInternal");
    return NULL;
  }

  /* Store colormap from RenderContext for legacy pen-based drawing. */
  if (rctx->colormap) {
    board->colormap = rctx->colormap;
    D(bug("ZuneRenderer: DrawingBoard colormap set from RenderContext: %p\n", board->colormap));
  }

  D(bug("ZuneRenderer: DrawingBoard bitmap allocated: %p\n", board->bitmap));

  /*
   * For OpenGL backend: FBO will be created lazily when the DrawingBoard
   * is first used as a render target. The FBO is stored in backend_data.
   *
   * This happens in OpenGL_SwitchToDrawingBoard() when ZuneSetTarget() is called.
   */

  /* Add to tracking list */
  AddDrawingBoardToList(base, board);

  D(bug("ZuneRenderer: DrawingBoard created for RenderContext, bitmap=%p\n",
        board->bitmap));

  EXIT_FUNCTION("CreateDrawingBoardForRenderContextInternal");
  return board;
}

/*****************************************************************************/
/* Validation Functions */
/*****************************************************************************/

BOOL ValidateDrawingBoard(struct DrawingBoard *board) {
  if (!board) {
    return FALSE;
  }

  if (!board->valid) {
    return FALSE;
  }

  /*
   * Note: board->bitmap may be NULL for OpenGL backend!
   * OpenGL uses its own framebuffer and doesn't need a bitmap.
   * We only check dimensions here - bitmap is optional.
   */

  if (board->width == 0 || board->height == 0) {
    return FALSE;
  }

  return TRUE;
}

/*****************************************************************************/
/* Utility Functions */
/*****************************************************************************/

ULONG GetDrawingBoardPixelFormat(struct DrawingBoard *board) {
  if (!ValidateDrawingBoard(board))
    return 0;
  return board->pixel_format;
}

ULONG GetBytesPerPixel(ULONG pixel_format) {
  switch (pixel_format) {
  case PIXFMT_ARGB32:
  case PIXFMT_RGBA32:
    return 4;
  case PIXFMT_RGB24:
    return 3;
  case PIXFMT_RGB16:
  case PIXFMT_RGB15:
    return 2;
  case PIXFMT_LUT8:
    return 1;
  default:
    return 4; /* Default to 32-bit */
  }
}

BOOL IsPixelFormatSupported(ULONG pixel_format) {
  switch (pixel_format) {
  case PIXFMT_ARGB32:
  case PIXFMT_RGBA32:
  case PIXFMT_RGB24:
  case PIXFMT_RGB16:
  case PIXFMT_RGB15:
  case PIXFMT_LUT8:
    return TRUE;
  default:
    return FALSE;
  }
}

/*****************************************************************************/
/* Debug Functions */
/*****************************************************************************/

void DumpDrawingBoard(struct DrawingBoard *board) {
  if (!board)
    return;

  D(bug("=== DrawingBoard Dump ===\n"));
  D(bug("DrawingBoard: %p\n", board));
  D(bug("Valid: %s\n", board->valid ? "Yes" : "No"));
  D(bug("Dimensions: %dx%dx%d\n", board->width, board->height, board->depth));
  D(bug("Flags: 0x%08x\n", board->flags));
  D(bug("Bitmap: %p\n", board->bitmap));
  D(bug("Hardware surface: %s\n", board->hardware_surface ? "Yes" : "No"));
  D(bug("Pixels locked: %s\n", board->pixels_locked ? "Yes" : "No"));
  if (board->pixels_locked) {
    D(bug("Pixel buffer: %p\n", board->pixels));
    D(bug("Pitch: %u bytes\n", board->pitch));
    D(bug("Pixel format: %u\n", board->pixel_format));
  }
  D(bug("=========================\n"));
}

/*****************************************************************************/
/* Surface Blitting Operations */
/*****************************************************************************/

void BlitDrawingBoardInternal(struct DrawingBoard *src,
                              struct DrawingBoard *dst, WORD src_x, WORD src_y,
                              WORD dst_x, WORD dst_y, UWORD width,
                              UWORD height) {

  D(bug("ZuneRenderer: Blitting between DrawingBoards (%d,%d)->(%d,%d) %dx%d\n",
        src_x, src_y, dst_x, dst_y, width, height));

  /*
   * OpenGL to OpenGL: Not directly supported - would need texture copy
   * OpenGL to Bitmap: Would need glReadPixels (not implemented yet)
   * Bitmap to OpenGL: Would need texture upload (not implemented yet)
   */

  /* Use fast blit if both surfaces are locked */
  if (src->pixels_locked && dst->pixels_locked) {
    FastBlitInternal(src, dst, src_x, src_y, dst_x, dst_y, width, height);
  } else {
    /* Use standard bitmap blitting */
    BltBitMap(src->bitmap, src_x, src_y, dst->bitmap, dst_x, dst_y, width,
              height, 0xC0, 0xFF, NULL);
  }
}

void FastBlitInternal(struct DrawingBoard *src, struct DrawingBoard *dst,
                      WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                      UWORD width, UWORD height) {
  ENTER_FUNCTION("FastBlitInternal");

  /* Validate parameters */
  if (!src || !dst || !src->pixels_locked || !dst->pixels_locked) {
    D(bug("FastBlitInternal: Invalid parameters or unlocked surfaces\n"));
    return;
  }

  /* Validate coordinates */
  if (src_x < 0 || src_y < 0 || dst_x < 0 || dst_y < 0) {
    D(bug("FastBlitInternal: Invalid coordinates\n"));
    return;
  }

  /* Clip to source boundaries */
  if (src_x >= src->width || src_y >= src->height) {
    return; /* Source coordinates outside source surface */
  }

  /* Clip to destination boundaries */
  if (dst_x >= dst->width || dst_y >= dst->height) {
    return; /* Destination coordinates outside destination surface */
  }

  /* Clip width and height to fit both surfaces */
  if (src_x + width > src->width) {
    width = src->width - src_x;
  }
  if (src_y + height > src->height) {
    height = src->height - src_y;
  }
  if (dst_x + width > dst->width) {
    width = dst->width - dst_x;
  }
  if (dst_y + height > dst->height) {
    height = dst->height - dst_y;
  }

  /* Check if there's anything left to blit after clipping */
  if (width <= 0 || height <= 0) {
    D(bug("FastBlitInternal: Nothing to blit after clipping\n"));
    return;
  }

  /* Get bytes per pixel for both surfaces */
  ULONG src_bpp = GetBytesPerPixel(src->pixel_format);
  ULONG dst_bpp = GetBytesPerPixel(dst->pixel_format);

  if (src_bpp == 0 || dst_bpp == 0) {
    D(bug("FastBlitInternal: Unsupported pixel format\n"));
    return;
  }

  /* Fast path: same pixel format */
  if (src->pixel_format == dst->pixel_format && src_bpp == dst_bpp) {
    UBYTE *src_ptr =
        (UBYTE *)src->pixels + (src_y * src->pitch) + (src_x * src_bpp);
    UBYTE *dst_ptr =
        (UBYTE *)dst->pixels + (dst_y * dst->pitch) + (dst_x * dst_bpp);

    ULONG copy_width = width * src_bpp;

    /* Check for overlapping regions in the same surface */
    if (src == dst) {
      BOOL overlapping = FALSE;

      /* Check if rectangles overlap */
      if (!(dst_x >= src_x + width || src_x >= dst_x + width ||
            dst_y >= src_y + height || src_y >= dst_y + height)) {
        overlapping = TRUE;
      }

      if (overlapping) {
        /* Use memmove for overlapping regions */
        for (UWORD y = 0; y < height; y++) {
          memmove(dst_ptr + (y * dst->pitch), src_ptr + (y * src->pitch),
                  copy_width);
        }
      } else {
        /* Use memcpy for non-overlapping regions */
        for (UWORD y = 0; y < height; y++) {
          memcpy(dst_ptr + (y * dst->pitch), src_ptr + (y * src->pitch),
                 copy_width);
        }
      }
    } else {
      /* Different surfaces - use memcpy */
      for (UWORD y = 0; y < height; y++) {
        memcpy(dst_ptr + (y * dst->pitch), src_ptr + (y * src->pitch),
               copy_width);
      }
    }
  } else {
    /* Slow path: different pixel formats - need pixel-by-pixel conversion */
    D(bug("FastBlitInternal: Format conversion required (src=%ld, dst=%ld)\n",
          src->pixel_format, dst->pixel_format));

    for (UWORD y = 0; y < height; y++) {
      for (UWORD x = 0; x < width; x++) {
        //        ULONG pixel = GetPixelInternal(src, src_x + x, src_y + y);
        //  SetPixelInternal(dst, dst_x + x, dst_y + y, pixel);
      }
    }
  }

  EXIT_FUNCTION("FastBlitInternal");
}
