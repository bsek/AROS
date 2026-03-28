/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZunePresent
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH7(void, ZunePresent,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(WORD, src_x, D0),
         AROS_LHA(WORD, src_y, D1),
         AROS_LHA(WORD, dst_x, D2),
         AROS_LHA(WORD, dst_y, D3),
         AROS_LHA(UWORD, width, D4),
         AROS_LHA(UWORD, height, D5),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 26, zunegfx)

/*  FUNCTION
    Presents (flushes) DrawingBoard content to the associated window.
    This is the primary function for double-buffered rendering - call it
    to display what has been rendered to the DrawingBoard.

    Automatically syncs OpenGL FBO to bitmap before blitting to window.

INPUTS
    rctx - RenderContext with a DrawingBoard and associated window
    src_x, src_y - Source coordinates in the DrawingBoard
    dst_x, dst_y - Destination coordinates in the window
    width, height - Size of area to present

RESULT
    None

NOTES
    The RenderContext must have both a valid target_board and window reference.

SEE ALSO
    ZuneBlit(), ZuneCapture(), ZuneCreateDrawingBoardForRenderContext()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZunePresent");

  D(bug("ZuneGfx: ZunePresent(rctx=%p, src=%d,%d dst=%d,%d %dx%d)\n",
        rctx, src_x, src_y, dst_x, dst_y, width, height));

  if (!rctx) {
    D(bug("ZuneGfx: Invalid RenderContext for ZunePresent\n"));
    EXIT_FUNCTION("ZunePresent");
    return;
  }

  if (!rctx->window || !rctx->window->RPort || !rctx->window->RPort->BitMap) {
    D(bug("ZuneGfx: RenderContext has no valid window\n"));
    EXIT_FUNCTION("ZunePresent");
    return;
  }

  if (!rctx->target_board || !rctx->target_board->bitmap || !rctx->target_board->valid) {
    D(bug("ZuneGfx: RenderContext has no valid DrawingBoard\n"));
    EXIT_FUNCTION("ZunePresent");
    return;
  }

  if (width == 0 || height == 0) {
    EXIT_FUNCTION("ZunePresent");
    return;
  }

  /* Sync FBO region to bitmap before presenting */
  {
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
    D(bug("ZuneGfx: ZunePresent - backend=%p\n", backend));
    if (backend && backend->ops) {
      D(bug("ZuneGfx: ZunePresent - backend->ops=%p, name=%s\n",
            backend->ops, backend->ops->name ? (const char *)backend->ops->name : "NULL"));
      if (backend->ops->FlushBatch) {
        D(bug("ZuneGfx: ZunePresent - calling ZuneFlushBatch\n"));
        backend->ops->FlushBatch(rctx);
      }
      if (backend->ops->CopyRegionFromDrawingBoard) {
        D(bug("ZuneGfx: ZunePresent - calling CopyRegionFromDrawingBoard\n"));
        backend->ops->CopyRegionFromDrawingBoard(rctx, src_x, src_y, width, height);
      } else if (backend->ops->CopyFromDrawingBoard) {
        D(bug("ZuneGfx: ZunePresent - calling CopyFromDrawingBoard\n"));
        backend->ops->CopyFromDrawingBoard(rctx);
      } else {
        D(bug("ZuneGfx: ZunePresent - no sync function available!\n"));
      }
    } else {
      D(bug("ZuneGfx: ZunePresent - no backend available!\n"));
    }
  }

  /* Blit from DrawingBoard bitmap to window's RastPort */
  BltBitMapRastPort(rctx->target_board->bitmap, src_x, src_y,
                    rctx->window->RPort, dst_x, dst_y,
                    width, height, 0xC0);

  EXIT_FUNCTION("ZunePresent");

  AROS_LIBFUNC_EXIT
}
