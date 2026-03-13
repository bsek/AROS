/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawRectangleRoundedOutlineStyled
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
AROS_LH5(void, ZuneDrawRectangleRoundedOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0), AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(ULONG, color, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 47, zunegfx)

/*  FUNCTION
    Draws a styled rounded rectangle outline.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    cornerRadius - Corner radius
    lineWidth - Line width for outline
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleRoundedOutlineStyled");

  if (!rctx || !rect) {
    D(bug("DrawRectangleRoundedOutlineStyled: Invalid parameters (rctx=%p, "
          "rect=%p)\n",
          rctx, rect));
    EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleRoundedOutlineStyled: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, borderWidth,
                    cornerRadius, NULL, &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");

  AROS_LIBFUNC_EXIT
}
