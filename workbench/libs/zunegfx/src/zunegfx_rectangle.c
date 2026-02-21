/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Core Drawing Implementation

    This file implements the main drawing API functions using the unified
    target system. All drawing operations go through this layer which
    dispatches to the appropriate backend.
*/
#define DEBUG 0
#include "backends/backend_interface.h"
#include "batching/batching_intern.h"
#include "zunegfx_intern.h"
#include <aros/debug.h>
#include <aros/libcall.h>
/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneFillRectangle,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 36, zunegfx)

/*  FUNCTION
    Draws a filled rectangle.

INPUTS
    rctx - RenderContext
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

  if (!rctx || !rect) {
    D(bug("DrawRectangle: Invalid parameters (rctx=%p, rect=%p)\n", rctx, rect));
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

  /* Batch solid-color fills when batching is active */
  if (rctx->batching_enabled && rctx->batch_state && brush &&
      brush->type == ZUNE_BRUSH_TYPE_SOLID) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddCommandToBatch(batch, BATCH_CMD_FILL_RECT, x, y, width, height, 0,
                           0, brush->data.solid.color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_FILL_RECT, x, y, width, height, 0, 0,
                        brush->data.solid.color);
    }
    EXIT_FUNCTION("DrawRectangle");
    return;
  }

  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, 0, 0, brush,
                    NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawRectangle");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneFillRectangleRounded,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(struct ZuneBrush *, brush, A2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 37, zunegfx)

/*  FUNCTION
    Draws a filled rounded rectangle.

INPUTS
    rctx - RenderContext
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

  if (!rctx || !rect) {
    D(bug("DrawRectangleRounded: Invalid parameters (rctx=%p, rect=%p)\n", rctx,
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

  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)0, cornerRadius, brush, NULL, TRUE, FALSE);

  EXIT_FUNCTION("DrawRectangleRounded");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneDrawRectangleOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 41, zunegfx)

/*  FUNCTION
    Draws a rectangle outline.

INPUTS
    rctx - RenderContext
    x, y - Top-left corner
    width, height - Rectangle dimensions
    color - Border color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("DrawRectangleOutline");

  if (!rctx || !rect) {
    D(bug("DrawRectangleOutline: Invalid parameters (rctx=%p, rect=%p)\n", rctx,
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

  /* Batch outline rectangles when batching is active */
  if (rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddCommandToBatch(batch, BATCH_CMD_DRAW_RECT, x, y, width, height, 0,
                           0, color)) {
      ExecuteBatchCommands(batch);
      AddCommandToBatch(batch, BATCH_CMD_DRAW_RECT, x, y, width, height, 0, 0,
                        color);
    }
    EXIT_FUNCTION("DrawRectangleOutline");
    return;
  }

  struct InternalColor border_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)1, (UBYTE)0, NULL,
                    &border_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleOutline");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawRectangleRoundedOutline,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0), AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 42, zunegfx)

/*  FUNCTION
    Draws a rounded rectangle outline.

INPUTS
    rctx - RenderContext
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

  if (!rctx || !rect) {
    D(bug("DrawRectangleRoundedOutline: Invalid parameters (rctx=%p, rect=%p)\n",
          rctx, rect));
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
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, (UBYTE)1, cornerRadius,
                    NULL, &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleRoundedOutline");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawRectangleRoundedOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0), AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(ULONG, color, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 47, zunegfx)

/*  FUNCTION
    Draws a styled rounded rectangle outline.

INPUTS
    rctx - RenderContext
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

  if (!rctx || !rect) {
    D(bug("DrawRectangleRoundedOutlineStyled: Invalid parameters (rctx=%p, "
          "rect=%p)\n",
          rctx, rect));
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
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, borderWidth,
                    cornerRadius, NULL, &internal_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleRoundedOutlineStyled");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH6(void, ZuneDrawRectangleRoundedStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, cornerRadius, D0),
         AROS_LHA(UBYTE, borderWidth, D1),
         AROS_LHA(struct ZuneBrush *, fillBrush, A2),
         AROS_LHA(ULONG, borderColor, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 50, zunegfx)

/*  FUNCTION
    Draws a rounded rectangle with both fill and border.

INPUTS
    rctx - RenderContext
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

  if (!rctx || !rect) {
    D(bug("ZuneDrawRectangleRoundedStyled: Invalid parameters (rctx=%p, "
          "rect=%p)\n",
          rctx, rect));
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
      ZuneColorToInternal(rctx, borderColor, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, borderWidth,
                    cornerRadius, fillBrush, &internal_border_color, TRUE, FALSE);

  EXIT_FUNCTION("ZuneDrawRectangleRoundedStyled");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawRectangleOutlineStyled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneRect *, rect, A1),
         AROS_LHA(UBYTE, borderWidth, D0), AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 45, zunegfx)

/*  FUNCTION
    Draws a styled rectangle outline.

INPUTS
    rctx - RenderContext
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

  if (!rctx || !rect) {
    D(bug("DrawRectangleOutlineStyled: Invalid parameters (rctx=%p, rect=%p)\n",
          rctx, rect));
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

  /* Batch styled outline rectangles when batching is active */
  if (rctx->batching_enabled && rctx->batch_state) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (!AddStyledCommandToBatch(batch, BATCH_CMD_DRAW_RECT_STYLED, x, y,
                                 width, height, borderWidth, color)) {
      ExecuteBatchCommands(batch);
      AddStyledCommandToBatch(batch, BATCH_CMD_DRAW_RECT_STYLED, x, y, width,
                              height, borderWidth, color);
    }
    EXIT_FUNCTION("DrawRectangleOutlineStyled");
    return;
  }

  struct InternalColor border_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, DrawRectangle, x, y, width, height, borderWidth, 0.0,
                    NULL, &border_color, FALSE, FALSE);

  EXIT_FUNCTION("DrawRectangleOutlineStyled");

  AROS_LIBFUNC_EXIT
}
