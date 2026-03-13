/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillRectangleRounded
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
AROS_LH4(void, ZuneFillRectangleRounded,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 37, zunegfx)

/*  FUNCTION
    Draws a filled rounded rectangle.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    cornerRadius - Corner radius
    color - Fill color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleRounded");

  if (!rctx || !rect) {
    D(bug("DrawRectangleRounded: Invalid parameters (rctx=%p, rect=%p)\n", rctx,
          rect));
    EXIT_FUNCTION("DrawRectangleRounded");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleRounded: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleRounded");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)0, cornerRadius, brush, NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawRectangleRounded");

  AROS_LIBFUNC_EXIT
}
