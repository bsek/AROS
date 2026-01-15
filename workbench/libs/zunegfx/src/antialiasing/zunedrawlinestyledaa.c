/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawLineStyledAA
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneDrawLineStyledAA ***************************
 *
 *   NAME
 *       ZuneDrawLineStyledAA -- Draw an antialiased line with custom width.
 *
 *   SYNOPSIS
 *       ZuneDrawLineStyledAA(rp, startX, startY, endX, endY, width, color)
 *
 *       VOID ZuneDrawLineStyledAA(struct RenderPort *rp, UWORD startX, UWORD
 *startY, UWORD endX, UWORD endY, UBYTE width, ULONG color)
 *
 *   FUNCTION
 *       Draws a line with antialiased edges between two points using the
 *       specified color and line width. The line width can be customized
 *       for thicker or thinner lines.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       startX -- X coordinate of the line start point
 *       startY -- Y coordinate of the line start point
 *       endX -- X coordinate of the line end point
 *       endY -- Y coordinate of the line end point
 *       width -- Width of the line in pixels
 *       color -- 32-bit ARGB color value
 *
 ***************************************************************************/

AROS_LH5(VOID, ZuneDrawLineStyledAA, AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(UBYTE, width, D0),
         AROS_LHA(ULONG, color, D1),
         struct Library *, ZuneGfxBase, 68, zunegfx)
{
  AROS_LIBFUNC_INIT

  if (!rp || !start || !end) {
    return;
  }

  const WORD startX = start->x;
  const WORD startY = start->y;
  const WORD endX = end->x;
  const WORD endY = end->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawLine, startX, startY, endX, endY, (float)width,
                    &internal_color, TRUE);

  AROS_LIBFUNC_EXIT
}
