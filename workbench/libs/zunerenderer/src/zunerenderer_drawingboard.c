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

#define DEBUG 0
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
                                ZuneBackendType backend_type) {
  if (!board)
    return FALSE;

  ENTER_FUNCTION("AllocateDrawingBoardBitmap");

  D(bug("ZuneRenderer: Allocating bitmap %dx%dx%d, flags=0x%08x\n",
        board->width, board->height, board->depth, board->flags));

  /* Detect CyberGraphics availability */
  board->hardware_surface = FALSE;

  if (backend_type == BACKEND_CYBERGFX) {
    /* CyberGraphics path */
    D(bug("ZuneRenderer: Using CyberGraphics backend\n"));

    /* Try hardware surface first if requested */
    if (board->flags & ZUNE_DRAWINGBOARD_HARDWARE) {
      D(bug("ZuneRenderer: Attempting hardware surface allocation\n"));
      board->bitmap = AllocBitMap(board->width, board->height, board->depth,
                                  BMF_DISPLAYABLE | BMF_CLEAR, NULL);
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
      D(bug("ZuneRenderer: Allocating software surface\n"));
      board->bitmap = AllocBitMap(board->width, board->height, board->depth,
                                  BMF_CLEAR, NULL);
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
        AllocBitMap(board->width, board->height, board->depth, BMF_CLEAR, NULL);
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

void CleanupDrawingBoard(struct DrawingBoard *board) {
  if (!board)
    return;

  ENTER_FUNCTION("CleanupDrawingBoard");

  /* Mark as invalid */
  board->valid = FALSE;

  /* Free bitmap and associated resources */
  /* Note: FreeDrawingBoardBitmap() already frees the rastport */
  FreeDrawingBoardBitmap(board);

  /* Clear all fields */
  board->width = 0;
  board->height = 0;
  board->depth = 0;
  board->flags = 0;
  board->colormap = NULL;

  EXIT_FUNCTION("DestroyDrawingBoard");
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
/* Internal DrawingBoard Creation Functions */
/*****************************************************************************/

void DestroyDrawingBoardInternal(struct IntZuneRendererBase *base,
                                 struct DrawingBoard *board) {
  if (!board)
    return;

  ENTER_FUNCTION("DestroyDrawingBoardInternal");

  /* Mark as invalid to prevent further use */
  board->valid = FALSE;

  /* Remove from resource tracking */
  RemoveDrawingBoardFromList(base, board);

  /* Cleanup DrawingBoard */
  CleanupDrawingBoard(board);

  /* Free structure */
  FreeVec(board);

  D(bug("ZuneRenderer: DrawingBoard destroyed internally\n"));
  EXIT_FUNCTION("DestroyDrawingBoardInternal");
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
AROS_LH4(struct DrawingBoard *, CreateDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(UWORD, width, D0), AROS_LHA(UWORD, height, D1),
         AROS_LHA(UBYTE, depth, D2), AROS_LHA(ULONG, flags, D3),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 10, zunerenderer)

/*  FUNCTION
    Creates a new off-screen rendering surface (DrawingBoard) with the
    specified dimensions and properties. The DrawingBoard can be used
    for off-screen rendering, double-buffering, and direct pixel access.

INPUTS
    width - Surface width in pixels (must be > 0)
    height - Surface height in pixels (must be > 0)
    depth - Color depth in bits per pixel (16, 24, or 32)
    flags - Creation flags (ZUNE_DRAWINGBOARD_*)

RESULT
    Pointer to new DrawingBoard structure, or NULL if creation failed.

NOTES
    The created DrawingBoard must be freed with DestroyDrawingBoard().
    Hardware surfaces are allocated when possible if HARDWARE flag is set.
    Direct pixel access is available through LockDrawingBoardPixels().

SEE ALSO
    DestroyDrawingBoard(), LockDrawingBoardPixels(),
CreateRenderPortWithDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
  struct DrawingBoard *board;

  ENTER_FUNCTION("CreateDrawingBoard");

  D(bug("ZuneRenderer: CreateDrawingBoard(%d, %d, %d, 0x%08x)\n", width, height,
        depth, flags));

  /* Validate parameters */
  if (width == 0 || height == 0) {
    D(bug("ZuneRenderer: Invalid dimensions\n"));
    return NULL;
  }

  if (depth != 16 && depth != 24 && depth != 32) {
    D(bug("ZuneRenderer: Unsupported color depth: %d\n", depth));
    return NULL;
  }

  /* Allocate DrawingBoard structure */
  board = AllocVec(sizeof(struct DrawingBoard), MEMF_CLEAR | MEMF_PUBLIC);
  if (!board) {
    D(bug("ZuneRenderer: Failed to allocate DrawingBoard structure\n"));
    return NULL;
  }

  /* Set basic properties */
  board->width = width;
  board->height = height;
  board->depth = depth;
  board->flags = flags;

  /* Initialize DrawingBoard */
  InitDrawingBoard(board);

  /* Add to tracking list */
  AddDrawingBoardToList(base, board);

  /* Allocate bitmap for the drawing board using best available backend */
  ZuneBackend *backend = ZuneFindBestBackend(NULL);
  ZuneBackendType backend_type =
      backend ? backend->ops->type : BACKEND_SOFTWARE;

  if (!AllocateDrawingBoardBitmap(board, backend_type)) {
    D(bug("ZuneRenderer: Failed to allocate drawing board bitmap\n"));
    RemoveDrawingBoardFromList(base, board);
    FreeVec(board);
    EXIT_FUNCTION("CreateDrawingBoard");
    return NULL;
  }

  D(bug("ZuneRenderer: DrawingBoard created successfully (%s)\n",
        board->hardware_surface ? "hardware" : "software"));

  EXIT_FUNCTION("CreateDrawingBoard");
  return board;

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, DestroyDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct DrawingBoard *, board, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 11, zunerenderer)

/*  FUNCTION
    Destroys a DrawingBoard and frees all associated resources including
    the bitmap, pixel buffers, and any locked memory.

INPUTS
    board - DrawingBoard to destroy (may be NULL)

RESULT
    None

NOTES
    After calling this function, the DrawingBoard pointer is no longer valid.
    Any RenderPorts using this DrawingBoard must be destroyed first.
    It is safe to pass NULL to this function.

SEE ALSO
    CreateDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);

  ENTER_FUNCTION("DestroyDrawingBoard");

  D(bug("ZuneRenderer: DestroyDrawingBoard(board=%p)\n", board));

  if (!board) {
    D(bug("ZuneRenderer: NULL DrawingBoard, nothing to destroy\n"));
    return;
  }

  /* Use internal destroy function which handles all cleanup including resource
   * tracking */
  DestroyDrawingBoardInternal(base, board);

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

  struct DrawingBoard *board = rp->target_board;
  struct ZuneBrush *fill_brush = ZUNE_BRUSH_SOLID(color);

  ZUNE_BACKEND_CALL(rp, DrawRectangle, 0, 0, board->width, board->height, 1.0,
                    0.0, fill_brush, NULL, TRUE, FALSE);

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

  if (!board->bitmap) {
    return FALSE;
  }

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

void BlitDrawingBoardToScreenInternal(struct DrawingBoard *src,
                                      struct RastPort *dst, WORD src_x,
                                      WORD src_y, WORD dst_x, WORD dst_y,
                                      UWORD width, UWORD height) {
  D(bug(
      "ZuneRenderer: Blitting DrawingBoard to screen (%d,%d)->(%d,%d) %dx%d\n",
      src_x, src_y, dst_x, dst_y, width, height));

  /* Bounds checking */
  if (src_x + width > src->width)
    width = src->width - src_x;
  if (src_y + height > src->height)
    height = src->height - src_y;

  struct BitMap *bitmap =
      src->rastport ? src->rastport->BitMap : src->bitmap;
  if (!bitmap) {
    D(bug("ZuneRenderer: No bitmap available for blit\n"));
    return;
  }

  /* Use BltBitMapRastPort for standard blitting */
  BltBitMapRastPort(bitmap, src_x, src_y, dst, dst_x, dst_y, width, height,
                    0xC0); /* Simple copy */
}

void BlitDrawingBoardInternal(struct DrawingBoard *src,
                              struct DrawingBoard *dst, WORD src_x, WORD src_y,
                              WORD dst_x, WORD dst_y, UWORD width,
                              UWORD height) {

  D(bug("ZuneRenderer: Blitting between DrawingBoards (%d,%d)->(%d,%d) %dx%d\n",
        src_x, src_y, dst_x, dst_y, width, height));

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
         AROS_LHA(struct DrawingBoard *, src, A0),
         AROS_LHA(struct RenderPort *, dst, A1),
         AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 26, zunerenderer)

/*  FUNCTION
    Blits a DrawingBoard to a RenderPort.

INPUTS
    src - Source DrawingBoard
    dst - Destination RenderPort
    src_x, src_y - Source coordinates
    dst_x, dst_y - Destination coordinates
    width, height - Blit dimensions

RESULT
    None

NOTES
    This function handles blitting to both screen and off-screen RenderPorts.

SEE ALSO
    BlitDrawingBoard(), BlitDrawingBoardToScreen()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("BlitDrawingBoardToRenderPort");

  if (!ValidateDrawingBoard(src) || !ValidateRenderPort(dst)) {
    D(bug("ZuneRenderer: Invalid parameters\n"));
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

  if (dst->target_board) {
    /* Blit to off-screen DrawingBoard */
    BlitDrawingBoardInternal(src, dst->target_board, src_x, src_y, dst_x, dst_y,
                             width, height);
  } else {
    /* Blit to screen RastPort */
    BlitDrawingBoardToScreenInternal(src, dst->target_rp, src_x, src_y, dst_x,
                                     dst_y, width, height);
  }

  EXIT_FUNCTION("BlitDrawingBoardToRenderPort");

  AROS_LIBFUNC_EXIT
}
