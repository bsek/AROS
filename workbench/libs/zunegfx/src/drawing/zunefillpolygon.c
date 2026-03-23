/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneFillPolygon
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
AROS_LH4(void, ZuneFillPolygon,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, points, A1),
         AROS_LHA(UWORD, count, D0),
         AROS_LHA(const struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 118, zunegfx)

/*  FUNCTION
    Fill a closed polygon defined by a series of points.
    The polygon is automatically closed (last point connects to first).

INPUTS
    rctx   - RenderContext
    points - Array of ZunePoint structures defining polygon vertices
    count  - Number of vertices (must be >= 3)
    brush  - Fill brush

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !points || count < 3 || !brush) {
    return;
  }

  ZUNE_BACKEND_CALL(rctx, FillPolygon, points, count,
                    (struct ZuneBrush *)brush, FALSE);

  AROS_LIBFUNC_EXIT
}
