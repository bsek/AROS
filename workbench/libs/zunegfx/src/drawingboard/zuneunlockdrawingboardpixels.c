/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneUnlockDrawingBoardPixels
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
AROS_LH1(void, ZuneUnlockDrawingBoardPixels,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 15, zunegfx)

/*  FUNCTION
    Unlocks the DrawingBoard pixel buffer previously locked with
    ZuneLockDrawingBoardPixels().

INPUTS
    board - DrawingBoard to unlock (must not be NULL)

RESULT
    None

NOTES
    This function must be called for every successful ZuneLockDrawingBoardPixels().
    After unlocking, the pixel buffer pointer is no longer valid.

SEE ALSO
    ZuneLockDrawingBoardPixels()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneUnlockDrawingBoardPixels");

  UnlockDrawingBoardPixelsInternal(rctx);

  EXIT_FUNCTION("ZuneUnlockDrawingBoardPixels");

  AROS_LIBFUNC_EXIT
}
