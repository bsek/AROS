/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Core Drawing Implementation

    This file implements the main drawing API functions using the unified
    target system. All drawing operations go through this layer which
    dispatches to the appropriate backend.
*/

#include "backends/backend_interface.h"
#include "zunerenderer_intern.h"
#include <aros/debug.h>
#include <aros/libcall.h>

/*****************************************************************************/
/* Core Drawing API Implementation - AROS Library Functions */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneFillCircle,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1), AROS_LHA(UBYTE, radius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 38, zunerenderer)

/*  FUNCTION
    Draws a filled circle.

INPUTS
    rp - RenderPort
    centerX, centerY - Center coordinates
    radius - Circle radius
    color - Fill color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawCircle");

  if (!rp || !center) {
    D(bug("DrawCircle: Invalid parameters (rp=%p, center=%p)\n", rp, center));
    EXIT_FUNCTION("DrawCircle");
    return;
  }

  if (radius == 0) {
    D(bug("DrawCircle: Invalid radius\n"));
    EXIT_FUNCTION("DrawCircle");
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, (UBYTE)0, brush, NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawCircle");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawCircleOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1), AROS_LHA(UBYTE, radius, D0),
         AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 43, zunerenderer)

/*  FUNCTION
    Draws a circle outline.

INPUTS
    rp - RenderPort
    centerX, centerY - Center coordinates
    radius - Circle radius
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawCircle");

  if (!rp || !center) {
    D(bug("DrawCircleOutline: Invalid parameters (rp=%p, center=%p)\n", rp,
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
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, (UBYTE)1, NULL,
                    &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawCircle");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawLine,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 39, zunerenderer)

/*  FUNCTION
    Draws a line using Bresenham's line algorithm.

INPUTS
    rp - RenderPort
    startX, startY - Starting coordinates
    endX, endY - Ending coordinates
    color - Line color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawLine");

  if (!rp || !start || !end) {
    D(bug("ZuneDrawLine: Invalid parameters (rp=%p, start=%p, end=%p)\n", rp,
          start, end));
    EXIT_FUNCTION("ZuneDrawLine");
    return;
  }

  const WORD startX = start->x;
  const WORD startY = start->y;
  const WORD endX = end->x;
  const WORD endY = end->y;

  struct InternalColor internal_color = ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawLine, startX, startY, endX, endY, 1.0, &internal_color, FALSE);

  EXIT_FUNCTION("ZuneDrawLine");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawCircleOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UBYTE, radius, D0),
         AROS_LHA(UBYTE, line_width, D1),
         AROS_LHA(ULONG, color, D2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 46, zunerenderer)

/*  FUNCTION
    Draws a styled circle outline.

INPUTS
    rp - RenderPort
    centerX, centerY - Center coordinates
    radius - Circle radius
    lineWidth - Line width for outline
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawCircleOutlineStyled");

  if (!rp || !center) {
    D(bug("DrawCircleOutlineStyled: Invalid parameters (rp=%p, center=%p)\n",
          rp, center));
    EXIT_FUNCTION("DrawCircleOutlineStyled");
    return;
  }

  if (radius == 0) {
    D(bug("DrawCircleOutlineStyled: Invalid radius\n"));
    EXIT_FUNCTION("DrawCircleOutlineStyled");
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, line_width, NULL,
                    &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawCircleOutlineStyled");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawLineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(UWORD, line_width, D0),
         AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 48, zunerenderer)

/*  FUNCTION
    Draws a styled line using Bresenham's line algorithm.

INPUTS
    rp - RenderPort
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

  if (!rp || !start || !end) {
    D(bug("ZuneDrawLineStyled: Invalid parameters (rp=%p, start=%p, end=%p)\n",
          rp, start, end));
    EXIT_FUNCTION("ZuneDrawLineStyled");
    return;
  }

  const WORD startX = start->x;
  const WORD startY = start->y;
  const WORD endX = end->x;
  const WORD endY = end->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawLine, startX, startY, endX, endY, line_width,
                    &internal_color, FALSE);

  EXIT_FUNCTION("ZuneDrawLineStyled");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneDrawPixel,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, point, A1),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 52, zunerenderer)

/*  FUNCTION
    Draws a single pixel.

INPUTS
    rp - RenderPort
    x, y - Pixel coordinates
    color - Pixel color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawPixel");

  if (!rp || !point) {
    D(bug("ZuneDrawPixel: Invalid parameters (rp=%p, point=%p)\n", rp, point));
    EXIT_FUNCTION("ZuneDrawPixel");
    return;
  }

  const WORD x = point->x;
  const WORD y = point->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawPixel, x, y, &internal_color, FALSE);

  EXIT_FUNCTION("ZuneDrawPixel");

  AROS_LIBFUNC_EXIT
}
