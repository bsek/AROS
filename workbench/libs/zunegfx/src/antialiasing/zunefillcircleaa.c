/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneFillCircleAA
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
 *       ZuneFillCircleAA -- Fill an antialiased circle on a RenderContext.
 *
 *   SYNOPSIS
 *       ZuneFillCircleAA(rctx, centerX, centerY, radius, color)
 *
 *       VOID ZuneFillCircleAA(struct RenderContext *rctx, UWORD centerX,
 *                             UWORD centerY, UWORD radius, ULONG color)
 *
 *   FUNCTION
 *       Fills a circle with antialiased edges using the specified color.
 *       The circle is drawn with smooth, antialiased edges to reduce
 *       visual artifacts.
 *
 *   INPUTS
 *       rctx -- The RenderContext to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       color -- 32-bit ARGB color value
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneFillCircleAA,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         struct Library *, ZuneGfxBase, 55, zunegfx)
{
  AROS_LIBFUNC_INIT

  if (!rctx || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  ZUNE_BACKEND_CALL(rctx, DrawCircle, centerX, centerY, radius, (UBYTE)0, brush, NULL, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}
