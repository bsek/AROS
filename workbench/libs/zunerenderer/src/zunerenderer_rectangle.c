/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Core Drawing Implementation

    This file implements the main drawing API functions using the unified
    target system. All drawing operations go through this layer which
    dispatches to the appropriate backend.
*/
#define DEBUG 0
#include "backends/backend_interface.h"
#include "zunerenderer_intern.h"
#include <aros/debug.h>
#include <aros/libcall.h>
/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneFillRectangle,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 36, zunerenderer)

/*  FUNCTION
    Draws a filled rectangle.

INPUTS
    rp - RenderPort
    x, y - Top-left corner
    width, height - Rectangle dimensions
    color - Fill color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangle");

  D(bug("DrawRectangle x = %d, y = %d, width = %d, height = %d\n", rect->x,
        rect->y, rect->width, rect->height));

  if (!rp || !rect) {
    D(bug("DrawRectangle: Invalid parameters (rp=%p, rect=%p)\n", rp, rect));
    EXIT_FUNCTION("DrawRectangle");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangle: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangle");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, 0, 0, brush,
                    NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawRectangle");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneFillRectangleRounded,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 37, zunerenderer)

/*  FUNCTION
    Draws a filled rounded rectangle.

INPUTS
    rp - RenderPort
    x, y - Top-left corner
    width, height - Rectangle dimensions
    cornerRadius - Corner radius
    color - Fill color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleRounded");

  if (!rp || !rect) {
    D(bug("DrawRectangleRounded: Invalid parameters (rp=%p, rect=%p)\n", rp,
          rect));
    EXIT_FUNCTION("DrawRectangleRounded");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleRounded: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleRounded");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, (UBYTE)0, cornerRadius, brush, NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawRectangleRounded");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneDrawRectangleOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 41, zunerenderer)

/*  FUNCTION
    Draws a rectangle outline.

INPUTS
    rp - RenderPort
    x, y - Top-left corner
    width, height - Rectangle dimensions
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleOutline");

  if (!rp || !rect) {
    D(bug("DrawRectangleOutline: Invalid parameters (rp=%p, rect=%p)\n", rp,
          rect));
    EXIT_FUNCTION("DrawRectangleOutline");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleOutline: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleOutline");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor border_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, (UBYTE)1, (UBYTE)0, NULL,
                    &border_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleOutline");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawRectangleRoundedOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0), AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 42, zunerenderer)

/*  FUNCTION
    Draws a rounded rectangle outline.

INPUTS
    rp - RenderPort
    x, y - Top-left corner
    width, height - Rectangle dimensions
    cornerRadius - Corner radius
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleRoundedOutline");

  if (!rp || !rect) {
    D(bug("DrawRectangleRoundedOutline: Invalid parameters (rp=%p, rect=%p)\n",
          rp, rect));
    EXIT_FUNCTION("DrawRectangleRoundedOutline");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleRoundedOutline: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleRoundedOutline");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, (UBYTE)1, cornerRadius,
                    NULL, &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleRoundedOutline");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawRectangleRoundedOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0), AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(ULONG, color, D2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 47, zunerenderer)

/*  FUNCTION
    Draws a styled rounded rectangle outline.

INPUTS
    rp - RenderPort
    x, y - Top-left corner
    width, height - Rectangle dimensions
    cornerRadius - Corner radius
    lineWidth - Line width for outline
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleRoundedOutlineStyled");

  if (!rp || !rect) {
    D(bug("DrawRectangleRoundedOutlineStyled: Invalid parameters (rp=%p, "
          "rect=%p)\n",
          rp, rect));
    EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleRoundedOutlineStyled: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, borderWidth,
                    cornerRadius, NULL, &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH6(void, ZuneDrawRectangleRoundedStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(struct ZuneBrush *, fillBrush, A2),
         AROS_LHA(ULONG, borderColor, D2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 50, zunerenderer)

/*  FUNCTION
    Draws a rounded rectangle with both fill and border.

INPUTS
    rp - RenderPort
    rect - Rectangle dimensions
    cornerRadius - Corner radius
    borderWidth - Border width
    fillBrush - Brush for the fill
    borderColor - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneDrawRectangleRoundedStyled");

  if (!rp || !rect) {
    D(bug("ZuneDrawRectangleRoundedStyled: Invalid parameters (rp=%p, "
          "rect=%p)\n",
          rp, rect));
    EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneDrawRectangleRoundedStyled: Invalid parameters\n"));
    EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor internal_border_color =
      ZuneColorToInternal(rp, borderColor, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, borderWidth,
                    cornerRadius, fillBrush, &internal_border_color, TRUE, FALSE);

  EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawRectangleOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, borderWidth, D0), AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 45, zunerenderer)

/*  FUNCTION
    Draws a styled rectangle outline.

INPUTS
    rp - RenderPort
    x, y - Top-left corner
    width, height - Rectangle dimensions
    lineWidth - Line width for outline
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleOutlineStyled");

  if (!rp || !rect) {
    D(bug("DrawRectangleOutlineStyled: Invalid parameters (rp=%p, rect=%p)\n",
          rp, rect));
    EXIT_FUNCTION("DrawRectangleOutlineStyled");
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("DrawRectangleOutlineStyled: Invalid parameters\n"));
    EXIT_FUNCTION("DrawRectangleOutlineStyled");
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor border_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, borderWidth, 0.0,
                    NULL, &border_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleOutlineStyled");

  AROS_LIBFUNC_EXIT
}
