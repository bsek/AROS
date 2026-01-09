/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Simplified DrawingBoard Implementation

    This module provides complete DrawingBoard (off-screen surface)
   functionality with direct pixel buffer access for CyberGraphics operations.
   The simplified approach eliminates complex abstractions while preserving all
   essential functionality needed for modern graphics programming.

    Key features:
    - Off-screen bitmap allocation with hardware acceleration support
    - Direct pixel buffer access for CyberGraphics operations
    - Fast pixel operations for locked surfaces
    - Surface clearing and blitting operations
    - Automatic backend detection and selection
    - Efficient resource management
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

#include "backends/backend_interface.h"
#include "clib/arossupport_protos.h"
#include "clib/graphics_protos.h"
#include "include/zunerenderer.h"
#include "zunerenderer_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************/
/* Forward declarations */
/*****************************************************************************/

void AddDrawingBoardToList(struct IntZuneRendererBase *base,
                           struct DrawingBoard *board);
void RemoveDrawingBoardFromList(struct IntZuneRendererBase *base,
                                struct DrawingBoard *board);
void FastBlitInternal(struct DrawingBoard *src, struct DrawingBoard *dst,
                      WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                      UWORD width, UWORD height);
BOOL ValidateDrawingBoard(struct DrawingBoard *board);

/*****************************************************************************/
/* DrawingBoard Internal Functions */
/*****************************************************************************/

void SetPixelInternal(struct RenderPort *rp, WORD x, WORD y, ULONG color) {
  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZuneBackend *backend = ZuneGetRenderPortBackend(rp);
  if (backend && backend->ops && backend->ops->SetPixel) {
    backend->ops->SetPixel(rp->target_board, x, y, &internal_color);
  } else {
    ZuneFallback_SetPixel(rp->target_board, x, y, &internal_color);
  }
}

ULONG GetPixelInternal(struct RenderPort *rp, WORD x, WORD y) {
  ZuneBackend *backend = ZuneGetRenderPortBackend(rp);
  if (backend && backend->ops && backend->ops->GetPixel) {
    return backend->ops->GetPixel(rp->target_board, x, y);
  }
  return ZuneFallback_GetPixel(rp->target_board, x, y);
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
      if (board->flags & ZUNE_DRAWINGBOARD_LINEARMEM) {
        /* LINEARMEM flag: Force ARGB32 format for direct pixel access.
         * This gives us linear memory that can be locked, but loses colormap
         * inheritance from friend_bitmap. Use this for AA rendering. */
        D(bug("ZuneRenderer: Allocating LINEARMEM surface (ARGB32, no friend)\n"));
        board->bitmap = AllocBitMap(board->width, board->height, board->depth,
                                    BMF_CLEAR | BMF_SPECIALFMT | SHIFT_PIXFMT(PIXFMT_ARGB32),
                                    NULL);
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
  } else {
    /* Standard graphics.library path */
    D(bug("ZuneRenderer: Using graphics.library backend\n"));
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

void CleanupDrawingBoard(struct RenderPort *rp, struct DrawingBoard *board) {
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
    backend = rp ? ZuneGetRenderPortBackend(rp) : ZuneFindBackendByType(BACKEND_OPENGL);
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

APTR LockDrawingBoardPixelsInternal(struct RenderPort *rp, ULONG *pitch) {
  if (!ValidateDrawingBoard(rp->target_board)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard\n"));
    return NULL;
  }

  if (rp->target_board->pixels_locked) {
    D(bug("ZuneRenderer: DrawingBoard already locked\n"));
    return NULL;
  }

  ZuneBackend *backend = ZuneGetRenderPortBackend(rp);
  if (backend && backend->ops && backend->ops->LockPixels) {
    return backend->ops->LockPixels(rp->target_board, pitch);
  }

  return ZuneFallback_LockPixels(rp->target_board, pitch);
}

void UnlockDrawingBoardPixelsInternal(struct RenderPort *rp) {
  if (!rp) {
    D(bug("ZuneRenderer: UnlockDrawingBoardPixels called with NULL RenderPort\n"));
    return;
  }
  if (!ValidateDrawingBoard(rp->target_board)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard\n"));
    return;
  }
  if (!rp->target_board->pixels_locked) {
    D(bug("ZuneRenderer: DrawingBoard already UNlocked\n"));
    return;
  }

  ZuneBackend *backend = ZuneGetRenderPortBackend(rp);
  if (backend && backend->ops && backend->ops->UnlockPixels) {
    backend->ops->UnlockPixels(rp->target_board);
    return;
  }

  ZuneFallback_UnlockPixels(rp->target_board);
}

/*****************************************************************************/
/* Resource Tracking */
/*****************************************************************************/

void AddDrawingBoardToList(struct IntZuneRendererBase *base,
                           struct DrawingBoard *board) {
  if (!base || !board)
    return;

  ObtainSemaphore(&base->lock);
  AddTail((struct List *)&base->drawingboards, (struct Node *)&board->node);
  ReleaseSemaphore(&base->lock);
}

void RemoveDrawingBoardFromList(struct IntZuneRendererBase *base,
                                struct DrawingBoard *board) {
  if (!base || !board)
    return;

  ObtainSemaphore(&base->lock);
  Remove((struct Node *)&board->node);
  ReleaseSemaphore(&base->lock);
}

/*****************************************************************************/
/* Public API Implementation */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH4(struct DrawingBoard *, CreateDrawingBoardForRenderPort,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(UWORD, width, D0),
         AROS_LHA(UWORD, height, D1),
         AROS_LHA(ULONG, flags, D2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 6, zunerenderer)

/*  FUNCTION
    Creates a new DrawingBoard bound to a RenderPort.

    This is the preferred way to create DrawingBoards in the new architecture.
    The DrawingBoard is created with:
    - A BitMap (always, for legacy SetAPen/RectFill compatibility)
    - An FBO (if OpenGL backend, for accelerated rendering)

    The RenderPort's window reference is used for OpenGL context.

    Flags control bitmap allocation:
    - ZUNE_DRAWINGBOARD_LINEARMEM: Force linear memory for direct pixel access.
      This enables LockDrawingBoardPixels() to work, but the bitmap won't
      inherit colormap from the window (legacy pen drawing may not work).
    - Without LINEARMEM: Bitmap inherits colormap from window for legacy
      pen drawing, but may not support direct pixel locking.

INPUTS
    rp - Parent RenderPort (must not be NULL, must have window)
    width - Surface width in pixels
    height - Surface height in pixels
    flags - ZUNE_DRAWINGBOARD_* flags (0 for default behavior)

RESULT
    Pointer to new DrawingBoard, or NULL if creation failed.

NOTES
    Use ZUNE_DRAWINGBOARD_LINEARMEM when you need LockDrawingBoardPixels().
    Use 0 for legacy pen drawing compatibility.
    The DrawingBoard must be freed with DestroyDrawingBoard().
    The RenderPort must remain valid for the lifetime of the DrawingBoard.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
  struct DrawingBoard *board;
  UBYTE depth;

  ENTER_FUNCTION("CreateDrawingBoardForRenderPort");

  D(bug("ZuneRenderer: CreateDrawingBoardForRenderPort(rp=%p, %dx%d, flags=0x%08x)\n",
        rp, width, height, flags));

  /* Validate parameters */
  if (!rp || !rp->valid) {
    D(bug("ZuneRenderer: Invalid RenderPort\n"));
    EXIT_FUNCTION("CreateDrawingBoardForRenderPort");
    return NULL;
  }

  if (width == 0 || height == 0) {
    D(bug("ZuneRenderer: Invalid dimensions\n"));
    EXIT_FUNCTION("CreateDrawingBoardForRenderPort");
    return NULL;
  }

  /* Determine depth from window bitmap */
  if (rp->window && rp->window->RPort && rp->window->RPort->BitMap) {
    depth = GetBitMapAttr(rp->window->RPort->BitMap, BMA_DEPTH);
  } else {
    depth = 32;  /* Default to 32-bit */
  }

  /* Allocate DrawingBoard structure */
  board = AllocVec(sizeof(struct DrawingBoard), MEMF_CLEAR | MEMF_PUBLIC);
  if (!board) {
    D(bug("ZuneRenderer: Failed to allocate DrawingBoard\n"));
    EXIT_FUNCTION("CreateDrawingBoardForRenderPort");
    return NULL;
  }

  /* Set basic properties */
  board->width = width;
  board->height = height;
  board->depth = depth;
  board->flags = flags | ZUNE_DRAWINGBOARD_CACHED;

  /* Store parent window for OpenGL FBO support */
  if (rp->window) {
    board->parent_window = rp->window;
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
    if (rp->window && rp->window->RPort && rp->window->RPort->BitMap) {
      friend_bitmap = rp->window->RPort->BitMap;
    }
  }

  if (!AllocateDrawingBoardBitmap(board, rp->backend_type, friend_bitmap)) {
    D(bug("ZuneRenderer: Failed to allocate DrawingBoard bitmap\n"));
    FreeVec(board);
    EXIT_FUNCTION("CreateDrawingBoardForRenderPort");
    return NULL;
  }

  /* Store colormap from RenderPort for legacy pen-based drawing. */
  if (rp->colormap) {
    board->colormap = rp->colormap;
    D(bug("ZuneRenderer: DrawingBoard colormap set from RenderPort: %p\n", board->colormap));
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

  D(bug("ZuneRenderer: DrawingBoard created for RenderPort, bitmap=%p\n",
        board->bitmap));

  EXIT_FUNCTION("CreateDrawingBoardForRenderPort");
  return board;

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, SyncDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 102, zunerenderer)

/*  FUNCTION
    Synchronizes the backend's render buffer to the DrawingBoard's bitmap.

    For OpenGL backend: Copies FBO contents to the DrawingBoard's bitmap
    using glReadPixels. This is required before using CyberGfx or
    graphics.library functions directly on the DrawingBoard's bitmap
    after OpenGL rendering.

    For CyberGfx backend: No-op since the bitmap IS the render target.

INPUTS
    rp - RenderPort with target DrawingBoard to sync (must not be NULL)

RESULT
    TRUE if sync was successful or not needed, FALSE on error.

NOTES
    Call this function after ZuneRenderer drawing and before direct
    CyberGfx/graphics.library operations on the same DrawingBoard.

    Example mixed-mode rendering:
    1. ZuneSetTarget(rp, board)
    2. ZuneFillRectangle(...) // OpenGL draws to FBO
    3. SyncDrawingBoard(rp) // Copy FBO to bitmap
    4. FillPixelArray(board->rastport, ...) // CyberGfx sees OpenGL content

SEE ALSO
    CreateDrawingBoardForRenderPort(), ZuneSetTarget()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("SyncDrawingBoard");
  D(bug("ZuneRenderer: SyncDrawingBoard(rp=%p)\n", rp));

  if (!rp || !rp->target_board || !rp->target_board->valid) {
    D(bug("ZuneRenderer: SyncDrawingBoard - Invalid RenderPort or DrawingBoard\n"));
    EXIT_FUNCTION("SyncDrawingBoard");
    return FALSE;
  }

  return ZUNE_BACKEND_CALL_NO_ARGS_RET(rp, CopyFromDrawingBoard, FALSE);

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(void, DestroyDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct DrawingBoard *, board, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 11, zunerenderer)

/*  FUNCTION
    Destroys a DrawingBoard and frees all associated resources including
    the bitmap, pixel buffers, and any locked memory.

INPUTS
    rp    - RenderPort associated with the DrawingBoard
    board - DrawingBoard to destroy (may be NULL)

RESULT
    None

NOTES
    After calling this function, the DrawingBoard pointer is no longer valid.
    It is safe to pass NULL board to this function.

SEE ALSO
    CreateDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);

  ENTER_FUNCTION("DestroyDrawingBoard");

  D(bug("ZuneRenderer: DestroyDrawingBoard(rp=%p, board=%p)\n", rp, board));

  if (!board) {
    D(bug("ZuneRenderer: NULL DrawingBoard, nothing to destroy\n"));
    return;
  }

  /* Mark as invalid */
  board->valid = FALSE;

  /* Remove from resource tracking */
  RemoveDrawingBoardFromList(base, board);

  /* Cleanup backend-specific data and free resources */
  CleanupDrawingBoard(rp, board);

  /* Free structure */
  FreeVec(board);

  D(bug("ZuneRenderer: DrawingBoard destroyed\n"));

  EXIT_FUNCTION("DestroyDrawingBoard");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(void, ClearDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 12, zunerenderer)

/*  FUNCTION
    Clears the entire DrawingBoard with the specified color.

INPUTS
    board - DrawingBoard to clear (must not be NULL)
    color - Clear color in ARGB format (0xAARRGGBB)

RESULT
    None

NOTES
    This function uses the most efficient clearing method available
    based on the backend and whether pixels are currently locked.

SEE ALSO
    ClearRenderPort(), FastFillRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ClearDrawingBoard");

  D(bug("ZuneRenderer: ClearDrawingBoard(board=%p, color=0x%08x)\n",
        rp->target_board, color));

  if (!ValidateDrawingBoard(rp->target_board)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard\n"));
    return;
  }

  /*
   * Use ClearRenderPort which is more efficient than DrawRectangle:
   * - OpenGL: Uses glClear() which is hardware-optimized
   * - CyberGfx: Uses optimized fill operations
   */
  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, ClearRenderPort, &internal_color);

  D(bug("ZuneRenderer: DrawingBoard cleared\n"));

  EXIT_FUNCTION("ClearDrawingBoard");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Direct Pixel Access Functions */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH2(APTR, LockDrawingBoardPixels,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(ULONG *, pitch, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 14, zunerenderer)

/*  FUNCTION
    Locks the DrawingBoard for direct pixel buffer access. This is essential
    for CyberGraphics operations and provides maximum performance for pixel
    manipulation.

INPUTS
    board - DrawingBoard to lock (must not be NULL)
    pitch - Pointer to store bytes per scanline (may be NULL)

RESULT
    Pointer to pixel buffer, or NULL if locking failed.

NOTES
    The DrawingBoard must be unlocked with UnlockDrawingBoardPixels().
    Only one lock is allowed per DrawingBoard at a time.
    This function only works with CyberGraphics bitmaps.

SEE ALSO
    UnlockDrawingBoardPixels(), GetPixel(), SetPixel(), FastFillRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("LockDrawingBoardPixels");

  D(bug("ZuneRenderer: LockDrawingBoardPixels(board=%p)\n", rp->target_board));

  return LockDrawingBoardPixelsInternal(rp, pitch);

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, UnlockDrawingBoardPixels,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 15, zunerenderer)

/*  FUNCTION
    Unlocks the DrawingBoard pixel buffer previously locked with
    LockDrawingBoardPixels().

INPUTS
    board - DrawingBoard to unlock (must not be NULL)

RESULT
    None

NOTES
    This function must be called for every successful LockDrawingBoardPixels().
    After unlocking, the pixel buffer pointer is no longer valid.

SEE ALSO
    LockDrawingBoardPixels()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("UnlockDrawingBoardPixels");

  UnlockDrawingBoardPixelsInternal(rp);

  EXIT_FUNCTION("UnlockDrawingBoardPixels");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Direct Pixel Operations */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH2(ULONG, GetPixel,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, point, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 16, zunerenderer)

/*  FUNCTION
    Gets the pixel value at the specified coordinates from a locked
DrawingBoard.

INPUTS
    board - DrawingBoard (must be locked)
    x, y - Pixel coordinates

RESULT
    Pixel value in ARGB format, or 0 if failed.

NOTES
    The DrawingBoard must be locked with LockDrawingBoardPixels().
    Coordinates are not bounds-checked for performance.

SEE ALSO
    SetPixel(), LockDrawingBoardPixels()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rp || !point) {
    D(bug("GetPixel: Invalid parameters (rp=%p, point=%p)\n", rp, point));
    return 0;
  }

  return GetPixelInternal(rp, point->x, point->y);

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, SetPixel,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, point, A1), AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 17, zunerenderer)

/*  FUNCTION
    Sets the pixel value at the specified coordinates in a locked DrawingBoard.

INPUTS
    board - DrawingBoard (must be locked)
    x, y - Pixel coordinates
    color - Pixel color in ARGB format

RESULT
    None

NOTES
    The DrawingBoard must be locked with LockDrawingBoardPixels().
    Coordinates are not bounds-checked for performance.

SEE ALSO
    GetPixel(), LockDrawingBoardPixels(), FastFillRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rp || !point || !rp->target_board || !rp->target_board->pixels_locked)
    return;

  SetPixelInternal(rp, point->x, point->y, color);

  AROS_LIBFUNC_EXIT
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

/*****************************************************************************

    NAME */
AROS_LH4(void, BlitDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct DrawingBoard *, src, A0),
         AROS_LHA(struct DrawingBoard *, dst, A1),
         AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 25, zunerenderer)

/*  FUNCTION
    Blits between two DrawingBoards.

INPUTS
    src - Source DrawingBoard
    dst - Destination DrawingBoard
    src_x, src_y - Source coordinates
    dst_x, dst_y - Destination coordinates
    width, height - Blit dimensions

RESULT
    None

NOTES
    If both DrawingBoards are locked, uses fast pixel operations.
    Otherwise, uses standard bitmap blitting.

SEE ALSO
    BlitDrawingBoardToScreen(), FastBlit()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("BlitDrawingBoard");

  if (!ValidateDrawingBoard(src) || !ValidateDrawingBoard(dst)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard parameters\n"));
    return;
  }

  if (!src_rect || !dest_rect) {
    D(bug("ZuneRenderer: NULL blit rectangles (src_rect=%p, dest_rect=%p)\n",
          src_rect, dest_rect));
    return;
  }

  if (src_rect->width == 0 || src_rect->height == 0 || dest_rect->width == 0 ||
      dest_rect->height == 0) {
    D(bug("ZuneRenderer: Invalid blit rectangle dimensions\n"));
    return;
  }

  if (src_rect->width != dest_rect->width ||
      src_rect->height != dest_rect->height) {
    D(bug("ZuneRenderer: Source/destination rectangle sizes must match (%ux%u "
          "vs %ux%u)\n",
          src_rect->width, src_rect->height, dest_rect->width,
          dest_rect->height));
    return;
  }

  const WORD src_x = src_rect->x;
  const WORD src_y = src_rect->y;
  const WORD dst_x = dest_rect->x;
  const WORD dst_y = dest_rect->y;
  const UWORD width = src_rect->width;
  const UWORD height = src_rect->height;

  BlitDrawingBoardInternal(src, dst, src_x, src_y, dst_x, dst_y, width, height);

  EXIT_FUNCTION("BlitDrawingBoard");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, BlitDrawingBoardToRenderPort,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, src_rp, A0),
         AROS_LHA(struct RenderPort *, dst_rp, A1),
         AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 26, zunerenderer)

/*  FUNCTION
    Blits from one RenderPort's target DrawingBoard to another RenderPort.

INPUTS
    src_rp - Source RenderPort (must have a target DrawingBoard set)
    dst_rp - Destination RenderPort
    src_rect - Source rectangle
    dest_rect - Destination rectangle

RESULT
    None

NOTES
    This function handles blitting to both screen and off-screen RenderPorts.
    The source RenderPort's backend is used to sync its DrawingBoard before
    blitting, ensuring correct behavior when source and destination use
    different backends (e.g., OpenGL source to CyberGfx destination).

SEE ALSO
    BlitDrawingBoard(), BlitDrawingBoardToScreen()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct DrawingBoard *src;

  ENTER_FUNCTION("BlitDrawingBoardToRenderPort");

  if (!ValidateRenderPort(src_rp) || !ValidateRenderPort(dst_rp)) {
    D(bug("ZuneRenderer: Invalid RenderPort parameters\n"));
    return;
  }

  src = src_rp->target_board;
  if (!src) {
    D(bug("ZuneRenderer: Source RenderPort has no valid target DrawingBoard\n"));
    return;
  }

  if (!ValidateDrawingBoard(src)) {
    D(bug("ZuneRenderer: Source RenderPort has no valid target DrawingBoard\n"));
    return;
  }

  if (!src_rect || !dest_rect) {
    D(bug("ZuneRenderer: NULL blit rectangles (src_rect=%p, dest_rect=%p)\n",
          src_rect, dest_rect));
    return;
  }

  if (src_rect->width == 0 || src_rect->height == 0 || dest_rect->width == 0 ||
      dest_rect->height == 0) {
    D(bug("ZuneRenderer: Invalid blit rectangle dimensions\n"));
    return;
  }

  if (src_rect->width != dest_rect->width ||
      src_rect->height != dest_rect->height) {
    D(bug("ZuneRenderer: Source/destination rectangle sizes must match (%ux%u "
          "vs %ux%u)\n",
          src_rect->width, src_rect->height, dest_rect->width,
          dest_rect->height));
    return;
  }

  const WORD src_x = src_rect->x;
  const WORD src_y = src_rect->y;
  const WORD dst_x = dest_rect->x;
  const WORD dst_y = dest_rect->y;
  const UWORD width = src_rect->width;
  const UWORD height = src_rect->height;

  D(bug("BlitDrawingBoardToRenderPort: dst_rp->target_board=%p, dst_rp->target_rp=%p\n",
        dst_rp->target_board, dst_rp->target_rp));

  /*
   * CRITICAL: Sync backend buffer to the DrawingBoard's bitmap BEFORE blitting.
   * For OpenGL backend, this copies FBO content to the bitmap using glReadPixels.
   * For CyberGfx backend, this is a no-op since the bitmap IS the render target.
   * This MUST happen regardless of whether destination is DrawingBoard or RastPort,
   * because both paths use the source bitmap for the actual blit operation.
   */
  if (src->valid) {
    ZUNE_BACKEND_CALL_NO_ARGS_RET(src_rp, CopyFromDrawingBoard, FALSE);
  }

  if (dst_rp->target_board) {
    /* Blit to off-screen DrawingBoard */
    D(bug("BlitDrawingBoardToRenderPort: Blitting to off-screen DrawingBoard\n"));
    BlitDrawingBoardInternal(src, dst_rp->target_board, src_x, src_y, dst_x, dst_y,
                             width, height);
  } else {
    /* Blit to screen RastPort */
    struct BitMap *bitmap;
    UWORD blit_width = width;
    UWORD blit_height = height;

    /* Validate destination RastPort */
    if (!dst_rp->target_rp || !dst_rp->target_rp->BitMap) {
      D(bug("BlitDrawingBoardToRenderPort: Invalid destination RastPort\n"));
      EXIT_FUNCTION("BlitDrawingBoardToRenderPort");
      return;
    }

    D(bug("BlitDrawingBoardToRenderPort: Blitting to screen RastPort %p\n", dst_rp->target_rp));

    /* Bounds checking */
    if (src_x + blit_width > src->width)
      blit_width = src->width - src_x;
    if (src_y + blit_height > src->height)
      blit_height = src->height - src_y;

    /* Use standard bitmap path for all backends */
    bitmap = src->rastport ? src->rastport->BitMap : src->bitmap;

    if (bitmap) {
      BltBitMapRastPort(bitmap, src_x, src_y, dst_rp->target_rp, dst_x, dst_y,
                        blit_width, blit_height, 0xC0);
    }
  }

  EXIT_FUNCTION("BlitDrawingBoardToRenderPort");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH8(void, ZuneCopyFromRastPort,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct RastPort *, src_rp, A1),
         AROS_LHA(WORD, src_x, D0),
         AROS_LHA(WORD, src_y, D1),
         AROS_LHA(WORD, dst_x, D2),
         AROS_LHA(WORD, dst_y, D3),
         AROS_LHA(UWORD, width, D4),
         AROS_LHA(UWORD, height, D5),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 98, zunerenderer)

/*  FUNCTION
    Copies pixels from a RastPort into a DrawingBoard's buffer.
    This is used to read background content for proper alpha blending
    when drawing antialiased graphics over existing content.

INPUTS
    rp - Destination RenderPort (must target a DrawingBoard)
    src_rp - Source RastPort to read pixels from
    src_x, src_y - Source coordinates in the RastPort
    dst_x, dst_y - Destination coordinates in the DrawingBoard
    width, height - Size of area to copy

RESULT
    None

NOTES
    The RenderPort must target a DrawingBoard. For CyberGfx backend,
    this copies directly to the DrawingBoard's pixel buffer. For OpenGL
    backend, this uploads the pixels as a texture and draws to the
    framebuffer.

SEE ALSO
    CreateDrawingBoard(), CreateRenderPortWithDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ZuneBackend *backend;

  ENTER_FUNCTION("ZuneCopyFromRastPort");

  D(bug("ZuneRenderer: ZuneCopyFromRastPort(rp=%p, src_rp=%p, src=%d,%d dst=%d,%d %dx%d)\n",
        rp, src_rp, src_x, src_y, dst_x, dst_y, width, height));

  if (!rp || !src_rp) {
    D(bug("ZuneRenderer: Invalid parameters for ZuneCopyFromRastPort\n"));
    EXIT_FUNCTION("ZuneCopyFromRastPort");
    return;
  }

  if (!rp->target_board) {
    D(bug("ZuneRenderer: RenderPort does not target a DrawingBoard\n"));
    EXIT_FUNCTION("ZuneCopyFromRastPort");
    return;
  }

  if (width == 0 || height == 0) {
    EXIT_FUNCTION("ZuneCopyFromRastPort");
    return;
  }

  /* Get the backend and call its CopyFromRastPort function */
  backend = ZuneGetRenderPortBackend(rp);
  if (backend && backend->ops && backend->ops->CopyFromRastPort) {
    backend->ops->CopyFromRastPort(rp, src_rp, src_x, src_y, dst_x, dst_y, width, height);
  } else {
    D(bug("ZuneRenderer: Backend does not support CopyFromRastPort\n"));
  }

  EXIT_FUNCTION("ZuneCopyFromRastPort");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH7(void, ZuneBlitToWindow,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(WORD, src_x, D0),
         AROS_LHA(WORD, src_y, D1),
         AROS_LHA(WORD, dst_x, D2),
         AROS_LHA(WORD, dst_y, D3),
         AROS_LHA(UWORD, width, D4),
         AROS_LHA(UWORD, height, D5),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 100, zunerenderer)

/*  FUNCTION
    Blits content from the RenderPort's DrawingBoard directly to its
    associated window. This eliminates the need for a separate
    WindowRenderPort for blitting operations.

INPUTS
    rp - RenderPort with a DrawingBoard target and window reference
    src_x, src_y - Source coordinates in the DrawingBoard
    dst_x, dst_y - Destination coordinates in the window
    width, height - Size of area to blit

RESULT
    None

NOTES
    The RenderPort must have both a valid window reference and a
    DrawingBoard target. Uses BltBitMapRastPort for the actual blit.

SEE ALSO
    CreateDrawingBoardForRenderPort(), ZuneSetTarget()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneBlitToWindow");

  D(bug("ZuneRenderer: ZuneBlitToWindow(rp=%p, src=%d,%d dst=%d,%d %dx%d)\n",
        rp, src_x, src_y, dst_x, dst_y, width, height));

  if (!rp) {
    D(bug("ZuneRenderer: Invalid RenderPort for ZuneBlitToWindow\n"));
    EXIT_FUNCTION("ZuneBlitToWindow");
    return;
  }

  if (!rp->window) {
    D(bug("ZuneRenderer: RenderPort has no window reference\n"));
    EXIT_FUNCTION("ZuneBlitToWindow");
    return;
  }

  if (!rp->target_board || !rp->target_board->bitmap || !rp->target_board->valid) {
    D(bug("ZuneRenderer: RenderPort has no DrawingBoard, bitmap, or board invalid\n"));
    EXIT_FUNCTION("ZuneBlitToWindow");
    return;
  }

  /* Validate window's RastPort before using it */
  if (!rp->window->RPort || !rp->window->RPort->BitMap) {
    D(bug("ZuneRenderer: Window has no valid RastPort or BitMap\n"));
    EXIT_FUNCTION("ZuneBlitToWindow");
    return;
  }

  if (width == 0 || height == 0) {
    EXIT_FUNCTION("ZuneBlitToWindow");
    return;
  }

  /*
   * Flush the backend and sync FBO to bitmap before blitting.
   * For OpenGL backend, this copies FBO content to the bitmap using glReadPixels.
   * For CyberGfx backend, FlushBatch ensures pending operations complete,
   * and CopyFromDrawingBoard is a no-op since the bitmap IS the render target.
   */
  {
    ZuneBackend *backend = ZuneGetRenderPortBackend(rp);
    if (backend && backend->ops) {
      if (backend->ops->FlushBatch) {
        backend->ops->FlushBatch(rp);
      }
      if (backend->ops->CopyFromDrawingBoard) {
        backend->ops->CopyFromDrawingBoard(rp);
      }
    }
  }

  /* Blit from DrawingBoard bitmap to window's RastPort */
  BltBitMapRastPort(rp->target_board->bitmap, src_x, src_y,
                    rp->window->RPort, dst_x, dst_y,
                    width, height, 0xC0);

  EXIT_FUNCTION("ZuneBlitToWindow");

  AROS_LIBFUNC_EXIT
}
