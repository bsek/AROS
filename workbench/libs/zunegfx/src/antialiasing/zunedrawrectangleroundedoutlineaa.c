/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawRectangleRoundedOutlineAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneDrawRectangleRoundedOutlineAA **************
 *
 *   NAME
 *       ZuneDrawRectangleRoundedOutlineAA -- Draw antialiased rounded rectangle
 *border with default line width.
 *
 *   SYNOPSIS
 *       ZuneDrawRectangleRoundedOutlineAA(rctx, x, y, width, height,
 *corner_radius, color)
 *
 *       VOID ZuneDrawRectangleRoundedOutlineAA(struct RenderContext *rctx, UWORD x,
 *UWORD y, UWORD width, UWORD height, UBYTE corner_radius, IPTR color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a rounded rectangle with antialiased
 *edges. Uses a default line width of 1 pixel.
 *
 *   INPUTS
 *       rctx -- The RenderContext to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       corner_radius -- Radius of the rounded corners in pixels
 *       color -- Color value for the outline
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneDrawRectangleRoundedOutlineAA,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, corner_radius, D0),
         AROS_LHA(IPTR, color, D1),
         struct Library *, ZuneGfxBase, 65, zunegfx)

{
  AROS_LIBFUNC_INIT

  D(bug("Write ZuneDrawRectangleRoundedOutlineAA\n"));

  if (!rctx || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneDrawRectangleRoundedOutlineAA: Invalid rectangle dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor border_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);

  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, 1.0, corner_radius,
                    NULL, &border_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}
