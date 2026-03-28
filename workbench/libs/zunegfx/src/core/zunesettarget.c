/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneSetTarget
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <hidd/gfx.h>

#include "../../include/zunegfx.h"
#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(BOOL, ZuneSetTarget,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct DrawingBoard *, board, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 10, zunegfx)

/*  FUNCTION
    Switch the render target of a RenderContext.

    board = NULL: Render to the window's RastPort
    board != NULL: Render to the DrawingBoard

INPUTS
    rctx - RenderContext to modify
    board - Target DrawingBoard, or NULL for window

RESULT
    TRUE if target was switched successfully, FALSE otherwise.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneSetTarget");

  if (!rctx || !rctx->valid) {
    D(bug("ZuneGfx: ZuneSetTarget - invalid RenderContext\n"));
    EXIT_FUNCTION("ZuneSetTarget");
    return FALSE;
  }

  if (board && !board->valid) {
    D(bug("ZuneGfx: ZuneSetTarget - invalid DrawingBoard\n"));
    EXIT_FUNCTION("ZuneSetTarget");
    return FALSE;
  }

  D(bug("ZuneGfx: ZuneSetTarget(rctx=%p, board=%p)\n", rctx, board));

  /* Update target */
  rctx->target_board = board;

  if (board) {
    /* Switching to DrawingBoard */
    rctx->target_rastport = board->rastport;
    if (board->bitmap) {
      rctx->hidd_bitmap_obj = HIDD_BM_OBJ(board->bitmap);
    }
    D(bug("ZuneGfx: Target set to DrawingBoard %p (%dx%d)\n",
          board, board->width, board->height));
  } else {
    /* Switching to window */
    if (rctx->window) {
      rctx->target_rastport = rctx->window->RPort;
      if (rctx->window->RPort && rctx->window->RPort->BitMap) {
        rctx->hidd_bitmap_obj = HIDD_BM_OBJ(rctx->window->RPort->BitMap);
      }
      D(bug("ZuneGfx: Target set to window %p RastPort\n", rctx->window));
    } else {
      D(bug("ZuneGfx: Warning - no window, keeping current target_rastport\n"));
    }
  }

  EXIT_FUNCTION("ZuneSetTarget");
  return TRUE;

  AROS_LIBFUNC_EXIT
}
