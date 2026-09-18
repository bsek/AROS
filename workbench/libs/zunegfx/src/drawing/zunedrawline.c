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
         AROS_LHA(const struct ZuneShapeStyle *, style, A3),

/*  LOCATION */
         struct Library *, ZuneGfxBase, 39, zunegfx)

/*  FUNCTION
    Draws a line. A line is a pure stroke, so it reads stroke_color,
    stroke_width and the flags from the style and ignores fill and
    corner_radius. A stroke_width of 0 is treated as 1.

INPUTS
    rctx  - RenderContext
    start - first endpoint
    end   - second endpoint
    style - stroke colour, width and ZUNE_SHAPE_ANTIALIAS

RESULT
    None

SEE ALSO
    ZuneDrawPolyline(), ZuneDrawRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct InternalColor color;
  UWORD width;
  BOOL aa;

  ENTER_FUNCTION("ZuneDrawLine");

  if (!ValidateRenderContext(rctx) || !start || !end || !style) {
    EXIT_FUNCTION("ZuneDrawLine");
    return;
  }

  width = style->stroke_width ? style->stroke_width : 1;
  aa = (style->flags & ZUNE_SHAPE_ANTIALIAS) != 0;

  /* Batch plain thin lines while batching is active */
  if (!aa && width == 1 && rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;

    if (!AddCommandToBatch(batch, BATCH_CMD_DRAW_LINE, start->x, start->y, 0, 0,
                           end->x, end->y, style->stroke_color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_DRAW_LINE, start->x, start->y, 0, 0,
                        end->x, end->y, style->stroke_color);
    }
    EXIT_FUNCTION("ZuneDrawLine");
    return;
  }

  color = ZuneColorToInternal(rctx, style->stroke_color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawLine, start->x, start->y, end->x, end->y, width,
                    &color, aa);

  EXIT_FUNCTION("ZuneDrawLine");

  AROS_LIBFUNC_EXIT
}
