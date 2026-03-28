/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneGetPixel
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(ULONG, ZuneGetPixel,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, point, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 16, zunegfx)

/*  FUNCTION
    Gets the pixel value at the specified coordinates from a locked
DrawingBoard.

INPUTS
    board - DrawingBoard (must be locked)
    x, y - Pixel coordinates

RESULT
    Pixel value in ARGB format, or 0 if failed.

NOTES
    The DrawingBoard must be locked with ZuneLockDrawingBoardPixels().
    Coordinates are not bounds-checked for performance.

SEE ALSO
    ZuneSetPixel(), ZuneLockDrawingBoardPixels()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !point) {
    D(bug("ZuneGetPixel: Invalid parameters (rctx=%p, point=%p)\n", rctx, point));
    return 0;
  }

  return GetPixelInternal(rctx, point->x, point->y);

  AROS_LIBFUNC_EXIT
}
