#include "../cybergfx_brush_sampler.h"
#include "cybergfx_rectangle_internal.h"

#include <stdlib.h>

/*****************************************************************************
 * CybergfxDrawRoundedRectangleToRasterPort
 *
 * Draws a rounded rectangle to a RastPort using CyberGraphics operations.
 * For outline-only drawing (filled=FALSE), only border pixels are written
 * to preserve the existing background.
 *****************************************************************************/
void CybergfxDrawRoundedRectangleToRasterPort(struct RenderContext *renderport, struct RastPort *rp, UWORD x, UWORD y, UWORD width, UWORD height,
                                              UBYTE border_width, struct ZuneBrush *fill_brush, ULONG border_color, UWORD border_radius,
                                              BOOL filled) {
    if (!renderport || !rp || width == 0 || height == 0) {
        return;
    }

    WORD clipped_x = x;
    WORD clipped_y = y;
    WORD clipped_width = width;
    WORD clipped_height = height;

    if (!CybergfxClipRectangle(renderport, x, y, width, height, &clipped_x, &clipped_y, &clipped_width, &clipped_height)) {
        return;
    }

    WORD r = (WORD)border_radius;
    if (r > (WORD)(width / 2)) {
        r = (WORD)(width / 2);
    }
    if (r > (WORD)(height / 2)) {
        r = (WORD)(height / 2);
    }

    if (filled && (!fill_brush || !fill_brush->internal.valid)) {
        if (fill_brush && !fill_brush->internal.valid) {
            D(bug("CybergfxDrawRoundedRectangleToRasterPort: Brush preparation failed\n"));
        }
        filled = FALSE;
    }

    BOOL draw_border = (border_width > 0);
    WORD bw = draw_border ? (WORD)border_width : 0;
    if (bw < 1 && draw_border) bw = 1;

    WORD inner_r = r - bw;
    if (inner_r < 0) inner_r = 0;

    /* Get fill color for solid brushes */
    ULONG fill_color = 0;
    BOOL solid_fill = FALSE;
    if (filled && (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID || fill_brush->type == ZUNE_BRUSH_TYPE_PEN)) {
        fill_color = fill_brush->internal.color.original_pixel;
        solid_fill = TRUE;
    }

    /* Allocate row buffer for WritePixelArray */
    ULONG *row_buffer = malloc((ULONG)width * sizeof(ULONG));
    if (!row_buffer) {
        return;
    }

    /* Allocate mask to track which pixels to write */
    UBYTE *pixel_mask = malloc((ULONG)width);
    if (!pixel_mask) {
        free(row_buffer);
        return;
    }

    /* Corner center positions - centers are at radius distance from edges */
    WORD tl_cx = x + r;              /* Top-left */
    WORD tl_cy = y + r;
    WORD tr_cx = x + width - 1 - r;  /* Top-right */
    WORD tr_cy = y + r;
    WORD bl_cx = x + r;              /* Bottom-left */
    WORD bl_cy = y + height - 1 - r;
    WORD br_cx = x + width - 1 - r;  /* Bottom-right */
    WORD br_cy = y + height - 1 - r;

    LONG r_sq = r * r;
    LONG inner_r_sq = inner_r * inner_r;

    /* Process each scanline */
    for (WORD py = y; py < (WORD)(y + height); py++) {
        /* Clear mask for this row */
        for (WORD i = 0; i < (WORD)width; i++) {
            pixel_mask[i] = 0;
        }

        for (WORD px = x; px < (WORD)(x + width); px++) {
            WORD buf_x = px - x;
            BOOL is_border = FALSE;
            BOOL is_fill = FALSE;

            /* Determine which region this pixel is in */
            BOOL in_corner = FALSE;
            LONG dist_sq = 0;

            /* Check if we're in a corner region */
            if (px < tl_cx && py < tl_cy) {
                /* Top-left corner */
                WORD dx = px - tl_cx;
                WORD dy = py - tl_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            } else if (px > tr_cx && py < tr_cy) {
                /* Top-right corner */
                WORD dx = px - tr_cx;
                WORD dy = py - tr_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            } else if (px < bl_cx && py > bl_cy) {
                /* Bottom-left corner */
                WORD dx = px - bl_cx;
                WORD dy = py - bl_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            } else if (px > br_cx && py > br_cy) {
                /* Bottom-right corner */
                WORD dx = px - br_cx;
                WORD dy = py - br_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            }

            if (in_corner) {
                /* In corner region - check distance from corner center */
                if (dist_sq < r_sq) {
                    /* Inside the rounded corner */
                    if (draw_border && dist_sq >= inner_r_sq) {
                        is_border = TRUE;
                    } else if (filled) {
                        is_fill = TRUE;
                    }
                }
                /* else: outside the corner curve, pixel stays transparent/unchanged */
            } else {
                /* In straight edge or center region */
                /* Check if in border region */
                if (draw_border) {
                    if (py < y + bw || py >= y + height - bw ||
                        px < x + bw || px >= x + width - bw) {
                        is_border = TRUE;
                    } else if (filled) {
                        is_fill = TRUE;
                    }
                } else if (filled) {
                    is_fill = TRUE;
                }
            }

            /* Set pixel color based on region */
            if (is_border) {
                row_buffer[buf_x] = border_color;
                pixel_mask[buf_x] = 1;
            } else if (is_fill) {
                if (solid_fill) {
                    row_buffer[buf_x] = fill_color;
                } else {
                    /* Sample brush for gradient/pattern */
                    UBYTE fc_r, fc_g, fc_b, fc_a;
                    SampleBrush(fill_brush, x, y, width, height, px, py, &fc_r, &fc_g, &fc_b, &fc_a);
                    /* Use logical format for WritePixelArray with CYBERGFX_PIXELFORMAT_ARGB32 */
                    row_buffer[buf_x] = pack_argb32_logical(0xFF, fc_r, fc_g, fc_b);
                }
                pixel_mask[buf_x] = 1;
            }
        }

        /* Write contiguous segments of pixels that are set in the mask */
        WORD seg_start = -1;
        for (WORD i = 0; i <= (WORD)width; i++) {
            BOOL has_pixel = (i < (WORD)width) ? pixel_mask[i] : 0;
            
            if (has_pixel && seg_start < 0) {
                /* Start of a new segment */
                seg_start = i;
            } else if (!has_pixel && seg_start >= 0) {
                /* End of segment - write it */
                WORD seg_width = i - seg_start;
                WritePixelArray(&row_buffer[seg_start], 0, 0, (ULONG)seg_width * 4, rp,
                               x + seg_start, py, seg_width, 1, CYBERGFX_PIXELFORMAT_ARGB32);
                seg_start = -1;
            }
        }
    }

    free(pixel_mask);
    free(row_buffer);
}
