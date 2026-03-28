/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneCreateCircleRegion
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
AROS_LH2(struct Region *, ZuneCreateCircleRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct ZunePoint *, center, A0),
         AROS_LHA(WORD, radius, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 93, zunegfx)

/*  FUNCTION
    Creates a circular region for clipping operations.

INPUTS
    center_x - X coordinate of circle center
    center_y - Y coordinate of circle center
    radius - Radius of the circle in pixels

RESULT
    A new Region structure representing the circle, or NULL if allocation failed.
    The caller is responsible for freeing the region with DisposeRegion().

NOTES
    The circle is approximated using horizontal line segments for compatibility
    with the AROS Region system.

SEE ALSO
    ZuneCreateRoundedRectRegion(), DisposeRegion()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct Region *region;
  struct Rectangle rect;
  WORD y, x_width;

  ENTER_FUNCTION("ZuneCreateCircleRegion");

  if (!center || radius <= 0) {
    D(bug("ZuneGfx: Invalid parameters for circle region (center=%p, radius=%d)\n", center, radius));
    EXIT_FUNCTION("ZuneCreateCircleRegion");
    return NULL;
  }

  const WORD center_x = center->x;
  const WORD center_y = center->y;

  region = NewRegion();
  if (!region) {
    D(bug("ZuneGfx: Failed to create circle region\n"));
    EXIT_FUNCTION("ZuneCreateCircleRegion");
    return NULL;
  }

  /* Build circle using horizontal line segments */
  for (y = -radius; y <= radius; y++) {
    /* Calculate half-width at this y coordinate using Pythagorean theorem */
    LONG y_squared = (LONG)y * y;
    LONG radius_squared = (LONG)radius * radius;

    if (y_squared <= radius_squared) {
      /* Use integer square root approximation */
      LONG x_width_squared = radius_squared - y_squared;
      x_width = 0;

      /* Simple integer square root */
      while ((LONG)x_width * x_width < x_width_squared) {
        x_width++;
      }
      if ((LONG)x_width * x_width > x_width_squared && x_width > 0) {
        x_width--;
      }

      if (x_width > 0) {
        rect.MinX = center_x - x_width;
        rect.MaxX = center_x + x_width;
        rect.MinY = center_y + y;
        rect.MaxY = center_y + y;

        if (!OrRectRegion(region, &rect)) {
          D(bug("ZuneGfx: Failed to add rectangle to circle region\n"));
          DisposeRegion(region);
          EXIT_FUNCTION("ZuneCreateCircleRegion");
          return NULL;
        }
      }
    }
  }

  D(bug("ZuneGfx: Created circle region center=(%d,%d) radius=%d\n",
        center_x, center_y, radius));

  EXIT_FUNCTION("ZuneCreateCircleRegion");
  return region;

  AROS_LIBFUNC_EXIT
}
