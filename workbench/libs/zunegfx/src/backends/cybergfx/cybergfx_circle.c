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
            if (board->pixel_format == PIXFMT_ARGB32 || board->pixel_format == PIXFMT_RGBA32) {
                ULONG *pixels = (ULONG *)board->pixels;
                ULONG pitch_pixels = board->pitch / 4;
                ULONG converted_color = fill_brush->data.solid.color;

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
                    CybergfxClipFillPixelArray(rctx, rastport, start_x, py, end_x - start_x + 1, 1, fill_brush->data.solid.color);
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
            if (board->pixel_format == PIXFMT_ARGB32 || board->pixel_format == PIXFMT_RGBA32) {
                ULONG *pixels = (ULONG *)board->pixels;
                ULONG pitch_pixels = board->pitch / 4;
                /* Use pack_argb32 for correct format when writing directly to memory */
                ULONG color = pack_argb32(border_color->a, border_color->r, border_color->g, border_color->b);

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

/* SDF function for circle */
static inline float sdf_circle(float px, float py, float cx, float cy, float radius) {
    float dx = px - cx;
    float dy = py - cy;
    return sqrtf(dx * dx + dy * dy) - radius;
}

/* Helper functions for color mixing and blending */
static inline float clamp(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

static inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline void blend_over(UBYTE *r, UBYTE *g, UBYTE *b, UBYTE cr, UBYTE cg, UBYTE cb, float a) {
    *r = (UBYTE)((1 - a) * (*r) + a * cr);
    *g = (UBYTE)((1 - a) * (*g) + a * cg);
    *b = (UBYTE)((1 - a) * (*b) + a * cb);
}

/* Extract color components from BGRA32 pixel */
static inline void extract_bgra32(ULONG pixel, UBYTE *a, UBYTE *r, UBYTE *g, UBYTE *b) {
    *b = (pixel >> 24) & 0xFF;
    *g = (pixel >> 16) & 0xFF;
    *r = (pixel >> 8) & 0xFF;
    *a = pixel & 0xFF;
}

/* Extract color components from BGRA32 pixel */
static inline void extract_argb32(ULONG pixel, UBYTE *a, UBYTE *r, UBYTE *g, UBYTE *b) {
    *a = (pixel >> 24) & 0xFF;
    *r = (pixel >> 16) & 0xFF;
    *g = (pixel >> 8) & 0xFF;
    *b = pixel & 0xFF;
}

/* Make ARGB32 pixel from components */
static inline ULONG make_argb32(UBYTE a, UBYTE r, UBYTE g, UBYTE b) { return ((ULONG)a << 24) | ((ULONG)r << 16) | ((ULONG)g << 8) | b; }

/* Make BGRA32 pixel from components */
static inline ULONG make_bgra32(UBYTE a, UBYTE r, UBYTE g, UBYTE b) { return ((ULONG)b << 24) | ((ULONG)g << 16) | ((ULONG)r << 8) | a; }

/* Fast floating point to integer conversion */
static inline LONG cybergfx_fast_ftoi(float f) { return (LONG)(f + 0.5f); }

/* Draw filled circle with border using SDF antialiasing */
static void CybergfxDrawAACircleDrawingBoard(struct RenderContext *rctx, UWORD x, UWORD y, UWORD radius, struct ZuneBrush *fill_brush,
                                             struct InternalColor *border_color, UBYTE border_width) {
    ENTER_FUNCTION("CybergfxDrawAACircleDrawingBoard");

    struct DrawingBoard *board = rctx->target_board;

    if (!board || !board->pixels) {
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

    /* Get direct pixel buffer access */
    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;

    /* Calculate bounding box with padding for antialiasing */
    WORD min_x = fmaxf(0, cybergfx_fast_ftoi(x - radius - 2.0f));
    WORD min_y = fmaxf(0, cybergfx_fast_ftoi(y - radius - 2.0f));
    WORD max_x = fminf(board->width - 1, cybergfx_fast_ftoi(x + radius + 2.0f));
    WORD max_y = fminf(board->height - 1, cybergfx_fast_ftoi(y + radius + 2.0f));

    D(bug("CybergfxDrawAACircleRastPort: bounding box (%d,%d) to (%d,%d)\n", min_x, min_y, max_x, max_y));

    /* SIMD/scanline path */
    cybergfx_simd_level level = cybergfx_get_simd_level();
    WORD width = max_x - min_x + 1;
    ULONG *row_buffer = width > 0 ? malloc((ULONG)width * sizeof(ULONG)) : NULL;
    float rel_x_buf[8];

    /* Convert fill brush to internal color once before the loop */
    struct InternalColor fill_color_storage;
    struct InternalColor *fill_color = NULL;
    if (fill_brush && ZuneBrushToInternalColor(rctx, fill_brush, &fill_color_storage)) {
        fill_color = &fill_color_storage;
    }

    for (WORD py = min_y; py <= max_y; py++) {
        ULONG row_base = (ULONG)py * pitch_pixels + min_x;
        if (row_buffer) {
            memcpy(row_buffer, pixels + row_base, (ULONG)width * sizeof(ULONG));
        }

        float rel_y = (py + 0.5f) - y;
        WORD px = 0;

        for (; px <= width - 4; px += 4) {
            for (WORD i = 0; i < 4; i++) rel_x_buf[i] = (min_x + px + i + 0.5f) - x;
            float dist[4];
            cybergfx_sdf_circle_batch4(rel_x_buf, rel_y, radius, dist);

            float fill_a[4], border_a[4];
            cybergfx_circle_alphas_batch4(dist, border_width, fill_color != NULL, border_color != NULL, fill_a, border_a);

            ULONG bg_pixels[4];
            ULONG *src = row_buffer ? (row_buffer + px) : (pixels + row_base + px);
            for (WORD i = 0; i < 4; i++)
                bg_pixels[i] = argb32_native_to_logical(src[i]);

            ULONG result_pixels[4];
            ULONG changed_mask;
            cybergfx_blend_aa_pixels_batch4(bg_pixels, fill_a, border_a,
                                            fill_color ? fill_color->r : 0,
                                            fill_color ? fill_color->g : 0,
                                            fill_color ? fill_color->b : 0,
                                            fill_color ? (fill_color->a / 255.0f) : 0.0f,
                                            border_color ? border_color->r : 0,
                                            border_color ? border_color->g : 0,
                                            border_color ? border_color->b : 0,
                                            border_color ? (border_color->a / 255.0f) : 0.0f,
                                            fill_color != NULL, border_color != NULL,
                                            result_pixels, &changed_mask);

            if (changed_mask) {
                for (WORD i = 0; i < 4; i++) {
                    if (changed_mask & (1u << i)) {
                        ULONG final_color = argb32_logical_to_native(result_pixels[i]);
                        CybergfxWritePixelClamped(pixels, pitch_pixels, board->width, board->height,
                                                  min_x + px + i, py, final_color);
                        if (row_buffer) row_buffer[px + i] = final_color;
                    }
                }
            }
        }

        for (; px < width; px++) {
            float rel_x = (min_x + px + 0.5f) - x;
            float dist = sqrtf(rel_x * rel_x + rel_y * rel_y) - radius;

            ULONG bg = row_buffer ? row_buffer[px] : pixels[row_base + px];
            UBYTE bg_a, bg_r, bg_g, bg_b;
            extract_bgra32(bg, &bg_a, &bg_r, &bg_g, &bg_b);

            BOOL pixel_changed = FALSE;

            if (border_width > 0.0f && border_color != NULL) {
                if (fill_color != NULL) {
                    float outer_alpha = 1.0f - smoothstep(-0.5f, 0.5f, dist);

                    if (outer_alpha > 0.01f) {
                        float border_threshold = -border_width;
                        float blend_factor = smoothstep(border_threshold - 0.5f, border_threshold + 0.5f, dist);

                        UBYTE final_r = (UBYTE)(blend_factor * fill_color->r + (1.0f - blend_factor) * border_color->r);
                        UBYTE final_g = (UBYTE)(blend_factor * fill_color->g + (1.0f - blend_factor) * border_color->g);
                        UBYTE final_b = (UBYTE)(blend_factor * fill_color->b + (1.0f - blend_factor) * border_color->b);
                        UBYTE final_a = (UBYTE)(blend_factor * fill_color->a + (1.0f - blend_factor) * border_color->a);

                        float final_alpha = (final_a / 255.0f) * outer_alpha;
                        blend_over(&bg_r, &bg_g, &bg_b, final_r, final_g, final_b, final_alpha);
                        pixel_changed = TRUE;
                    }
                } else {
                    float half_border = border_width * 0.5f;
                    float border_alpha = 1.0f - smoothstep(-half_border - 0.5f, half_border + 0.5f, fabsf(dist));

                    if (border_alpha > 0.01f) {
                        float final_border_alpha = (border_color->a / 255.0f) * border_alpha;
                        blend_over(&bg_r, &bg_g, &bg_b, border_color->r, border_color->g, border_color->b, final_border_alpha);
                        pixel_changed = TRUE;
                    }
                }
            } else if (fill_color != NULL) {
                float fill_alpha = 1.0f - smoothstep(-0.5f, 0.5f, dist);

                if (fill_alpha > 0.01f) {
                    float final_fill_alpha = (fill_color->a / 255.0f) * fill_alpha;
                    blend_over(&bg_r, &bg_g, &bg_b, fill_color->r, fill_color->g, fill_color->b, final_fill_alpha);
                    pixel_changed = TRUE;
                }
            }

            if (pixel_changed) {
                ULONG final_color = make_bgra32(bg_a, bg_r, bg_g, bg_b);
                CybergfxWritePixelClamped(pixels, pitch_pixels, board->width,
                                          board->height, min_x + px, py, final_color);
                if (row_buffer) row_buffer[px] = final_color;
            }
        }
    }

    if (row_buffer) {
        free(row_buffer);
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

    /* Get bitmap dimensions */
    ULONG bitmap_width = GetBitMapAttr(rctx->target_rastport->BitMap, BMA_WIDTH);
    ULONG bitmap_height = GetBitMapAttr(rctx->target_rastport->BitMap, BMA_HEIGHT);

    /* Calculate bounding box with padding for antialiasing */
    WORD min_x = fmaxf(0, cybergfx_fast_ftoi(x - radius - 2.0f));
    WORD min_y = fmaxf(0, cybergfx_fast_ftoi(y - radius - 2.0f));
    WORD max_x = fminf(bitmap_width - 1, cybergfx_fast_ftoi(x + radius + 2.0f));
    WORD max_y = fminf(bitmap_height - 1, cybergfx_fast_ftoi(y + radius + 2.0f));

    D(bug("CybergfxDrawAACircleRastPort: bounding box (%d,%d) to (%d,%d)\n", min_x, min_y, max_x, max_y));

    /* Use multi-scanline batching to reduce ReadPixelArray/WritePixelArray syscall overhead */
    #define SCANLINE_BATCH 32
    WORD width = max_x - min_x + 1;
    WORD total_height = max_y - min_y + 1;

    ULONG *batch_buffer = (width > 0 && total_height > 0)
                             ? malloc((ULONG)width * SCANLINE_BATCH * sizeof(ULONG))
                             : NULL;
    float rel_x_buf[8];

    /* Track which rows in the batch are dirty */
    BOOL batch_dirty[SCANLINE_BATCH];

    /* Convert fill brush to internal color once before the loop */
    struct InternalColor fill_color_storage;
    struct InternalColor *fill_color = NULL;
    if (fill_brush && ZuneBrushToInternalColor(rctx, fill_brush, &fill_color_storage)) {
        fill_color = &fill_color_storage;
    }

    /* Process rows in batches */
    for (WORD batch_start_y = min_y; batch_start_y <= max_y; batch_start_y += SCANLINE_BATCH) {
        WORD batch_height = (batch_start_y + SCANLINE_BATCH <= max_y + 1)
                              ? SCANLINE_BATCH
                              : (max_y - batch_start_y + 1);

        /* Read entire batch from raster port */
        BOOL batch_ok = FALSE;
        if (batch_buffer) {
            batch_ok = ReadPixelArray(batch_buffer, 0, 0, (ULONG)width * 4, rctx->target_rastport,
                                      min_x, batch_start_y, width, batch_height, CYBERGFX_PIXELFORMAT_ARGB32);
        }

        /* Reset dirty flags */
        for (WORD i = 0; i < batch_height; ++i) {
            batch_dirty[i] = FALSE;
        }

        BOOL any_dirty = FALSE;

        for (WORD row_in_batch = 0; row_in_batch < batch_height; ++row_in_batch) {
            WORD py = batch_start_y + row_in_batch;
            ULONG *row_buffer = batch_buffer + (row_in_batch * width);

            float rel_y = (py + 0.5f) - y;
            WORD px = 0;

            if (batch_ok && cybergfx_get_simd_level() == CYBERGFX_SIMD_AVX2) {
                for (; px <= width - 8; px += 8) {
                    for (WORD i = 0; i < 8; i++) rel_x_buf[i] = (min_x + px + i + 0.5f) - x;
                    float dist[8];
                    cybergfx_sdf_circle_batch8(rel_x_buf, rel_y, radius, dist);

                    float fill_a[8], border_a[8];
                    cybergfx_circle_alphas_batch8(dist, border_width, fill_color != NULL, border_color != NULL, fill_a, border_a);

                    ULONG bg_pixels[8];
                    memcpy(bg_pixels, row_buffer + px, sizeof(bg_pixels));

                    ULONG result_pixels[8];
                    ULONG changed_mask;
                    cybergfx_blend_aa_pixels_batch8(bg_pixels, fill_a, border_a,
                                                    fill_color ? fill_color->r : 0,
                                                    fill_color ? fill_color->g : 0,
                                                    fill_color ? fill_color->b : 0,
                                                    fill_color ? (fill_color->a / 255.0f) : 0.0f,
                                                    border_color ? border_color->r : 0,
                                                    border_color ? border_color->g : 0,
                                                    border_color ? border_color->b : 0,
                                                    border_color ? (border_color->a / 255.0f) : 0.0f,
                                                    fill_color != NULL, border_color != NULL,
                                                    result_pixels, &changed_mask);

                    if (changed_mask) {
                        for (WORD i = 0; i < 8; i++) {
                            if (changed_mask & (1u << i)) row_buffer[px + i] = result_pixels[i];
                        }
                        batch_dirty[row_in_batch] = TRUE;
                        any_dirty = TRUE;
                    }
                }
            }

            if (batch_ok) {
                for (; px <= width - 4; px += 4) {
                    for (WORD i = 0; i < 4; i++) rel_x_buf[i] = (min_x + px + i + 0.5f) - x;
                    float dist[4];
                    cybergfx_sdf_circle_batch4(rel_x_buf, rel_y, radius, dist);

                    float fill_a[4], border_a[4];
                    cybergfx_circle_alphas_batch4(dist, border_width, fill_color != NULL, border_color != NULL, fill_a, border_a);

                    ULONG bg_pixels[4];
                    memcpy(bg_pixels, row_buffer + px, sizeof(bg_pixels));

                    ULONG result_pixels[4];
                    ULONG changed_mask;
                    cybergfx_blend_aa_pixels_batch4(bg_pixels, fill_a, border_a,
                                                    fill_color ? fill_color->r : 0,
                                                    fill_color ? fill_color->g : 0,
                                                    fill_color ? fill_color->b : 0,
                                                    fill_color ? (fill_color->a / 255.0f) : 0.0f,
                                                    border_color ? border_color->r : 0,
                                                    border_color ? border_color->g : 0,
                                                    border_color ? border_color->b : 0,
                                                    border_color ? (border_color->a / 255.0f) : 0.0f,
                                                    fill_color != NULL, border_color != NULL,
                                                    result_pixels, &changed_mask);

                    if (changed_mask) {
                        for (WORD i = 0; i < 4; i++) {
                            if (changed_mask & (1u << i)) row_buffer[px + i] = result_pixels[i];
                        }
                        batch_dirty[row_in_batch] = TRUE;
                        any_dirty = TRUE;
                    }
                }
            }

            for (; px < width; px++) {
                float rel_x = (min_x + px + 0.5f) - x;
                float dist = sqrtf(rel_x * rel_x + rel_y * rel_y) - radius;

                ULONG bg = batch_ok ? row_buffer[px] : ReadRGBPixel(rctx->target_rastport, min_x + px, py);
                UBYTE bg_a, bg_r, bg_g, bg_b;
                extract_argb32(bg, &bg_a, &bg_r, &bg_g, &bg_b);

                BOOL pixel_changed = FALSE;

                if (border_width > 0.0f && border_color != NULL) {
                    if (fill_color != NULL) {
                        float outer_alpha = 1.0f - smoothstep(-0.5f, 0.5f, dist);

                        if (outer_alpha > 0.01f) {
                            float border_threshold = -border_width;
                            float blend_factor = smoothstep(border_threshold - 0.5f, border_threshold + 0.5f, dist);

                            UBYTE final_r = (UBYTE)(blend_factor * fill_color->r + (1.0f - blend_factor) * border_color->r);
                            UBYTE final_g = (UBYTE)(blend_factor * fill_color->g + (1.0f - blend_factor) * border_color->g);
                            UBYTE final_b = (UBYTE)(blend_factor * fill_color->b + (1.0f - blend_factor) * border_color->b);
                            UBYTE final_a = (UBYTE)(blend_factor * fill_color->a + (1.0f - blend_factor) * border_color->a);

                            float final_alpha = (final_a / 255.0f) * outer_alpha;
                            blend_over(&bg_r, &bg_g, &bg_b, final_r, final_g, final_b, final_alpha);
                            pixel_changed = TRUE;
                        }
                    } else {
                        float half_border = border_width * 0.5f;
                        float border_alpha = 1.0f - smoothstep(-half_border - 0.5f, half_border + 0.5f, fabsf(dist));

                        if (border_alpha > 0.01f) {
                            float final_border_alpha = (border_color->a / 255.0f) * border_alpha;
                            blend_over(&bg_r, &bg_g, &bg_b, border_color->r, border_color->g, border_color->b, final_border_alpha);
                            pixel_changed = TRUE;
                        }
                    }
                } else if (fill_color != NULL) {
                    float fill_alpha = 1.0f - smoothstep(-0.5f, 0.5f, dist);

                    if (fill_alpha > 0.01f) {
                        float final_fill_alpha = (fill_color->a / 255.0f) * fill_alpha;
                        blend_over(&bg_r, &bg_g, &bg_b, fill_color->r, fill_color->g, fill_color->b, final_fill_alpha);
                        pixel_changed = TRUE;
                    }
                }

                if (pixel_changed) {
                    ULONG final_color = make_argb32(bg_a, bg_r, bg_g, bg_b);
                    if (batch_ok) {
                        row_buffer[px] = final_color;
                        batch_dirty[row_in_batch] = TRUE;
                        any_dirty = TRUE;
                    } else {
                        WriteRGBPixel(rctx->target_rastport, min_x + px, py, final_color);
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
                                    rctx->target_rastport, min_x, batch_start_y + write_start, width, write_height,
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
