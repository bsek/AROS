/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawPolylineStyled
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
AROS_LH5(void, ZuneDrawPolylineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, points, A1),
         AROS_LHA(UWORD, count, D0),
         AROS_LHA(UWORD, lineWidth, D1),
         AROS_LHA(ULONG, color, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 117, zunegfx)

/*  FUNCTION
    Draw connected line segments with custom line width.

INPUTS
    rctx      - RenderContext
    points    - Array of ZunePoint structures
    count     - Number of points (must be >= 2)
    lineWidth - Line width in pixels
    color     - Line color in ARGB format

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
                      lineWidth, &internal_color, FALSE);
  }

  AROS_LIBFUNC_EXIT
}
