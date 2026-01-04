/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Generic Antialiasing Interface

    This module provides the generic antialiasing API that dispatches
    to backend-specific implementations. Different backends handle AA
    in the most appropriate way for their graphics system:

    - Graphics Library backend: No AA support (fallback to regular drawing)
    - CyberGraphX backend: Wu's line algorithm + SDF shapes
    - OpenGL backend: Hardware-based multisampling (future)

    Key features:
    - Backend abstraction for antialiasing
    - Graceful fallback when AA not supported
    - Unified API across all backends
    - Per-backend optimization opportunities
*/

#include <aros/libcall.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "backends/backend_interface.h"
#include "exec/libraries.h"
#include "include/zunerenderer.h"
#include "zunerenderer_intern.h"

#define DEBUG 0
#include <aros/debug.h>

static UBYTE g_aa_quality = 1;
/****** zunerenderer.library/ZuneDrawCircleAA ******************
 *
 *   NAME
 *       ZuneDrawCircleAA -- Draw an antialiased circle outline
 *
 *   SYNOPSIS
 *       ZuneDrawCircleAA(rp, centerX, centerY, radius, color)
 *
 *       VOID ZuneDrawCircleAA(struct RenderPort *rp, UWORD centerX,
 *                                          UWORD centerY, UWORD radius,
 *                                          ULONG color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a circle with antialiased edges.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       color -- 32-bit ARGB color value for the outline
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneDrawCircleAA, AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1), AROS_LHA(UWORD, radius, D0),
         AROS_LHA(ULONG, color, D1), struct Library *, ZuneRendererBase, 54,
         zunerenderer)

{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, 1, NULL,
                    &internal_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneFillCircleAA ******************************
 *
 *   NAME
 *       ZuneFillCircleAA -- Fill an antialiased circle on a RenderPort.
 *
 *   SYNOPSIS
 *       ZuneFillCircleAA(rp, centerX, centerY, radius, color)
 *
 *       VOID ZuneFillCircleAA(struct RenderPort *rp, UWORD centerX,
 *                             UWORD centerY, UWORD radius, ULONG color)
 *
 *   FUNCTION
 *       Fills a circle with antialiased edges using the specified color.
 *       The circle is drawn with smooth, antialiased edges to reduce
 *       visual artifacts.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       color -- 32-bit ARGB color value
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneFillCircleAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         struct Library *, ZuneRendererBase, 55, zunerenderer)
{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  // struct InternalColor internal_color;
  // if (!ZuneBrushToInternalColor(rp, brush, &internal_color)) {
  //   D(bug("ZuneFillCircleAA: Unsupported brush configuration\n"));
  //   return;
  // }

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, (UBYTE)0, brush, NULL, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneDrawCircleOutlineStyledAA ******************
 *
 *   NAME
 *       ZuneDrawCircleOutlineStyledAA -- Draw an antialiased circle outline
 *with custom border width.
 *
 *   SYNOPSIS
 *       ZuneDrawCircleOutlineStyledAA(rp, centerX, centerY, radius,
 *borderWidth, color)
 *
 *       VOID ZuneDrawCircleOutlineStyledAA(struct RenderPort *rp, UWORD
 *centerX, UWORD centerY, UWORD radius, UBYTE borderWidth, ULONG color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a circle with antialiased edges.
 *       The border width can be customized.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       borderWidth -- Width of the border outline in pixels
 *       color -- 32-bit ARGB color value for the outline
 *
 ***************************************************************************/

AROS_LH5(VOID, ZuneDrawCircleOutlineStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(ULONG, color, D2),
         struct Library *, ZuneRendererBase, 56, zunerenderer)

{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, borderWidth, NULL,
                    &internal_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneFillCircleStyledAA *************************
 *
 *   NAME
 *       ZuneFillCircleStyledAA -- Fill an antialiased circle with border
 *styling.
 *
 *   SYNOPSIS
 *       ZuneFillCircleStyledAA(rp, centerX, centerY, radius, borderWidth,
 *color, borderColor)
 *
 *       VOID ZuneFillCircleStyledAA(struct RenderPort *rp, UWORD centerX,
 *                                   UWORD centerY, UWORD radius, UBYTE
 *borderWidth, ULONG color, ULONG borderColor)
 *
 *   FUNCTION
 *       Fills a circle with antialiased edges and draws an antialiased border
 *       around it. Both fill and border colors can be specified independently.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       centerX -- X coordinate of the circle center
 *       centerY -- Y coordinate of the circle center
 *       radius -- Radius of the circle in pixels
 *       borderWidth -- Width of the border outline in pixels
 *       color -- 32-bit ARGB color value for the fill
 *       borderColor -- 32-bit ARGB color value for the border
 *
 ***************************************************************************/

AROS_LH6(VOID, ZuneFillCircleStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(struct ZuneBrush *, brush, A2),
         AROS_LHA(ULONG, borderColor, D2),

         struct Library *, ZuneRendererBase, 57, zunerenderer)

{
  AROS_LIBFUNC_INIT

  if (!rp || !center) {
    return;
  }

  const WORD centerX = center->x;
  const WORD centerY = center->y;

  // struct InternalColor fill_color;
  // if (!ZuneBrushToInternalColor(rp, brush, &fill_color)) {
  //   D(bug("ZuneFillCircleStyledAA: Unsupported brush configuration\n"));
  //   return;
  // }
  struct InternalColor border_color =
      ZuneColorToInternal(rp, borderColor, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawCircle, centerX, centerY, radius, borderWidth, brush, &border_color, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneSetAntialiasingQuality *********************
 *
 *   NAME
 *       ZuneSetAntialiasingQuality -- Set the antialiasing quality level
 *
 *   SYNOPSIS
 *       ZuneSetAntialiasingQuality(rp, quality);
 *                                  A0  D0
 *
 *       VOID ZuneSetAntialiasingQuality(struct RenderPort *, UBYTE);
 *
 *   FUNCTION
 *       Sets the antialiasing quality level for the graphics backend.
 *       This is a global setting that affects all subsequent antialiased
 *       drawing operations.
 *
 *   INPUTS
 *       rp -- RenderPort (used to determine the backend)
 *       quality -- Quality level (0=fast, 1=good, 2=best)
 *
 ***************************************************************************/

AROS_LH2(VOID, ZuneSetAntialiasingQuality,
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(UBYTE, quality, D0),
         struct Library *, ZuneRendererBase, 59, zunerenderer)

{
  AROS_LIBFUNC_INIT

  if (!rp) {
    return;
  }

  (void)rp;
  g_aa_quality = quality;

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneGetAntialiasingQuality *********************
 *
 *   NAME
 *       ZuneGetAntialiasingQuality -- Get the current antialiasing quality
 *
 *   SYNOPSIS
 *       quality = ZuneGetAntialiasingQuality(rp);
 *       D0                                   A0
 *
 *       UBYTE ZuneGetAntialiasingQuality(struct RenderPort *);
 *
 *   FUNCTION
 *       Returns the current antialiasing quality level from the graphics
 *       backend. This reflects the global quality setting that affects
 *       all antialiased drawing operations.
 *
 *   INPUTS
 *       rp -- RenderPort (used to determine the backend)
 *
 *   RESULT
 *       quality -- Current quality level (0=fast, 1=good, 2=best)
 *                  Returns 0 if backend doesn't support quality control
 *
 ***************************************************************************/

AROS_LH1(UWORD, ZuneGetAntialiasingQuality,
         AROS_LHA(struct RenderPort *, rp, A0), struct Library *,
         ZuneRendererBase, 60, zunerenderer)

{
  AROS_LIBFUNC_INIT

  if (!rp) {
    return 0;
  }

  (void)rp;
  return g_aa_quality;

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneFillRectangleRoundedAA *********************
 *
 *   NAME
 *       ZuneFillRectangleRoundedAA -- Fill an antialiased rounded rectangle on
 *a RenderPort.
 *
 *   SYNOPSIS
 *       ZuneFillRectangleRoundedAA(rp, x, y, width, height, cornerRadius,
 *color)
 *
 *       VOID ZuneFillRectangleRoundedAA(struct RenderPort *rp, UWORD x,
 *       UWORD *y, UWORD width, UWORD height, UBYTE cornerRadius, IPTR color)
 *
 *   FUNCTION
 *       Fills a rounded rectangle with antialiased edges. The corner radius
 *       determines how rounded the corners are.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       cornerRadius -- Radius of the rounded corners in pixels
 *       color -- Color value for the fill
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneFillRectangleRoundedAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),
         struct Library *, ZuneRendererBase, 62, zunerenderer)
{
  AROS_LIBFUNC_INIT

  if (!rp || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneFillRectangleRoundedAA: Invalid rectangle dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  // struct InternalColor internal_color;
  // if (!ZuneBrushToInternalColor(rp, brush, &internal_color)) {
  //   D(bug("ZuneFillRectangleRoundedAA: Unsupported brush configuration\n"));
  //   return;
  // }

  D(bug(
      "ZuneFillRectangleRoundedAA: CALLING backend with cornerRadius=%d\n",
      (int)cornerRadius));
  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, (UBYTE)0, cornerRadius,
                    brush, NULL, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneFillRectangleRoundedStyledAA ***************
 *
 *   NAME
 *       ZuneFillRectangleRoundedStyledAA -- Fill an antialiased rounded
 *rectangle with border styling.
 *
 *   SYNOPSIS
 *       ZuneFillRectangleRoundedStyledAA(rp, x, y, width, height,
 *corner_radius, border_width, fill_color, border_color)
 *
 *       VOID ZuneFillRectangleRoundedStyledAA(struct RenderPort *rp, UWORD x,
 *UWORD y, UWORD width, UWORD height, UBYTE corner_radius, UBYTE border_width,
 *IPTR fill_color, IPTR border_color)
 *
 *   FUNCTION
 *       Fills a rounded rectangle with antialiased edges and draws an
 *antialiased border around it. Both fill and border colors can be specified
 *independently.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       corner_radius -- Radius of the rounded corners in pixels
 *       border_width -- Width of the border outline in pixels
 *       fill_color -- Color value for the fill
 *       border_color -- Color value for the border
 *
 ***************************************************************************/

AROS_LH6(VOID, ZuneFillRectangleRoundedStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, corner_radius, D0),
         AROS_LHA(UBYTE, border_width, D1),
         AROS_LHA(struct ZuneBrush *, fill_brush, A2),
         AROS_LHA(ULONG, border_color, D2),
         struct Library *, ZuneRendererBase, 63, zunerenderer)

{
  AROS_LIBFUNC_INIT

  if (!rp || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneFillRectangleRoundedStyledAA: Invalid rectangle dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  // struct InternalColor fillColor;
  // if (!ZuneBrushToInternalColor(rp, fill_brush, &fillColor)) {
  //   D(bug(
  //       "ZuneFillRectangleRoundedStyledAA: Unsupported brush configuration\n"));
  //   return;
  // }
  struct InternalColor borderColor =
      ZuneColorToInternal(rp, border_color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, border_width,
                    corner_radius, fill_brush, &borderColor, TRUE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneDrawRectangleRoundedOutlineStyledAA ********
 *
 *   NAME
 *       ZuneDrawRectangleRoundedOutlineStyledAA -- Draw antialiased rounded
 *rectangle border with custom line width.
 *
 *   SYNOPSIS
 *       ZuneDrawRectangleRoundedOutlineStyledAA(rp, x, y, width, height,
 *                                               corner_radius, line_width,
 *color)
 *
 *       VOID ZuneDrawRectangleRoundedOutlineStyledAA(struct RenderPort *rp,
 *UWORD x, UWORD y, UWORD width, UWORD height, UBYTE corner_radius, UBYTE
 *line_width, IPTR color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a rounded rectangle with antialiased
 *edges. The line width can be customized.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       corner_radius -- Radius of the rounded corners in pixels
 *       line_width -- Width of the outline in pixels
 *       color -- Color value for the outline
 *
 ***************************************************************************/

AROS_LH5(VOID, ZuneDrawRectangleRoundedOutlineStyledAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, corner_radius, D0),
         AROS_LHA(UBYTE, line_width, D1),
         AROS_LHA(IPTR, color, D2),
         struct Library *, ZuneRendererBase, 64, zunerenderer)
{
  AROS_LIBFUNC_INIT

  D(bug("Write ZuneDrawRectangleRoundedOutlineStyledAA\n"));

  if (!rp || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneDrawRectangleRoundedOutlineStyledAA: Invalid rectangle "
          "dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor border_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, line_width,
                    corner_radius, NULL, &border_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneDrawRectangleRoundedOutlineAA **************
 *
 *   NAME
 *       ZuneDrawRectangleRoundedOutlineAA -- Draw antialiased rounded rectangle
 *border with default line width.
 *
 *   SYNOPSIS
 *       ZuneDrawRectangleRoundedOutlineAA(rp, x, y, width, height,
 *corner_radius, color)
 *
 *       VOID ZuneDrawRectangleRoundedOutlineAA(struct RenderPort *rp, UWORD x,
 *UWORD y, UWORD width, UWORD height, UBYTE corner_radius, IPTR color)
 *
 *   FUNCTION
 *       Draws only the outline/border of a rounded rectangle with antialiased
 *edges. Uses a default line width of 1 pixel.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       x -- X coordinate of the rectangle's top-left corner
 *       y -- Y coordinate of the rectangle's top-left corner
 *       width -- Width of the rectangle in pixels
 *       height -- Height of the rectangle in pixels
 *       corner_radius -- Radius of the rounded corners in pixels
 *       color -- Color value for the outline
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneDrawRectangleRoundedOutlineAA,
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, corner_radius, D0),
         AROS_LHA(IPTR, color, D1),
         struct Library *, ZuneRendererBase, 65, zunerenderer)

{
  AROS_LIBFUNC_INIT

  D(bug("Write ZuneDrawRectangleRoundedOutlineAA\n"));

  if (!rp || !rect) {
    return;
  }

  if (rect->width == 0 || rect->height == 0) {
    D(bug("ZuneDrawRectangleRoundedOutlineAA: Invalid rectangle dimensions\n"));
    return;
  }

  const WORD x = rect->x;
  const WORD y = rect->y;
  const UWORD width = rect->width;
  const UWORD height = rect->height;

  struct InternalColor border_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);

  ZUNE_BACKEND_CALL(rp, DrawRectangle, x, y, width, height, 1.0, corner_radius,
                    NULL, &border_color, FALSE, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneDrawLineAA *********************************
 *
 *   NAME
 *       ZuneDrawLineAA -- Draw an antialiased line with default width.
 *
 *   SYNOPSIS
 *       ZuneDrawLineAA(rp, startX, startY, endX, endY, color)
 *
 *       VOID ZuneDrawLineAA(struct RenderPort *rp, UWORD startX, UWORD startY,
 *                           UWORD endX, UWORD endY, ULONG color)
 *
 *   FUNCTION
 *       Draws a line with antialiased edges between two points using the
 *       specified color. Uses a default line width of 1 pixel.
 *
 *   INPUTS
 *       rp -- The RenderPort to draw on
 *       startX -- X coordinate of the line start point
 *       startY -- Y coordinate of the line start point
 *       endX -- X coordinate of the line end point
 *       endY -- Y coordinate of the line end point
 *       color -- 32-bit ARGB color value
 *
 ***************************************************************************/

AROS_LH4(VOID, ZuneDrawLineAA, AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct ZunePoint *, start, A1),
         AROS_LHA(struct ZunePoint *, end, A2),
         AROS_LHA(ULONG, color, D0),
         struct Library *, ZuneRendererBase, 67, zunerenderer)

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

  ZUNE_BACKEND_CALL(rp, DrawLine, startX, startY, endX, endY, (UWORD)1,
                    &internal_color, TRUE);

  AROS_LIBFUNC_EXIT
}

/****** zunerenderer.library/ZuneDrawLineStyledAA ***************************
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
         struct Library *, ZuneRendererBase, 68, zunerenderer)
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
