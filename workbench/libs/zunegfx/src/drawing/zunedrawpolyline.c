/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawPolyline
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
AROS_LH4(void, ZuneDrawPolyline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, points, A1),
         AROS_LHA(UWORD, count, D0),
         AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 116, zunegfx)

/*  FUNCTION
    Draw connected line segments through a series of points.
    Lines are drawn from points[0] to points[1] to ... to points[count-1].

INPUTS
    rctx   - RenderContext
    points - Array of ZunePoint structures
    count  - Number of points (must be >= 2)
    color  - Line color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  UWORD i;

  if (!rctx || !points || count < 2) {
    return;
  }

  struct InternalColor internal_color = ZuneColorToInternal(rctx, color, rctx->pixel_format);

  for (i = 0; i < count - 1; i++) {
    ZUNE_BACKEND_CALL(rctx, DrawLine,
                      points[i].x, points[i].y,
                      points[i + 1].x, points[i + 1].y,
                      1, &internal_color, FALSE);
  }

  AROS_LIBFUNC_EXIT
}
