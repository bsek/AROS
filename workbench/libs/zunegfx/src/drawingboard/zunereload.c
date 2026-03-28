/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneReload
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ZuneReload,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 102, zunegfx)

/*  FUNCTION
    Reloads the backend's render buffer from the DrawingBoard's bitmap.

    For OpenGL backend: Uploads bitmap contents to the FBO using
    glTexSubImage2D. This is required after using CyberGfx or
    graphics.library functions directly on the DrawingBoard's bitmap
    before continuing with OpenGL rendering.

    For CyberGfx backend: No-op since the bitmap IS the render target.

INPUTS
    rctx - RenderContext with target DrawingBoard to reload (must not be NULL)

RESULT
    TRUE if reload was successful or not needed, FALSE on error.

NOTES
    Call this function after direct CyberGfx/graphics.library operations
    on the DrawingBoard's bitmap and before ZuneGfx drawing.

    This is the inverse of ZuneSync().

    Example mixed-mode rendering:
    1. ZuneSetTarget(rctx, board)
    2. FillPixelArray(board->rastport, ...) // CyberGfx draws to bitmap
    3. ZuneReload(rctx)                       // Upload bitmap to FBO
    4. ZuneFillRectangle(...)               // OpenGL sees bitmap content

SEE ALSO
    ZuneSync(), ZuneCreateDrawingBoardForRenderContext(), ZuneSetTarget()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ZuneBackend *backend;
  struct DrawingBoard *board;

  ENTER_FUNCTION("ZuneReload");
  D(bug("ZuneGfx: ZuneReload(rctx=%p)\n", rctx));

  if (!rctx || !rctx->target_board || !rctx->target_board->valid) {
    D(bug("ZuneGfx: ZuneReload - Invalid RenderContext or DrawingBoard\n"));
    EXIT_FUNCTION("ZuneReload");
    return FALSE;
  }

  board = rctx->target_board;
  if (!board->rastport) {
    D(bug("ZuneGfx: ZuneReload - DrawingBoard has no rastport\n"));
    EXIT_FUNCTION("ZuneReload");
    return FALSE;
  }

  /* Use CopyFromRastPort to upload the DrawingBoard's bitmap to the FBO */
  backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->CopyFromRastPort) {
    backend->ops->CopyFromRastPort(rctx, board->rastport,
                                   0, 0, 0, 0, board->width, board->height);
    EXIT_FUNCTION("ZuneReload");
    return TRUE;
  }

  EXIT_FUNCTION("ZuneReload");
  return FALSE;

  AROS_LIBFUNC_EXIT
}
