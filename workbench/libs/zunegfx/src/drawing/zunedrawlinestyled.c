/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawLineStyled
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
AROS_LH5(void, ZuneDrawLineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(UWORD, line_width, D0),
         AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 48, zunegfx)

/*  FUNCTION
    Draws a styled line using Bresenham's line algorithm.

INPUTS
    rctx - RenderContext
    startX, startY - Starting coordinates
    endX, endY - Ending coordinates
    lineWidth - Width of the line
    color - Line color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawLineStyled");

  if (!rctx || !start || !end) {
    D(bug("ZuneDrawLineStyled: Invalid parameters (rctx=%p, start=%p, end=%p)\n",
          rctx, start, end));
    EXIT_FUNCTION("ZuneDrawLineStyled");
    return;
  }

  const WORD startX = start->x;
  const WORD startY = start->y;
  const WORD endX = end->x;
  const WORD endY = end->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawLine, startX, startY, endX, endY, line_width,
                    &internal_color, FALSE);

  EXIT_FUNCTION("ZuneDrawLineStyled");

  AROS_LIBFUNC_EXIT
}
