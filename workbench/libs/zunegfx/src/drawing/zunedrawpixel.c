/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawPixel
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
AROS_LH3(void, ZuneDrawPixel,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, point, A1),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 52, zunegfx)

/*  FUNCTION
    Draws a single pixel.

INPUTS
    rctx - RenderContext
    x, y - Pixel coordinates
    color - Pixel color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawPixel");

  if (!rctx || !point) {
    D(bug("ZuneDrawPixel: Invalid parameters (rctx=%p, point=%p)\n", rctx, point));
    EXIT_FUNCTION("ZuneDrawPixel");
    return;
  }

  const WORD x = point->x;
  const WORD y = point->y;

  /* Batch pixels when batching is active */
  if (rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddCommandToBatch(batch, BATCH_CMD_DRAW_PIXEL, x, y, 0, 0, 0, 0,
                           color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_DRAW_PIXEL, x, y, 0, 0, 0, 0, color);
    }
    EXIT_FUNCTION("ZuneDrawPixel");
    return;
  }

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawPixel, x, y, &internal_color, FALSE);

  EXIT_FUNCTION("ZuneDrawPixel");

  AROS_LIBFUNC_EXIT
}
