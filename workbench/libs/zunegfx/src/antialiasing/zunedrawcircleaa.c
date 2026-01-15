/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawCircleAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneDrawCircleAA ******************
 *
 *   NAME
 *       ZuneDrawCircleAA -- Draw an antialiased circle outline
 *
 *   SYNOPSIS
 *       ZuneDrawCircleAA(rp, centerX, centerY, radius, color)
 *
 *       VOID ZuneDrawCircleAA(struct RenderPort *rp, UWORD centerX,
 *                                          UWORD centerY, UWORD radius,
 *                                          ULONG color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a circle with antialiased edges.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       color -- 32-bit ARGB color value for the outline
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneDrawCircleAA, AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1), AROS_LHA(UWORD, radius, D0),
         AROS_LHA(ULONG, color, D1), struct Library *, ZuneGfxBase, 54,
         zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, 1, NULL,
                    &internal_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}
