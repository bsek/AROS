/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawLine
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../batching/batching_intern.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawLine,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 39, zunegfx)

/*  FUNCTION
    Draws a line using Bresenham's line algorithm.

INPUTS
    rctx - RenderContext
    startX, startY - Starting coordinates
    endX, endY - Ending coordinates
    color - Line color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawLine");

  if (!rctx || !start || !end) {
    D(bug("ZuneDrawLine: Invalid parameters (rctx=%p, start=%p, end=%p)\n", rctx,
          start, end));
    EXIT_FUNCTION("ZuneDrawLine");
    return;
  }

  const WORD startX = start->x;
  const WORD startY = start->y;
  const WORD endX = end->x;
  const WORD endY = end->y;

  /* Batch lines when batching is active */
  if (rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddCommandToBatch(batch, BATCH_CMD_DRAW_LINE, startX, startY, 0, 0,
                           endX, endY, color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_DRAW_LINE, startX, startY, 0, 0, endX,
                        endY, color);
    }
    EXIT_FUNCTION("ZuneDrawLine");
    return;
  }

  struct InternalColor internal_color = ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawLine, startX, startY, endX, endY, 1.0, &internal_color, FALSE);

  EXIT_FUNCTION("ZuneDrawLine");

  AROS_LIBFUNC_EXIT
}
