/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneSync
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ZuneSync,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 100, zunegfx)

/*  FUNCTION
    Synchronizes the backend's render buffer to the DrawingBoard's bitmap.

    For OpenGL backend: Copies FBO contents to the DrawingBoard's bitmap
    using glReadPixels. This is required before using CyberGfx or
    graphics.library functions directly on the DrawingBoard's bitmap
    after OpenGL rendering.

    For CyberGfx backend: No-op since the bitmap IS the render target.

INPUTS
    rctx - RenderContext with target DrawingBoard to sync (must not be NULL)

RESULT
    TRUE if sync was successful or not needed, FALSE on error.

NOTES
    Call this function after ZuneRenderer drawing and before direct
    CyberGfx/graphics.library operations on the same DrawingBoard.

    Example mixed-mode rendering:
    1. ZuneSetTarget(rctx, board)
    2. ZuneFillRectangle(...) // OpenGL draws to FBO
    3. ZuneSync(rctx)           // Copy FBO to bitmap
    4. FillPixelArray(board->rastport, ...) // CyberGfx sees OpenGL content

SEE ALSO
    ZuneCreateDrawingBoardForRenderContext(), ZuneSetTarget(), ZunePresent()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneSync");
  D(bug("ZuneRenderer: ZuneSync(rctx=%p)\n", rctx));

  if (!rctx || !rctx->target_board || !rctx->target_board->valid) {
    D(bug("ZuneRenderer: ZuneSync - Invalid RenderContext or DrawingBoard\n"));
    EXIT_FUNCTION("ZuneSync");
    return FALSE;
  }

  return ZUNE_BACKEND_CALL_NO_ARGS_RET(rctx, CopyFromDrawingBoard, FALSE);

  AROS_LIBFUNC_EXIT
}
