#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "backends/backend_interface.h"
#include "inline/exec.h"
#include "inline/graphics.h"
#include "zunerenderer_intern.h"

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneSetClipRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct Region *, region, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 28, zunerenderer)

/*  FUNCTION
    Sets a clipping region for a RenderPort.

INPUTS
    rp - RenderPort to set clipping for
    region - Clipping region to be installed

RESULT
    None

NOTES
    All subsequent drawing operations will be clipped to this region.
    The region defines the area where drawing is allowed.

SEE ALSO
    ZuneClearClipRegion(), ZuneCombineRegions()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneSetClipRegion");

  if (!ValidateRenderPort(rp)) {
    D(bug("ZuneRenderer: Invalid RenderPort\n"));
    EXIT_FUNCTION("ZuneSetClipRegion");
    return;
  }

  if (!region) {
    D(bug("ZuneRenderer: Invalid region\n"));
    EXIT_FUNCTION("ZuneSetClipRegion");
    return;
  }

  /* Clear existing region if any */
  if (rp->clip_region) {
    DisposeRegion(rp->clip_region);
    rp->clip_region = NULL;
  }

  /* Create new region and copy input */
  rp->clip_region = NewRegion();
  if (!rp->clip_region) {
    D(bug("ZuneRenderer: Failed to allocate new region\n"));
    rp->clipping_enabled = FALSE;
    EXIT_FUNCTION("ZuneSetClipRegion");
    return;
  }

  if (OrRegionRegion(region, rp->clip_region)) {
    rp->clipping_enabled = TRUE;
    D(bug("ZuneRenderer: Clipping region installed successfully\n"));
  } else {
    D(bug("ZuneRenderer: Failed to copy region\n"));
    DisposeRegion(rp->clip_region);
    rp->clip_region = NULL;
    rp->clipping_enabled = FALSE;
  }

  EXIT_FUNCTION("ZuneSetClipRegion");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneClearClipRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 29, zunerenderer)

/*  FUNCTION
    Clears the clipping region for a RenderPort, disabling clipping.

INPUTS
    rp - RenderPort to clear clipping for

RESULT
    None

NOTES
    This function automatically frees the existing region if present.
    After calling this function, all drawing operations will be unclipped.

SEE ALSO
    ZuneSetClipRegion(), ZuneCombineRegions()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneClearClipRegion");

  if (!ValidateRenderPort(rp)) {
    EXIT_FUNCTION("ZuneClearClipRegion");
    return;
  }

  rp->clipping_enabled = FALSE;
  if (rp->clip_region) {
    DisposeRegion(rp->clip_region);
    rp->clip_region = NULL;
  }

  D(bug("ZuneRenderer: Clipping disabled\n"));

  EXIT_FUNCTION("ZuneClearClipRegion");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(struct Region *, ZuneCreateCircleRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct ZunePoint *, center, A0),
         AROS_LHA(WORD, radius, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 92, zunerenderer)

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
    ZuneCreateRoundedRectRegion(), ZuneCreateEllipseRegion(), DisposeRegion()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct Region *region;
  struct Rectangle rect;
  WORD y, x_width;

  ENTER_FUNCTION("ZuneCreateCircleRegion");

  if (!center || radius <= 0) {
    D(bug("ZuneRenderer: Invalid parameters for circle region (center=%p, radius=%d)\n", center, radius));
    EXIT_FUNCTION("ZuneCreateCircleRegion");
    return NULL;
  }

  const WORD center_x = center->x;
  const WORD center_y = center->y;

  region = NewRegion();
  if (!region) {
    D(bug("ZuneRenderer: Failed to create circle region\n"));
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
          D(bug("ZuneRenderer: Failed to add rectangle to circle region\n"));
          DisposeRegion(region);
          EXIT_FUNCTION("ZuneCreateCircleRegion");
          return NULL;
        }
      }
    }
  }

  D(bug("ZuneRenderer: Created circle region center=(%d,%d) radius=%d\n",
        center_x, center_y, radius));

  EXIT_FUNCTION("ZuneCreateCircleRegion");
  return region;

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(struct Region *, ZuneCreateRoundedRectRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneRect *, rect_input, A0),
         AROS_LHA(WORD, corner_radius, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 93, zunerenderer)

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

/*****************************************************************************

    NAME */
AROS_LH2(BOOL, ZuneCombineRegions,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),
         AROS_LHA(struct Region *, region, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 30, zunerenderer)

/*  FUNCTION
    Combines a region with the RenderPort's existing clipping region using OR operation.

INPUTS
    rp - RenderPort to modify clipping for
    region - Region to combine with existing clipping region

RESULT
    TRUE if successful, FALSE if operation failed or invalid parameters.

NOTES
    If the RenderPort has no existing clipping region, the provided region
    becomes the new clipping region. The operation uses OR logic to combine
    the regions, expanding the clippable area.

SEE ALSO
    ZuneSetClipRegion(), ZuneClearClipRegion()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderPort(rp))
    return FALSE;

  if (!region)
    return FALSE;

  if (!rp->clip_region) {
    /* No existing region, create a new one by copying the input */
    rp->clip_region = NewRegion();
    if (!rp->clip_region)
      return FALSE;
    if (!OrRegionRegion(region, rp->clip_region)) {
      DisposeRegion(rp->clip_region);
      rp->clip_region = NULL;
      return FALSE;
    }
    rp->clipping_enabled = TRUE;
  } else {
    /* Combine with existing region */
    return !OrRegionRegion(region, rp->clip_region);
  }

  D(bug("ZuneRenderer: Combined region with RenderPort clipping\n"));

  return TRUE;

  AROS_LIBFUNC_EXIT
}
