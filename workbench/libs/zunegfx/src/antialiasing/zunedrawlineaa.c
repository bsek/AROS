/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawLineAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneDrawLineAA *********************************
 *
 *   NAME
 *       ZuneDrawLineAA -- Draw an antialiased line with default width.
 *
 *   SYNOPSIS
 *       ZuneDrawLineAA(rctx, startX, startY, endX, endY, color)
 *
 *       VOID ZuneDrawLineAA(struct RenderContext *rctx, UWORD startX, UWORD startY,
 *                           UWORD endX, UWORD endY, ULONG color)
 *
 *   FUNCTION
 *       Draws a line with antialiased edges between two points using the
 *       specified color. Uses a default line width of 1 pixel.
 *
 *   INPUTS
 *       rctx -- The RenderContext to draw on
 *       startX -- X coordinate of the line start point
 *       startY -- Y coordinate of the line start point
 *       endX -- X coordinate of the line end point
 *       endY -- Y coordinate of the line end point
 *       color -- 32-bit ARGB color value
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneDrawLineAA, AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(ULONG, color, D0),
         struct Library *, ZuneGfxBase, 67, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rctx || !start || !end) {
    return;
  }

  const WORD startX = start->x;
  const WORD startY = start->y;
  const WORD endX = end->x;
  const WORD endY = end->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);

  ZUNE_BACKEND_CALL(rctx, DrawLine, startX, startY, endX, endY, (UWORD)1,
                    &internal_color, TRUE);

  AROS_LIBFUNC_EXIT
}
