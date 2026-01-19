#include "../cybergfx_brush_sampler.h"
#include "../cybergfx_pixel_format.h"
#include "cybergfx_rectangle_internal.h"

#include <stdlib.h>

/*****************************************************************************
 * CybergfxDrawRoundedRectangleToLockedDrawingBoard
 *
 * Draws a rounded rectangle to a locked DrawingBoard.
 * Fill and border are drawn in a single pass to avoid edge artifacts.
 *****************************************************************************/
void CybergfxDrawRoundedRectangleToLockedDrawingBoard(struct RenderPort *rp, UWORD x, UWORD y, UWORD width, UWORD height, UBYTE border_width,
                                                      struct ZuneBrush *fill_brush, ULONG border_color, UWORD border_radius, BOOL filled) {
    struct DrawingBoard *board = rp->target_board;

    /* Validate input parameters */
    if (!rp || !board) {
        return;
    }

    int r = (int)border_radius;
    if (r > (int)width / 2)
        r = (int)width / 2;
    if (r > (int)height / 2)
        r = (int)height / 2;

    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;

    BOOL draw_border = (border_width > 0);
    int bw = draw_border ? (int)border_width : 0;
    if (bw < 1 && draw_border) bw = 1;

    int inner_r = r - bw;
    if (inner_r < 0) inner_r = 0;

    /* Get fill color for solid brushes */
    ULONG fill_color = 0;
    BOOL solid_fill = FALSE;
    if (filled && fill_brush && (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID || fill_brush->type == ZUNE_BRUSH_TYPE_PEN)) {
        /* Use pack_argb32 for correct format when writing directly to memory */
        fill_color = pack_argb32(255, fill_brush->internal.color.r,
                                 fill_brush->internal.color.g,
                                 fill_brush->internal.color.b);
        solid_fill = TRUE;
    }

    /* Corner center positions */
    int tl_cx = x + r;              /* Top-left */
    int tl_cy = y + r;
    int tr_cx = x + width - r - 1;  /* Top-right */
    int tr_cy = y + r;
    int bl_cx = x + r;              /* Bottom-left */
    int bl_cy = y + height - r - 1;
    int br_cx = x + width - r - 1;  /* Bottom-right */
    int br_cy = y + height - r - 1;

    int r_sq = r * r;
    int inner_r_sq = inner_r * inner_r;

    /* Process each pixel */
    for (int py = y; py < (int)(y + height); py++) {
        for (int px = x; px < (int)(x + width); px++) {
            BOOL is_border = FALSE;
            BOOL is_fill = FALSE;

            /* Determine which region this pixel is in */
            BOOL in_corner = FALSE;
            int dist_sq = 0;

            /* Check if we're in a corner region */
            if (px < tl_cx && py < tl_cy) {
                /* Top-left corner */
                int dx = px - tl_cx;
                int dy = py - tl_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            } else if (px > tr_cx && py < tr_cy) {
                /* Top-right corner */
                int dx = px - tr_cx;
                int dy = py - tr_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            } else if (px < bl_cx && py > bl_cy) {
                /* Bottom-left corner */
                int dx = px - bl_cx;
                int dy = py - bl_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            } else if (px > br_cx && py > br_cy) {
                /* Bottom-right corner */
                int dx = px - br_cx;
                int dy = py - br_cy;
                dist_sq = dx * dx + dy * dy;
                in_corner = TRUE;
            }

            if (in_corner) {
                /* In corner region - check distance from corner center */
                if (dist_sq < r_sq) {
                    /* Inside the rounded corner */
                    if (draw_border && dist_sq >= inner_r_sq) {
                        is_border = TRUE;
                    } else if (filled && fill_brush) {
                        is_fill = TRUE;
                    }
                }
                /* else: outside the corner curve, skip pixel */
            } else {
                /* In straight edge or center region */
                if (draw_border) {
                    if (py < y + bw || py >= y + height - bw ||
                        px < x + bw || px >= x + width - bw) {
                        is_border = TRUE;
                    } else if (filled && fill_brush) {
                        is_fill = TRUE;
                    }
                } else if (filled && fill_brush) {
                    is_fill = TRUE;
                }
            }

            /* Write pixel based on region */
            if (is_border) {
                CybergfxWritePixelClamped(pixels, pitch_pixels, board->width, board->height, px, py, border_color);
            } else if (is_fill) {
                if (solid_fill) {
                    CybergfxWritePixelClamped(pixels, pitch_pixels, board->width, board->height, px, py, fill_color);
                } else {
                    /* Sample brush for gradient/pattern */
                    UBYTE fc_r, fc_g, fc_b, fc_a;
                    SampleBrush(fill_brush, x, y, width, height, px, py, &fc_r, &fc_g, &fc_b, &fc_a);
                    ULONG packed_pixel = pack_argb32(0xFF, fc_r, fc_g, fc_b);
                    CybergfxWritePixelClamped(pixels, pitch_pixels, board->width, board->height, px, py, packed_pixel);
                }
            }
        }
    }
}
