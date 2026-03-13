/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateRoundedRectRegion
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(struct Region *, ZuneCreateRoundedRectRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneRect *, rect_input, A0),
         AROS_LHA(WORD, corner_radius, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 94, zunegfx)

/*  FUNCTION
    Creates a rounded rectangle region for clipping operations.

INPUTS
    x - Left edge of rectangle
    y - Top edge of rectangle
    w - Width of rectangle
    h - Height of rectangle
    corner_radius - Radius of rounded corners

RESULT
    A new Region structure representing the rounded rectangle, or NULL if
    allocation failed. The caller is responsible for freeing the region.

NOTES
    If corner_radius is 0 or negative, creates a regular rectangle.
    Corner radius is clamped to half the smaller dimension.

SEE ALSO
    ZuneCreateCircleRegion(), ZuneCreateEllipseRegion(), DisposeRegion()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct Region *region;
  struct Rectangle rect;
  WORD effective_radius;
  WORD corner_y, x_width;

  ENTER_FUNCTION("ZuneCreateRoundedRectRegion");

  if (!rect_input || rect_input->width == 0 || rect_input->height == 0) {
    D(bug("ZuneRenderer: Invalid rectangle for rounded rect region (%p)\n", rect_input));
    EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
    return NULL;
  }

  const WORD x = rect_input->x;
  const WORD y = rect_input->y;
  const WORD w = rect_input->width;
  const WORD h = rect_input->height;

  region = NewRegion();
  if (!region) {
    D(bug("ZuneRenderer: Failed to create rounded rectangle region\n"));
    EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
    return NULL;
  }

  /* Clamp corner radius to reasonable bounds */
  effective_radius = corner_radius;
  if (effective_radius > w / 2) effective_radius = w / 2;
  if (effective_radius > h / 2) effective_radius = h / 2;
  if (effective_radius <= 0) effective_radius = 0;

  if (effective_radius == 0) {
    /* Simple rectangle */
    rect.MinX = x;
    rect.MaxX = x + w - 1;
    rect.MinY = y;
    rect.MaxY = y + h - 1;

    if (!OrRectRegion(region, &rect)) {
      DisposeRegion(region);
      EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
      return NULL;
    }
  } else {
    /* Main rectangle (excluding rounded corners) */
    rect.MinX = x + effective_radius;
    rect.MaxX = x + w - effective_radius - 1;
    rect.MinY = y;
    rect.MaxY = y + h - 1;

    if (rect.MaxX >= rect.MinX && !OrRectRegion(region, &rect)) {
      DisposeRegion(region);
      EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
      return NULL;
    }

    /* Top and bottom strips */
    rect.MinX = x;
    rect.MaxX = x + w - 1;
    rect.MinY = y + effective_radius;
    rect.MaxY = y + h - effective_radius - 1;

    if (rect.MaxY >= rect.MinY && !OrRectRegion(region, &rect)) {
      DisposeRegion(region);
      EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
      return NULL;
    }

    /* Add rounded corners using quarter-circles */
    for (corner_y = 0; corner_y < effective_radius; corner_y++) {
      LONG y_from_center = effective_radius - corner_y;
      LONG y_squared = y_from_center * y_from_center;
      LONG radius_squared = (LONG)effective_radius * effective_radius;

      if (y_squared <= radius_squared) {
        LONG x_width_squared = radius_squared - y_squared;
        x_width = 0;

        /* Integer square root */
        while ((LONG)x_width * x_width < x_width_squared) {
          x_width++;
        }
        if ((LONG)x_width * x_width > x_width_squared && x_width > 0) {
          x_width--;
        }

        if (x_width > 0) {
          WORD corner_x = effective_radius - x_width;

          /* Top-left and top-right corners */
          rect.MinX = x + corner_x;
          rect.MaxX = x + w - corner_x - 1;
          rect.MinY = y + corner_y;
          rect.MaxY = y + corner_y;

          if (rect.MaxX >= rect.MinX && !OrRectRegion(region, &rect)) {
            DisposeRegion(region);
            EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
            return NULL;
          }

          /* Bottom-left and bottom-right corners */
          rect.MinY = y + h - corner_y - 1;
          rect.MaxY = y + h - corner_y - 1;

          if (rect.MaxX >= rect.MinX && !OrRectRegion(region, &rect)) {
            DisposeRegion(region);
            EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
            return NULL;
          }
        }
      }
    }
  }

  D(bug("ZuneRenderer: Created rounded rect region (%d,%d %dx%d) radius=%d\n",
        x, y, w, h, effective_radius));

  EXIT_FUNCTION("ZuneCreateRoundedRectRegion");
  return region;

  AROS_LIBFUNC_EXIT
}
