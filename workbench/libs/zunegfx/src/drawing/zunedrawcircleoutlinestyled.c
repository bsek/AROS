/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawCircleOutlineStyled
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawCircleOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UBYTE, radius, D0),
         AROS_LHA(UBYTE, line_width, D1),
         AROS_LHA(ULONG, color, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 46, zunegfx)

/*  FUNCTION
    Draws a styled circle outline.

INPUTS
    rctx - RenderContext
    centerX, centerY - Center coordinates
    radius - Circle radius
    lineWidth - Line width for outline
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawCircleOutlineStyled");

  if (!rctx || !center) {
    D(bug("DrawCircleOutlineStyled: Invalid parameters (rctx=%p, center=%p)\n",
          rctx, center));
    EXIT_FUNCTION("DrawCircleOutlineStyled");
    return;
  }

  if (radius == 0) {
    D(bug("DrawCircleOutlineStyled: Invalid radius\n"));
    EXIT_FUNCTION("DrawCircleOutlineStyled");
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawCircle, centerX, centerY, radius, line_width, NULL,
                    &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawCircleOutlineStyled");

  AROS_LIBFUNC_EXIT
}
