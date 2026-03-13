/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawRectangleOutline
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
AROS_LH3(void, ZuneDrawRectangleOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 41, zunegfx)

/*  FUNCTION
    Draws a rectangle outline.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleOutline");

  if (!rctx || !rect) {
    D(bug("DrawRectangleOutline: Invalid parameters (rctx=%p, rect=%p)\n", rctx,
          rect));
    EXIT_FUNCTION("DrawRectangleOutline");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleOutline: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleOutline");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  /* Batch outline rectangles when batching is active */
  if (rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddCommandToBatch(batch, BATCH_CMD_DRAW_RECT, x, y, width, height, 0,
                           0, color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_DRAW_RECT, x, y, width, height, 0, 0,
                        color);
    }
    EXIT_FUNCTION("DrawRectangleOutline");
    return;
  }

  struct InternalColor border_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)1, (UBYTE)0, NULL,
                    &border_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleOutline");

  AROS_LIBFUNC_EXIT
}
