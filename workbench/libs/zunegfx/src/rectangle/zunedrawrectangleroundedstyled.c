/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawRectangleRoundedStyled
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
AROS_LH6(void, ZuneDrawRectangleRoundedStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(struct ZuneBrush *, fillBrush, A2),
         AROS_LHA(ULONG, borderColor, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 50, zunegfx)

/*  FUNCTION
    Draws a rounded rectangle with both fill and border.

INPUTS
    rctx - RenderContext
    rect - Rectangle dimensions
    cornerRadius - Corner radius
    borderWidth - Border width
    fillBrush - Brush for the fill
    borderColor - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawRectangleRoundedStyled");

  if (!rctx || !rect) {
    D(bug("ZuneDrawRectangleRoundedStyled: Invalid parameters (rctx=%p, "
          "rect=%p)\n",
          rctx, rect));
    EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneDrawRectangleRoundedStyled: Invalid parameters\n"));
    EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor internal_border_color =
      ZuneColorToInternal(rctx, borderColor, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, borderWidth,
                    cornerRadius, fillBrush, &internal_border_color, TRUE, FALSE);

  EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");

  AROS_LIBFUNC_EXIT
}
