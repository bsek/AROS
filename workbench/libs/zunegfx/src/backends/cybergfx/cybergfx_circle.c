/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - CyberGraphics Circle Drawing Implementation

    This file implements circle drawing functions for the CyberGraphics backend,
    including Bresenham circle algorithm and optimized drawing paths for both
    locked and unlocked DrawingBoards with comprehensive clipping support.
*/

#include "clib/cybergraphics_protos.h"
#include "cybergfx_brush_sampler.h"
#include "cybergfx_pixel_format.h"
#include "graphics/rastport.h"
#include "libraries/zunegfx.h"
#define DEBUG 0
#include <aros/debug.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>

#include "../../zunegfx_intern.h"
#include "../backend_interface.h"
#include "cybergfx_backend.h"
#include "cybergfx_simd.h"
#include "cybergfx_antialiasing.h"

/* Forward declarations for antialiasing functions */
static void CybergfxDrawAACircleDrawingBoard(struct RenderContext *rctx, UWORD x, UWORD y, UWORD radius, struct ZuneBrush *fill_brush,
                                             struct InternalColor *border_color, UBYTE border_width);
static void CybergfxDrawAACircleRastPort(struct RenderContext *rctx, UWORD x, UWORD y, UWORD radius, struct ZuneBrush *fill_brush,
                                         struct InternalColor *border_color, UBYTE border_width);

/*****************************************************************************
 * CybergfxDrawCircle
 *
 * Circle drawing implementation with locked DrawingBoard support
 *
 * This function handles both locked and unlocked DrawingBoards:
 * - For locked DrawingBoards: Uses direct pixel manipulation for optimal
 *   performance
 * - For unlocked DrawingBoards: Uses standard FillPixelArray operations
 * - For RastPorts: Uses standard screen rendering operations
 * - Outline circles use CybergfxDrawPixel which already handles locked boards
 *   correctly
 *
 * Basic Bresenham circle implementation with comprehensive clipping support.
 *****************************************************************************/
void CybergfxDrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y, UWORD radius, UBYTE border_width, struct ZuneBrush *fill_brush,
                        struct InternalColor *border_color, BOOL filled, BOOL antialias) {
    ULONG fill_color_argb = 0;

    ENTER_FUNCTION("CybergfxDrawCircle");

    if (!rctx) {
        EXIT_FUNCTION("CybergfxDrawCircle");
        return;
    }

    /* Quick clipping check - test if circle bounding box intersects with clipping
     * region */
    WORD circle_left = center_x - radius;
    WORD circle_top = center_y - radius;
    WORD circle_width = 2 * radius + 1;
    WORD circle_height = 2 * radius + 1;

    WORD clipped_x, clipped_y, clipped_width, clipped_height;
    if (!CybergfxClipRectangle(rctx, circle_left, circle_top, circle_width, circle_height, &clipped_x, &clipped_y, &clipped_width, &clipped_height)) {
        /* Circle is completely outside clipping region */
        EXIT_FUNCTION("CybergfxDrawCircle");
        return;
    }

    if (antialias) {
        if (rctx->target_board) {
            if (rctx->target_board->pixels_locked) {
                return CybergfxDrawAACircleDrawingBoard(rctx, center_x, center_y, radius, fill_brush, border_color, border_width);
            } else {
                return CybergfxDrawAACircleRastPort(rctx, center_x, center_y, radius, fill_brush, border_color, border_width);
            }
        } else {
            return CybergfxDrawAACircleRastPort(rctx, center_x, center_y, radius, fill_brush, border_color, border_width);
        }
    }

    if (filled) {
        /* Use scanline flood-fill approach */
        if (!fill_brush) {
            EXIT_FUNCTION("CybergfxDrawCircle");
            return;
        }

        /* data is a union -- only SOLID actually holds a colour here. The
         * non-AA circle has no gradient/texture rasteriser, so rather than
         * reinterpret another union member as a colour, skip the fill. */
        if (fill_brush->type != ZUNE_BRUSH_TYPE_SOLID) {
            D(bug("CybergfxDrawCircle: brush type %ld unsupported for non-AA "
                  "fill, skipping\n", (LONG)fill_brush->type));
            EXIT_FUNCTION("CybergfxDrawCircle");
            return;
        }
        fill_color_argb = fill_brush->data.solid.color;

        UWORD board_width = rctx->target_board ? rctx->target_board->width : 640;
        UWORD board_height = rctx->target_board ? rctx->target_board->height : 480;

        WORD min_x = fmaxf(0, center_x - radius - 1);
        WORD min_y = fmaxf(0, center_y - radius - 1);
        WORD max_x = fminf(board_width - 1, center_x + radius + 1);
        WORD max_y = fminf(board_height - 1, center_y + radius + 1);

        if (min_x >= board_width || min_y >= board_height || max_x < 0 || max_y < 0) {
            EXIT_FUNCTION("CybergfxDrawCircle");
            return;
        }

        if (rctx->target_board && rctx->target_board->pixels_locked && rctx->target_board->pixels) {
            /* Direct pixel manipulation for locked DrawingBoard */
            struct DrawingBoard *board = rctx->target_board;
            if (board->pixel_layout.valid) {
                ULONG *pixels = (ULONG *)board->pixels;
                ULONG pitch_pixels = board->pitch / 4;
                ULONG converted_color = pack_pixel(&board->pixel_layout,
                                                   ZUNE_GET_ALPHA(fill_color_argb),
                                                   ZUNE_GET_RED(fill_color_argb),
                                                   ZUNE_GET_GREEN(fill_color_argb),
                                                   ZUNE_GET_BLUE(fill_color_argb));

                if (pitch_pixels == 0) {
                    EXIT_FUNCTION("CybergfxDrawCircle");
                    return;
                }

                LONG radius_sq = (LONG)radius * (LONG)radius;
                for (WORD py = min_y; py <= max_y; py++) {
                    WORD dy = py - center_y;
                    LONG dy_sq = dy * dy;
                    if (dy_sq > radius_sq)
                        continue;

                    WORD line_x = (WORD)sqrtf(radius_sq - dy_sq);
                    WORD start_x = center_x - line_x;
                    WORD end_x = center_x + line_x;

                    if (start_x <= end_x) {
                        CybergfxClipFillPixelArrayDirect(rctx, pixels, pitch_pixels, board_width, board_height, start_x, py,
                                                         end_x - start_x + 1, 1, converted_color);
                    }
                }
            }
        } else {
            /* Use scanline approach with FillPixelArray for unlocked boards */
            struct RastPort *rastport = NULL;
            if (rctx->target_board && rctx->target_board->rastport) {
                rastport = rctx->target_board->rastport;
            } else {
                rastport = rctx->target_rastport;
            }

            for (WORD py = min_y; py <= max_y; py++) {
                WORD dy = py - center_y;
                LONG dy_sq = dy * dy;
                LONG radius_sq = (LONG)radius * (LONG)radius;

                if (dy_sq > radius_sq)
                    continue;

                /* Find start and end of scanline for this y */
                WORD line_x = (WORD)sqrtf(radius_sq - dy_sq);
                WORD start_x = center_x - line_x;
                WORD end_x = center_x + line_x;

                if (start_x < min_x)
                    start_x = min_x;
                if (end_x > max_x)
                    end_x = max_x;

                if (start_x <= end_x) {
                    CybergfxClipFillPixelArray(rctx, rastport, start_x, py, end_x - start_x + 1, 1, fill_color_argb);
                }
            }
        }
    } else {
        // Draw circle outline

        if (!border_color) {
            EXIT_FUNCTION("CybergfxDrawCircle");
            return;
        }

        UWORD board_width = rctx->target_board ? rctx->target_board->width : 640;
        UWORD board_height = rctx->target_board ? rctx->target_board->height : 480;

        WORD min_x = fmaxf(0, center_x - radius - border_width - 1);
        WORD min_y = fmaxf(0, center_y - radius - border_width - 1);
        WORD max_x = fminf(board_width - 1, center_x + radius + border_width + 1);
        WORD max_y = fminf(board_height - 1, center_y + radius + border_width + 1);

        if (min_x >= board_width || min_y >= board_height || max_x < 0 || max_y < 0) {
            EXIT_FUNCTION("CybergfxDrawCircle");
            return;
        }

        float half_border = border_width * 0.5f;
        LONG radius_sq = (LONG)radius * (LONG)radius;

        if (rctx->target_board && rctx->target_board->pixels_locked && rctx->target_board->pixels) {
            /* Direct pixel manipulation for locked DrawingBoard */
            struct DrawingBoard *board = rctx->target_board;
            if (board->pixel_layout.valid) {
                ULONG *pixels = (ULONG *)board->pixels;
                ULONG pitch_pixels = board->pitch / 4;
                ULONG color = pack_pixel(&board->pixel_layout, border_color->a, border_color->r, border_color->g, border_color->b);

                for (WORD py = min_y; py <= max_y; py++) {
                    for (WORD px = min_x; px <= max_x; px++) {
                        WORD dx = px - center_x;
                        WORD dy = py - center_y;
                        float dist_sq = dx * dx + dy * dy;
                        float dist = sqrtf(dist_sq);
                        float diff = fabsf(dist - radius);

                        if (diff <= half_border) {
                            CybergfxClipFillPixelArrayDirect(rctx, pixels, pitch_pixels, board_width, board_height, px, py, 1, 1, color);
                        }
                    }
                }
            }
        } else {
            /* Fallback using Bresenham for unlocked boards */
            WORD r = (WORD)radius;
            WORD x = 0;
            WORD y = r;
            WORD d = 3 - 2 * r;
            WORD line_width = (WORD)(border_width + 0.5f);
            if (line_width < 1)
                line_width = 1;

            struct RastPort *rastport = NULL;
            if (rctx->target_board && rctx->target_board->rastport) {
                rastport = rctx->target_board->rastport;
            } else {
                rastport = rctx->target_rastport;
            }

            while (x <= y) {
                if (line_width == 1) {
                    WriteRGBPixel(rastport, center_x + x, center_y + y, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x - x, center_y + y, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x + x, center_y - y, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x - x, center_y - y, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x + y, center_y + x, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x - y, center_y + x, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x + y, center_y - x, border_color->original_pixel);
                    WriteRGBPixel(rastport, center_x - y, center_y - x, border_color->original_pixel);
                } else {
                    /* Thick outline using small rectangles */
                    WORD half_width = line_width / 2;
                    for (WORD dx = -half_width; dx <= half_width; dx++) {
                        for (WORD dy = -half_width; dy <= half_width; dy++) {
                            WriteRGBPixel(rastport, center_x + x + dx, center_y + y + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x - x + dx, center_y + y + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x + x + dx, center_y - y + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x - x + dx, center_y - y + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x + y + dx, center_y + x + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x - y + dx, center_y + x + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x + y + dx, center_y - x + dy, border_color->original_pixel);
                            WriteRGBPixel(rastport, center_x - y + dx, center_y - x + dy, border_color->original_pixel);
                        }
                    }
                }

                if (d < 0) {
                    d = d + 4 * x + 6;
                } else {
                    d = d + 4 * (x - y) + 10;
                    y--;
                }
                x++;
            }
        }
    }

    EXIT_FUNCTION("CybergfxDrawCircle");
}

/********************************************************************************************
 * Anti-aliasing functions
 ********************************************************************************************/

/*
 * One row of an antialiased circle, on pixels in layout dl. The coverage model
 * is the rounded rectangle's, and the GPU draws a circle as one: exact distance
 * divided by the pixel's footprint along the radius, a linear ramp, the border
 * inside the edge at full width and the fill running under it.
 * Returns TRUE if any pixel changed.
 */
static BOOL CybergfxAACircleRow(ULONG *row, WORD n, float rel_x0, float rel_y, float radius, float border_width,
                                const struct InternalColor *fill, const struct InternalColor *border,
                                const struct ZunePixelLayout *dl) {
    BOOL hasFill = fill != NULL;
    BOOL hasBorder = border != NULL && border_width > 0.0f;
    float fill_a = hasFill ? fill->a / 255.0f : 0.0f;
    float border_a = hasBorder ? border->a / 255.0f : 0.0f;
    BOOL any = FALSE;

    for (WORD px = 0; px < n; px += 4) {
        WORD valid = (n - px < 4) ? n - px : 4;
        float dist_outer[4], dist_inner[4], alphaFill[4], alphaLine[4];
        ULONG bg[4] = { 0, 0, 0, 0 }, result[4];
        ULONG changed;
        BOOL covered = FALSE;

        for (WORD i = 0; i < 4; i++) {
            float rx = rel_x0 + px + i;
            float foot = cybergfx_rr_footprint(rx, rel_y, radius, radius, radius);

            dist_outer[i] = (sqrtf(rx * rx + rel_y * rel_y) - radius) / foot;
            dist_inner[i] = dist_outer[i] + border_width / foot;
            if (dist_outer[i] < cybergfx_aa_smoothness)
                covered = TRUE;
        }
        if (!covered)
            continue;

        cybergfx_compute_alphas_batch4(dist_inner, dist_outer, -cybergfx_aa_smoothness, cybergfx_aa_smoothness,
                                       hasFill, hasBorder, alphaFill, alphaLine);
        if (hasFill && hasBorder) {
            cybergfx_fill_under_border(alphaFill, alphaLine, 4, border_a);
        }

        for (WORD i = 0; i < valid; i++)
            bg[i] = row[px + i];

        cybergfx_blend_aa_pixels_batch4(bg, alphaFill, alphaLine,
                                        hasFill ? fill->r : 0, hasFill ? fill->g : 0, hasFill ? fill->b : 0, fill_a,
                                        hasBorder ? border->r : 0, hasBorder ? border->g : 0, hasBorder ? border->b : 0,
                                        border_a, hasFill, hasBorder, result, &changed, dl);

        for (WORD i = 0; i < valid; i++) {
            if (changed & (1u << i)) {
                row[px + i] = result[i];
                any = TRUE;
            }
        }
    }

    return any;
}

/* Draw filled circle with border using SDF antialiasing */
static void CybergfxDrawAACircleDrawingBoard(struct RenderContext *rctx, UWORD x, UWORD y, UWORD radius, struct ZuneBrush *fill_brush,
                                             struct InternalColor *border_color, UBYTE border_width) {
    ENTER_FUNCTION("CybergfxDrawAACircleDrawingBoard");

    struct DrawingBoard *board = rctx->target_board;

    if (!board || !board->pixels) {
        EXIT_FUNCTION("CybergfxDrawAACircleDrawingBoard");
        return;
    }

    /* Writes 32-bit words straight into the locked buffer */
    if (!board->pixel_layout.valid) {
        D(bug("CybergfxDrawAACircleDrawingBoard: no direct layout for pixfmt %lu\n",
              (unsigned long)board->pixel_format));
        EXIT_FUNCTION("CybergfxDrawAACircleDrawingBoard");
        return;
    }

    if (fill_brush == NULL && border_color == NULL) {
        EXIT_FUNCTION("CybergfxDrawAACircleDrawingBoard");
        return;
    }

    if (fill_brush) {
      PrepareBrushForRendering(rctx, fill_brush, x, y, radius * 2, radius * 2);
    }

    /* Convert fill brush to internal color once before the loop */
    struct InternalColor fill_color_storage;
    struct InternalColor *fill_color = NULL;
    if (fill_brush && ZuneBrushToInternalColor(rctx, fill_brush, &fill_color_storage)) {
        fill_color = &fill_color_storage;
    }

    /* Bounding box, padded for the antialiasing band */
    WORD min_x = fmaxf(0, cybergfx_fast_ftoi(x - radius - 2.0f));
    WORD min_y = fmaxf(0, cybergfx_fast_ftoi(y - radius - 2.0f));
    WORD max_x = fminf(board->width - 1, cybergfx_fast_ftoi(x + radius + 2.0f));
    WORD max_y = fminf(board->height - 1, cybergfx_fast_ftoi(y + radius + 2.0f));
    WORD width = max_x - min_x + 1;

    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;

    /* The box is clamped to the board, so rows are processed in place */
    for (WORD py = min_y; width > 0 && py <= max_y; py++) {
        CybergfxAACircleRow(pixels + (ULONG)py * pitch_pixels + min_x, width, (min_x + 0.5f) - x, (py + 0.5f) - y,
                            radius, border_width, fill_color, border_color, &board->pixel_layout);
    }

    EXIT_FUNCTION("CybergfxDrawAACircleDrawingBoard");
}

static void CybergfxDrawAACircleRastPort(struct RenderContext *rctx, UWORD x, UWORD y, UWORD radius, struct ZuneBrush *fill_brush,
                                         struct InternalColor *border_color, UBYTE border_width) {
    ENTER_FUNCTION("CybergfxDrawAACircleRastPort");

    if (!rctx) {
        EXIT_FUNCTION("CybergfxDrawAACircleRastPort");
        return;
    }

    if (fill_brush == NULL && border_color == NULL) {
        EXIT_FUNCTION("CybergfxDrawAACircleRastPort");
        return;
    }

    struct RastPort *rp = rctx->target_rastport;

    /* Get bitmap dimensions */
    ULONG bitmap_width = GetBitMapAttr(rp->BitMap, BMA_WIDTH);
    ULONG bitmap_height = GetBitMapAttr(rp->BitMap, BMA_HEIGHT);

    /* Bounding box, padded for the antialiasing band */
    WORD min_x = fmaxf(0, cybergfx_fast_ftoi(x - radius - 2.0f));
    WORD min_y = fmaxf(0, cybergfx_fast_ftoi(y - radius - 2.0f));
    WORD max_x = fminf(bitmap_width - 1, cybergfx_fast_ftoi(x + radius + 2.0f));
    WORD max_y = fminf(bitmap_height - 1, cybergfx_fast_ftoi(y + radius + 2.0f));

    D(bug("CybergfxDrawAACircleRastPort: bounding box (%d,%d) to (%d,%d)\n", min_x, min_y, max_x, max_y));

    /* Convert fill brush to internal color once before the loop */
    struct InternalColor fill_color_storage;
    struct InternalColor *fill_color = NULL;
    if (fill_brush && ZuneBrushToInternalColor(rctx, fill_brush, &fill_color_storage)) {
        fill_color = &fill_color_storage;
    }

    /* Use multi-scanline batching to reduce ReadPixelArray/WritePixelArray syscall overhead */
    #define SCANLINE_BATCH 32
    WORD width = max_x - min_x + 1;
    WORD total_height = max_y - min_y + 1;
    float rel_x0 = (min_x + 0.5f) - x;

    ULONG *batch_buffer = (width > 0 && total_height > 0)
                             ? malloc((ULONG)width * SCANLINE_BATCH * sizeof(ULONG))
                             : NULL;

    /* Track which rows in the batch are dirty */
    BOOL batch_dirty[SCANLINE_BATCH];

    /* Process rows in batches */
    for (WORD batch_start_y = min_y; width > 0 && batch_start_y <= max_y; batch_start_y += SCANLINE_BATCH) {
        WORD batch_height = (batch_start_y + SCANLINE_BATCH <= max_y + 1)
                              ? SCANLINE_BATCH
                              : (max_y - batch_start_y + 1);

        /* Read entire batch from raster port */
        BOOL batch_ok = FALSE;
        if (batch_buffer) {
            batch_ok = ReadPixelArray(batch_buffer, 0, 0, (ULONG)width * 4, rp,
                                      min_x, batch_start_y, width, batch_height, CYBERGFX_PIXELFORMAT_ARGB32);
        }

        BOOL any_dirty = FALSE;

        for (WORD row_in_batch = 0; row_in_batch < batch_height; ++row_in_batch) {
            WORD py = batch_start_y + row_in_batch;
            float rel_y = (py + 0.5f) - y;

            if (batch_ok) {
                batch_dirty[row_in_batch] =
                    CybergfxAACircleRow(batch_buffer + (row_in_batch * width), width, rel_x0, rel_y, radius,
                                        border_width, fill_color, border_color, &zunegfx_logical_layout);
                any_dirty |= batch_dirty[row_in_batch];
                continue;
            }

            /* No batch buffer: four pixels at a time through ReadRGBPixel */
            for (WORD px = 0; px < width; px += 4) {
                WORD valid = (width - px < 4) ? width - px : 4;
                ULONG chunk[4], orig[4];

                for (WORD i = 0; i < valid; i++)
                    chunk[i] = orig[i] = ReadRGBPixel(rp, min_x + px + i, py);
                if (CybergfxAACircleRow(chunk, valid, rel_x0 + px, rel_y, radius, border_width, fill_color,
                                        border_color, &zunegfx_logical_layout)) {
                    for (WORD i = 0; i < valid; i++) {
                        if (chunk[i] != orig[i])
                            WriteRGBPixel(rp, min_x + px + i, py, chunk[i]);
                    }
                }
            }
        }

        /* Write back dirty rows - find contiguous dirty regions for optimal writes */
        if (batch_ok && any_dirty) {
            WORD write_start = -1;
            for (WORD i = 0; i <= batch_height; ++i) {
                BOOL is_dirty = (i < batch_height) && batch_dirty[i];
                if (is_dirty && write_start < 0) {
                    write_start = i;
                } else if (!is_dirty && write_start >= 0) {
                    /* Write contiguous dirty region */
                    WORD write_height = i - write_start;
                    WritePixelArray(batch_buffer + (write_start * width), 0, 0, (ULONG)width * 4,
                                    rp, min_x, batch_start_y + write_start, width, write_height,
                                    CYBERGFX_PIXELFORMAT_ARGB32);
                    write_start = -1;
                }
            }
        }
    }

    if (batch_buffer) {
        free(batch_buffer);
    }
    #undef SCANLINE_BATCH

    EXIT_FUNCTION("CybergfxDrawAACircleRastPort");
}
