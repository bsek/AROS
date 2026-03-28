/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneCapture
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH8(void, ZuneCapture,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct RastPort *, src_rp, A1),
         AROS_LHA(WORD, src_x, D0),
         AROS_LHA(WORD, src_y, D1),
         AROS_LHA(WORD, dst_x, D2),
         AROS_LHA(WORD, dst_y, D3),
         AROS_LHA(UWORD, width, D4),
         AROS_LHA(UWORD, height, D5),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 98, zunegfx)

/*  FUNCTION
    Captures pixels from a RastPort into the DrawingBoard.
    This is used to read background content for proper alpha blending
    when drawing antialiased graphics over existing content.

INPUTS
    rctx - RenderContext with a DrawingBoard target
    src_rp - Source RastPort to read pixels from (e.g., window or double buffer)
    src_x, src_y - Source coordinates in the RastPort
    dst_x, dst_y - Destination coordinates in the DrawingBoard
    width, height - Size of area to capture

RESULT
    None

NOTES
    The RenderContext must have a valid target_board.
    For OpenGL backend, this uploads the captured pixels to the FBO.
    For CyberGfx backend, this copies directly to the DrawingBoard's bitmap.

SEE ALSO
    ZuneBlit(), ZunePresent(), ZuneCreateDrawingBoardForRenderContext()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ZuneBackend *backend;

  ENTER_FUNCTION("ZuneCapture");

  D(bug("ZuneGfx: ZuneCapture(rctx=%p, src_rp=%p, src=%d,%d dst=%d,%d %dx%d)\n",
        rctx, src_rp, src_x, src_y, dst_x, dst_y, width, height));

  if (!rctx || !src_rp) {
    D(bug("ZuneGfx: Invalid RenderContext or source RastPort for ZuneCapture\n"));
    EXIT_FUNCTION("ZuneCapture");
    return;
  }

  if (!rctx->target_board || !rctx->target_board->valid) {
    D(bug("ZuneGfx: RenderContext has no valid DrawingBoard\n"));
    EXIT_FUNCTION("ZuneCapture");
    return;
  }

  if (width == 0 || height == 0) {
    EXIT_FUNCTION("ZuneCapture");
    return;
  }

  /* Use backend's CopyFromRastPort to capture content from source RastPort */
  backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->CopyFromRastPort) {
    backend->ops->CopyFromRastPort(rctx, src_rp, src_x, src_y,
                                   dst_x, dst_y, width, height);
  } else {
    D(bug("ZuneGfx: Backend does not support CopyFromRastPort\n"));
  }

  EXIT_FUNCTION("ZuneCapture");

  AROS_LIBFUNC_EXIT
}
