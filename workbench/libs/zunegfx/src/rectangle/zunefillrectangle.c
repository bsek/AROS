/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillRectangle
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
AROS_LH3(void, ZuneFillRectangle,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 36, zunegfx)

/*  FUNCTION
    Draws a filled rectangle.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    color - Fill color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangle");

  D(bug("DrawRectangle x = %d, y = %d, width = %d, height = %d\n", rect->x,
        rect->y, rect->width, rect->height));

  if (!rctx || !rect) {
    D(bug("DrawRectangle: Invalid parameters (rctx=%p, rect=%p)\n", rctx, rect));
    EXIT_FUNCTION("DrawRectangle");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangle: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangle");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  /* Batch solid-color fills when batching is active */
  if (rctx->batching_enabled && rctx->batch_state && brush &&
      brush->type == ZUNE_BRUSH_TYPE_SOLID) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddCommandToBatch(batch, BATCH_CMD_FILL_RECT, x, y, width, height, 0,
                           0, brush->data.solid.color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_FILL_RECT, x, y, width, height, 0, 0,
                        brush->data.solid.color);
    }
    EXIT_FUNCTION("DrawRectangle");
    return;
  }

  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, 0, 0, brush,
                    NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawRectangle");

  AROS_LIBFUNC_EXIT
}
