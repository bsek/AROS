/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillCircleStyledAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneFillCircleStyledAA *************************
 *
 *   NAME
 *       ZuneFillCircleStyledAA -- Fill an antialiased circle with border
 *styling.
 *
 *   SYNOPSIS
 *       ZuneFillCircleStyledAA(rp, centerX, centerY, radius, borderWidth,
 *color, borderColor)
 *
 *       VOID ZuneFillCircleStyledAA(struct RenderPort *rp, UWORD centerX,
 *                                   UWORD centerY, UWORD radius, UBYTE
 *borderWidth, ULONG color, ULONG borderColor)
 *
 *   FUNCTION
 *       Fills a circle with antialiased edges and draws an antialiased border
 *       around it. Both fill and border colors can be specified independently.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       borderWidth -- Width of the border outline in pixels
 *       color -- 32-bit ARGB color value for the fill
 *       borderColor -- 32-bit ARGB color value for the border
 *
 ***************************************************************************/

AROS_LH6(VOID, ZuneFillCircleStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(struct ZuneBrush *, brush, A2),
         AROS_LHA(ULONG, borderColor, D2),

         struct Library *, ZuneGfxBase, 57, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor border_color =
      ZuneColorToInternal(rp, borderColor, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, borderWidth, brush, &border_color, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}
