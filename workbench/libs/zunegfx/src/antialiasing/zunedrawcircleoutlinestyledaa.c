/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawCircleOutlineStyledAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneDrawCircleOutlineStyledAA ******************
 *
 *   NAME
 *       ZuneDrawCircleOutlineStyledAA -- Draw an antialiased circle outline
 *with custom border width.
 *
 *   SYNOPSIS
 *       ZuneDrawCircleOutlineStyledAA(rp, centerX, centerY, radius,
 *borderWidth, color)
 *
 *       VOID ZuneDrawCircleOutlineStyledAA(struct RenderPort *rp, UWORD
 *centerX, UWORD centerY, UWORD radius, UBYTE borderWidth, ULONG color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a circle with antialiased edges.
 *       The border width can be customized.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       borderWidth -- Width of the border outline in pixels
 *       color -- 32-bit ARGB color value for the outline
 *
 ***************************************************************************/

AROS_LH5(VOID, ZuneDrawCircleOutlineStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(ULONG, color, D2),
         struct Library *, ZuneGfxBase, 56, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, borderWidth, NULL,
                    &internal_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}
