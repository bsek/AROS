/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneLockDrawingBoardPixels
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(APTR, ZuneLockDrawingBoardPixels,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0), AROS_LHA(ULONG *, pitch, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 14, zunegfx)

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
    The DrawingBoard must be unlocked with ZuneUnlockDrawingBoardPixels().
    Only one lock is allowed per DrawingBoard at a time.
    This function only works with CyberGraphics bitmaps.

SEE ALSO
    ZuneUnlockDrawingBoardPixels(), ZuneGetPixel(), ZuneSetPixel(), FastFillRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneLockDrawingBoardPixels");

  D(bug("ZuneRenderer: ZuneLockDrawingBoardPixels(board=%p)\n", rctx->target_board));

  return LockDrawingBoardPixelsInternal(rctx, pitch);

  AROS_LIBFUNC_EXIT
}
