/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawCircleOutline
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
AROS_LH4(void, ZuneDrawCircleOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, center, A1), AROS_LHA(UBYTE, radius, D0),
         AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 43, zunegfx)

/*  FUNCTION
    Draws a circle outline.

INPUTS
    rctx - RenderContext
    centerX, centerY - Center coordinates
    radius - Circle radius
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawCircle");

  if (!rctx || !center) {
    D(bug("DrawCircleOutline: Invalid parameters (rctx=%p, center=%p)\n", rctx,
          center));
    EXIT_FUNCTION("DrawCircle");
    return;
  }

  if (radius == 0) {
    D(bug("DrawCircleOutline: Invalid radius\n"));
    EXIT_FUNCTION("DrawCircleOutline");
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawCircle, centerX, centerY, radius, (UBYTE)1, NULL,
                    &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawCircle");

  AROS_LIBFUNC_EXIT
}
