/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillCircleAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneFillCircleAA ******************************
 *
 *   NAME
 *       ZuneFillCircleAA -- Fill an antialiased circle on a RenderPort.
 *
 *   SYNOPSIS
 *       ZuneFillCircleAA(rp, centerX, centerY, radius, color)
 *
 *       VOID ZuneFillCircleAA(struct RenderPort *rp, UWORD centerX,
 *                             UWORD centerY, UWORD radius, ULONG color)
 *
 *   FUNCTION
 *       Fills a circle with antialiased edges using the specified color.
 *       The circle is drawn with smooth, antialiased edges to reduce
 *       visual artifacts.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       color -- 32-bit ARGB color value
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneFillCircleAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         struct Library *, ZuneGfxBase, 55, zunegfx)
{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, (UBYTE)0, brush, NULL, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}
