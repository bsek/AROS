/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneSetPixel
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneSetPixel,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, point, A1), AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 17, zunegfx)

/*  FUNCTION
    Sets the pixel value at the specified coordinates in a locked DrawingBoard.

INPUTS
    board - DrawingBoard (must be locked)
    x, y - Pixel coordinates
    color - Pixel color in ARGB format

RESULT
    None

NOTES
    The DrawingBoard must be locked with ZuneLockDrawingBoardPixels().
    Coordinates are not bounds-checked for performance.

SEE ALSO
    ZuneGetPixel(), ZuneLockDrawingBoardPixels(), FastFillRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !point || !rctx->target_board || !rctx->target_board->pixels_locked)
    return;

  SetPixelInternal(rctx, point->x, point->y, color);

  AROS_LIBFUNC_EXIT
}
