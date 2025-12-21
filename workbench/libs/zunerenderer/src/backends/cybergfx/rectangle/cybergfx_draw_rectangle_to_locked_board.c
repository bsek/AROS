#include "../cybergfx_brush_sampler.h"
#include "../cybergfx_pixel_format.h"
#include "cybergfx_rectangle_internal.h"

#include <stdlib.h>

/*****************************************************************************
 * CybergfxDrawRectangleToLockedDrawingBoard
 *
 * Draws a rectangle directly to a locked DrawingBoard's pixel buffer.
 * This provides optimal performance for direct pixel access operations.
 *****************************************************************************/
void CybergfxDrawRectangleToLockedDrawingBoard(struct RenderPort *renderport,
                                               WORD x, WORD y, UWORD width,
                                               UWORD height, UBYTE border_width,
                                               struct ZuneBrush *fill_brush,
                                               ULONG border_color,
                                               BOOL filled) {
  struct DrawingBoard *board = renderport->target_board;

  /* Validate input parameters */
  if (!renderport || !board) {
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

  D(bug("CybergfxDrawRectangleToLockedDrawingBoard: CALLED with x=%d, y=%d, "
        "w=%d, "
        "h=%d, border_width=%d, filled=%s\n",
        x, y, width, height, border_width, filled ? "TRUE" : "FALSE"));

  /* Handle filled rectangles */
  if (filled && fill_brush && width > 0 && height > 0) {
    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;

    /* Fast path for solid color brushes */
    if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID || fill_brush->type == ZUNE_BRUSH_TYPE_PEN) {
      /* Use pack_argb32 for correct format when writing directly to memory */
      ULONG fill_pixel = pack_argb32(255, fill_brush->internal.color->r,
                                     fill_brush->internal.color->g,
                                     fill_brush->internal.color->b);

      CybergfxClipFillPixelArrayDirect(renderport, pixels, pitch_pixels,
                                       board->width, board->height, x, y, width,
                                       height, fill_pixel);
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
          for (int i = 0; i < 4; ++i) {
            row_buffer[px + i] = pack_argb32(alpha[i], fc_r[i], fc_g[i], fc_b[i]);
          }
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

        /* Write the entire row directly to the pixel buffer */
        ULONG *dest_row = pixels + (py * pitch_pixels) + x;
        for (int i = 0; i < row_width; ++i) {
          dest_row[i] = row_buffer[i];
        }
      }

      free(row_buffer);
    }
  }

  if (border_width > 0) {
    D(bug("CybergfxDrawRectangleToLockedDrawingBoard: Drawing rectangle "
          "outline with 4 "
          "edges\n"));
    /* Draw rectangle outline using direct pixel access */
    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;
    int line_width = (int)(border_width + 0.5f); /* Round to nearest int */
    if (line_width < 1)
      line_width = 1;

    /* Top edge */
    CybergfxClipFillPixelArrayDirect(renderport, pixels, pitch_pixels,
                                     board->width, board->height, x, y, width,
                                     line_width, border_color);
    /* Bottom edge */
    CybergfxClipFillPixelArrayDirect(
        renderport, pixels, pitch_pixels, board->width, board->height, x,
        y + height - line_width, width, line_width, border_color);
    /* Left edge */
    CybergfxClipFillPixelArrayDirect(renderport, pixels, pitch_pixels,
                                     board->width, board->height, x, y,
                                     line_width, height, border_color);
    /* Right edge */
    CybergfxClipFillPixelArrayDirect(
        renderport, pixels, pitch_pixels, board->width, board->height,
        x + width - line_width, y, line_width, height, border_color);
    D(bug("CybergfxDrawRectangleToLockedDrawingBoard: Finished drawing "
          "outline\n"));
  }
}
