/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawRectangleOutlineStyled
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
AROS_LH4(void, ZuneDrawRectangleOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, borderWidth, D0), AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 45, zunegfx)

/*  FUNCTION
    Draws a styled rectangle outline.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    lineWidth - Line width for outline
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleOutlineStyled");

  if (!rctx || !rect) {
    D(bug("DrawRectangleOutlineStyled: Invalid parameters (rctx=%p, rect=%p)\n",
          rctx, rect));
    EXIT_FUNCTION("DrawRectangleOutlineStyled");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleOutlineStyled: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleOutlineStyled");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  /* Batch styled outline rectangles when batching is active */
  if (rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddStyledCommandToBatch(batch, BATCH_CMD_DRAW_RECT_STYLED, x, y,
                                 width, height, borderWidth, color)) {
      ExecuteBatchCommands(batch);
      AddStyledCommandToBatch(batch, BATCH_CMD_DRAW_RECT_STYLED, x, y, width,
                              height, borderWidth, color);
    }
    EXIT_FUNCTION("DrawRectangleOutlineStyled");
    return;
  }

  struct InternalColor border_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, borderWidth, 0.0,
                    NULL, &border_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleOutlineStyled");

  AROS_LIBFUNC_EXIT
}
