#include "../cybergfx_brush_sampler.h"
#include "cybergfx_rectangle_internal.h"

#include <stdlib.h>

/*****************************************************************************
 * CybergfxDrawRectangleToRasterPort
 *
 * Draws a regular rectangle to a RastPort using CyberGraphics operations.
 * This function handles both filled and outline rectangles with clipping.
 *****************************************************************************/
void CybergfxDrawRectangleToRasterPort(struct RenderPort *renderport,
                                       struct RastPort *rp, UWORD x, UWORD y,
                                       UWORD width, UWORD height,
                                       UBYTE border_width,
                                       struct ZuneBrush *fill_brush,
                                       ULONG border_color, BOOL filled) {
  /* Validate input parameters */
  if (!renderport || !rp) {
    return;
  }

  /* Clip rectangle to render port boundaries */
  WORD clipped_x = x;
  WORD clipped_y = y;
  WORD clipped_width = width;
  WORD clipped_height = height;

  if (!CybergfxClipRectangle(renderport, x, y, width, height, &clipped_x,
                             &clipped_y, &clipped_width, &clipped_height)) {
    return;
  }

  /* Update coordinates to clipped values */
  x = clipped_x;
  y = clipped_y;
  width = (UWORD)clipped_width;
  height = (UWORD)clipped_height;

  if (filled) {
    if (!fill_brush->internal.valid) {
      D(bug("Brush preperation has failed!\n"));
      filled = 0; /* Brush preparation failed */
    }
  }

  D(bug("CybergfxDrawRectangleToRasterPort: CALLED with x=%d, y=%d, w=%d, "
        "h=%d, border_width=%d, filled=%s, type=%d\n",
        x, y, width, height, border_width, filled ? "TRUE" : "FALSE",
        fill_brush ? fill_brush->type : -1));

  /* Handle filled rectangles */
  if (filled && fill_brush && width > 0 && height > 0) {
    /* Fast path for solid color brushes */
    if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID || fill_brush->type == ZUNE_BRUSH_TYPE_PEN) {
      FillPixelArray(rp, x, y, width, height, fill_brush->internal.color->original_pixel);
    } else {
      /* Pattern/gradient brushes require per-pixel sampling */
      int row_width = (int)width;
      ULONG *row_buffer =
          row_width > 0 ? malloc((size_t)row_width * sizeof(ULONG)) : NULL;
      if (!row_buffer) {
        return;
      }

      /* Process each row of the rectangle */
      for (int row = 0; row < (int)height; ++row) {
        WORD py = y + row;
        int px = 0;

        /* Process 4 pixels at a time using SIMD batch sampling */
        for (; px <= row_width - 4; px += 4) {
          int px_coords[4];
          for (int i = 0; i < 4; ++i) {
            px_coords[i] = x + px + i;
          }

          UBYTE fc_r[4], fc_g[4], fc_b[4], fc_a[4];
          SampleBrushBatch4(fill_brush, x, y, width, height, px_coords, py,
                            fc_r, fc_g, fc_b, fc_a);

          /* Write opaque pixels directly using vectorized packing (no alpha
           * blending) */
          UBYTE alpha[4] = {0xFF, 0xFF, 0xFF, 0xFF};
          cybergfx_make_argb32_batch4(alpha, fc_r, fc_g, fc_b, &row_buffer[px]);
        }

        /* Process remaining pixels one at a time */
        for (; px < row_width; ++px) {
          int actual_x = x + px;
          UBYTE fc_r, fc_g, fc_b, fc_a;
          SampleBrush(fill_brush, x, y, width, height, actual_x, py, &fc_r,
                      &fc_g, &fc_b, &fc_a);

          /* Write opaque pixel directly (no alpha blending) */
          row_buffer[px] = pack_argb32(0xFF, fc_r, fc_g, fc_b);
        }

        /* Write the entire row to the raster port */
        WritePixelArray(row_buffer, 0, 0, (ULONG)row_width * 4, rp, x, py,
                        row_width, 1, CYBERGFX_PIXELFORMAT_ARGB32);
      }

      free(row_buffer);
    }
  }

  if (border_width > 0) {
    D(bug("CybergfxDrawRectangleToRasterPort: Drawing rectangle outline with 4 "
          "edges\n"));
    /* Draw rectangle outline using FillPixelArray */
    int line_width = (int)(border_width + 0.5f); /* Round to nearest int */
    if (line_width < 1)
      line_width = 1;

    CybergfxClipFillPixelArray(renderport, rp, x, y, width, line_width,
                               border_color); /* Top edge */
    CybergfxClipFillPixelArray(renderport, rp, x, y + height - line_width,
                               width, line_width,
                               border_color); /* Bottom edge */
    CybergfxClipFillPixelArray(renderport, rp, x, y, line_width, height,
                               border_color); /* Left edge */
    CybergfxClipFillPixelArray(renderport, rp, x + width - line_width, y,
                               line_width, height,
                               border_color); /* Right edge */
    D(bug("CybergfxDrawRectangleToRasterPort: Finished drawing outline\n"));
  }
}
