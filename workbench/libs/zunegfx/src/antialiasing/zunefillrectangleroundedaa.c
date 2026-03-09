/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillRectangleRoundedAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneFillRectangleRoundedAA *********************
 *
 *   NAME
 *       ZuneFillRectangleRoundedAA -- Fill an antialiased rounded rectangle on
 *a RenderContext.
 *
 *   SYNOPSIS
 *       ZuneFillRectangleRoundedAA(rctx, x, y, width, height, cornerRadius,
 *color)
 *
 *       VOID ZuneFillRectangleRoundedAA(struct RenderContext *rctx, UWORD x,
 *       UWORD *y, UWORD width, UWORD height, UBYTE cornerRadius, IPTR color)
 *
 *   FUNCTION
 *       Fills a rounded rectangle with antialiased edges. The corner radius
 *       determines how rounded the corners are.
 *
 *   INPUTS
 *       rctx -- The RenderContext to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       cornerRadius -- Radius of the rounded corners in pixels
 *       color -- Color value for the fill
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneFillRectangleRoundedAA,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),
         struct Library *, ZuneGfxBase, 62, zunegfx)
{
  AROS_LIBFUNC_INIT

  if (!rctx || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneFillRectangleRoundedAA: Invalid rectangle dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  D(bug(
      "ZuneFillRectangleRoundedAA: CALLING backend with cornerRadius=%d\n",
      (WORD)cornerRadius));
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)0, cornerRadius,
                    brush, NULL, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}
