/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawRectangleRoundedOutline
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
AROS_LH4(void, ZuneDrawRectangleRoundedOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0), AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 42, zunegfx)

/*  FUNCTION
    Draws a rounded rectangle outline.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    cornerRadius - Corner radius
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleRoundedOutline");

  if (!rctx || !rect) {
    D(bug("DrawRectangleRoundedOutline: Invalid parameters (rctx=%p, rect=%p)\n",
          rctx, rect));
    EXIT_FUNCTION("DrawRectangleRoundedOutline");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleRoundedOutline: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleRoundedOutline");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)1, cornerRadius,
                    NULL, &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleRoundedOutline");

  AROS_LIBFUNC_EXIT
}
