#include "../cybergfx_brush_sampler.h"
#include "cybergfx_rectangle_internal.h"

#include <stdlib.h>

#define DEBUG 0
#include <aros/debug.h>

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
  D(bug("CybergfxDrawRectangleToRasterPort: rp=%p, rp->BitMap=%p\n",
        rp, rp ? rp->BitMap : NULL));

  /* Handle filled rectangles */
  if (filled && fill_brush && width > 0 && height > 0) {
    /* Fast path for solid color brushes - use FillPixelArray with ARGB color
     * This works correctly for both window RastPorts and off-screen DrawingBoards
     * since it uses the pre-converted ARGB value instead of pen index */
    if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID || fill_brush->type == ZUNE_BRUSH_TYPE_PEN) {
      ULONG argb = fill_brush->internal.color.original_pixel;
      D(bug("CybergfxDrawRectangleToRasterPort: Using FillPixelArray with ARGB=0x%08lx\n", argb));
      FillPixelArray(rp, x, y, width, height, argb);
    } else {
      /* Pattern/gradient brushes require per-pixel sampling */
      /* Use multi-scanline batching to reduce WritePixelArray syscall overhead */
      #define SCANLINE_BATCH 32
      int row_width = (int)width;
      int total_height = (int)height;
      
      /* Allocate buffer for multiple scanlines */
      ULONG *batch_buffer =
          (row_width > 0 && total_height > 0) 
              ? malloc((size_t)row_width * SCANLINE_BATCH * sizeof(ULONG)) 
              : NULL;
      if (!batch_buffer) {
        return;
      }

      /* Process rows in batches */
      for (int batch_start = 0; batch_start < total_height; batch_start += SCANLINE_BATCH) {
        int batch_height = (batch_start + SCANLINE_BATCH <= total_height) 
                              ? SCANLINE_BATCH 
                              : (total_height - batch_start);
        
        /* Fill the batch buffer with multiple scanlines */
        for (int row_in_batch = 0; row_in_batch < batch_height; ++row_in_batch) {
          int row = batch_start + row_in_batch;
          WORD py = y + row;
          ULONG *row_buffer = batch_buffer + (row_in_batch * row_width);
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
            /* Use logical format for WritePixelArray with CYBERGFX_PIXELFORMAT_ARGB32 */
            row_buffer[px] = pack_argb32_logical(0xFF, fc_r, fc_g, fc_b);
          }
        }

        /* Write the entire batch to the raster port in one syscall */
        WritePixelArray(batch_buffer, 0, 0, (ULONG)row_width * 4, rp, x, y + batch_start,
                        row_width, batch_height, CYBERGFX_PIXELFORMAT_ARGB32);
      }

      free(batch_buffer);
      #undef SCANLINE_BATCH
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
