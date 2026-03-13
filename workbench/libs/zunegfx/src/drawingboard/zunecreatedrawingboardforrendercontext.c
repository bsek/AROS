/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateDrawingBoardForRenderContext
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

/*****************************************************************************

    NAME */
AROS_LH4(struct DrawingBoard *, ZuneCreateDrawingBoardForRenderContext,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(UWORD, width, D0),
         AROS_LHA(UWORD, height, D1),
         AROS_LHA(ULONG, flags, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 6, zunegfx)

/*  FUNCTION
    Creates a new DrawingBoard bound to a RenderContext.

    This is the preferred way to create DrawingBoards in the new architecture.
    The DrawingBoard is created with:
    - A BitMap (always, for legacy SetAPen/RectFill compatibility)
    - An FBO (if OpenGL backend, for accelerated rendering)

    The RenderContext's window reference is used for OpenGL context.

    Flags control bitmap allocation:
    - ZUNE_DRAWINGBOARD_LINEARMEM: Force linear memory for direct pixel access.
      This enables ZuneLockDrawingBoardPixels() to work, but the bitmap won't
      inherit colormap from the window (legacy pen drawing may not work).
    - Without LINEARMEM: Bitmap inherits colormap from window for legacy
      pen drawing, but may not support direct pixel locking.

INPUTS
    rctx - Parent RenderContext (must not be NULL, must have window)
    width - Surface width in pixels
    height - Surface height in pixels
    flags - ZUNE_DRAWINGBOARD_* flags (0 for default behavior)

RESULT
    Pointer to new DrawingBoard, or NULL if creation failed.

NOTES
    Use ZUNE_DRAWINGBOARD_LINEARMEM when you need ZuneLockDrawingBoardPixels().
    Use 0 for legacy pen drawing compatibility.
    The DrawingBoard must be freed with ZuneDestroyDrawingBoard().
    The RenderContext must remain valid for the lifetime of the DrawingBoard.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
  return CreateDrawingBoardForRenderContextInternal(base, rctx, width, height, flags);

  AROS_LIBFUNC_EXIT
}
