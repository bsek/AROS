/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillRectangleRoundedStyledAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneFillRectangleRoundedStyledAA ***************
 *
 *   NAME
 *       ZuneFillRectangleRoundedStyledAA -- Fill an antialiased rounded
 *rectangle with border styling.
 *
 *   SYNOPSIS
 *       ZuneFillRectangleRoundedStyledAA(rp, x, y, width, height,
 *corner_radius, border_width, fill_color, border_color)
 *
 *       VOID ZuneFillRectangleRoundedStyledAA(struct RenderPort *rp, UWORD x,
 *UWORD y, UWORD width, UWORD height, UBYTE corner_radius, UBYTE border_width,
 *IPTR fill_color, IPTR border_color)
 *
 *   FUNCTION
 *       Fills a rounded rectangle with antialiased edges and draws an
 *antialiased border around it. Both fill and border colors can be specified
 *independently.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       corner_radius -- Radius of the rounded corners in pixels
 *       border_width -- Width of the border outline in pixels
 *       fill_color -- Color value for the fill
 *       border_color -- Color value for the border
 *
 ***************************************************************************/

AROS_LH6(VOID, ZuneFillRectangleRoundedStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, corner_radius, D0),
         AROS_LHA(UBYTE, border_width, D1),
         AROS_LHA(struct ZuneBrush *, fill_brush, A2),
         AROS_LHA(ULONG, border_color, D2),
         struct Library *, ZuneGfxBase, 63, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rp || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneFillRectangleRoundedStyledAA: Invalid rectangle dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor borderColor =
      ZuneColorToInternal(rp, border_color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, border_width,
                    corner_radius, fill_brush, &borderColor, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}
