/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - CyberGraphics Backend Polygon Fill

    Scanline fill algorithm for arbitrary polygons.
    Supports:
    - Locked DrawingBoards (direct pixel manipulation)
    - Unlocked DrawingBoards and RastPorts (FillPixelArray / WritePixelArray)
    - All brush types: solid, pen, linear gradient, texture, pattern
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include "libraries/zunegfx.h"
#include "../backend_interface.h"
#include "../../zunegfx_intern.h"
#include "cybergfx_backend.h"
#include "cybergfx_brush_sampler.h"
#include "cybergfx_pixel_format.h"

#define DEBUG 0
#include <aros/debug.h>

/* Maximum number of edge intersections per scanline */
#define MAX_INTERSECTIONS 64

/*
 * Simple insertion sort for a small array of WORD values.
 */
static void SortIntersections(WORD *arr, UWORD count)
{
    UWORD i, j;
    WORD key;

    for (i = 1; i < count; i++) {
        key = arr[i];
        j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

/*
 * Fill a solid-color span to a locked DrawingBoard.
 */
static void FillSpanLocked(struct RenderContext *rctx, ULONG *pixels,
                           ULONG pitch_pixels, WORD x_start, WORD y,
                           WORD width, ULONG pixel)
{
    CybergfxClipFillPixelArrayDirect(rctx, pixels, pitch_pixels,
                                     rctx->target_board->width,
                                     rctx->target_board->height,
                                     x_start, y, width, 1, pixel);
}

/*
 * Fill a brush-sampled span to a locked DrawingBoard.
 * Reuses the SampleBrush/SampleBrushBatch4 pattern from rectangle code.
 */
static void FillSpanBrushLocked(struct RenderContext *rctx, ULONG *pixels,
                                ULONG pitch_pixels,
                                struct ZuneBrush *brush,
                                WORD bbox_x, WORD bbox_y,
                                UWORD bbox_w, UWORD bbox_h,
                                WORD x_start, WORD y, WORD width)
{
    struct DrawingBoard *board = rctx->target_board;
    WORD px;

    /* Clip span to board bounds */
    WORD x_end = x_start + width;
    if (x_start < 0) x_start = 0;
    if (y < 0 || y >= board->height) return;
    if (x_end > board->width) x_end = board->width;
    if (x_start >= x_end) return;

    WORD span_width = x_end - x_start;
    ULONG *dest_row = pixels + ((ULONG)y * pitch_pixels) + x_start;

    /* Process 4 pixels at a time */
    px = 0;
    for (; px <= span_width - 4; px += 4) {
        WORD px_coords[4];
        UBYTE fc_r[4], fc_g[4], fc_b[4], fc_a[4];
        WORD k;

        for (k = 0; k < 4; k++)
            px_coords[k] = x_start + px + k;

        SampleBrushBatch4(brush, bbox_x, bbox_y, bbox_w, bbox_h,
                          px_coords, y, fc_r, fc_g, fc_b, fc_a);

        for (k = 0; k < 4; k++)
            dest_row[px + k] = pack_argb32(0xFF, fc_r[k], fc_g[k], fc_b[k]);
    }

    /* Remaining pixels */
    for (; px < span_width; px++) {
        UBYTE fc_r, fc_g, fc_b, fc_a;
        SampleBrush(brush, bbox_x, bbox_y, bbox_w, bbox_h,
                    x_start + px, y, &fc_r, &fc_g, &fc_b, &fc_a);
        dest_row[px] = pack_argb32(0xFF, fc_r, fc_g, fc_b);
    }
}

/*
 * Fill a brush-sampled span to a RastPort using WritePixelArray.
 * Reuses the pattern from cybergfx_draw_rectangle_to_raster_port.c.
 */
static void FillSpanBrushRastPort(struct RenderContext *rctx,
                                  struct RastPort *rp,
                                  struct ZuneBrush *brush,
                                  ULONG *span_buffer,
                                  WORD bbox_x, WORD bbox_y,
                                  UWORD bbox_w, UWORD bbox_h,
                                  WORD x_start, WORD y, WORD width)
{
    WORD px;

    /* Process 4 pixels at a time */
    px = 0;
    for (; px <= width - 4; px += 4) {
        WORD px_coords[4];
        UBYTE fc_r[4], fc_g[4], fc_b[4], fc_a[4];
        WORD k;

        for (k = 0; k < 4; k++)
            px_coords[k] = x_start + px + k;

        SampleBrushBatch4(brush, bbox_x, bbox_y, bbox_w, bbox_h,
                          px_coords, y, fc_r, fc_g, fc_b, fc_a);

        for (k = 0; k < 4; k++)
            span_buffer[px + k] = pack_argb32_logical(0xFF, fc_r[k], fc_g[k], fc_b[k]);
    }

    /* Remaining pixels */
    for (; px < width; px++) {
        UBYTE fc_r, fc_g, fc_b, fc_a;
        SampleBrush(brush, bbox_x, bbox_y, bbox_w, bbox_h,
                    x_start + px, y, &fc_r, &fc_g, &fc_b, &fc_a);
        span_buffer[px] = pack_argb32_logical(0xFF, fc_r, fc_g, fc_b);
    }

    WritePixelArray(span_buffer, 0, 0, (ULONG)width * 4,
                    rp, x_start, y, width, 1,
                    CYBERGFX_PIXELFORMAT_ARGB32);
}

/*
 * CybergfxFillPolygon - Fill a polygon using scanline algorithm
 *
 * Supports three rendering paths:
 * 1. Locked DrawingBoard: direct pixel buffer writes
 * 2. Unlocked DrawingBoard: via RastPort + FillPixelArray/WritePixelArray
 * 3. Direct RastPort: same as (2)
 *
 * Supports all brush types via PrepareBrushForRendering/SampleBrush.
 */
void CybergfxFillPolygon(struct RenderContext *rctx, struct ZunePoint *points,
                         UWORD count, struct ZuneBrush *brush, BOOL antialias)
{
    WORD minX, minY, maxX, maxY;
    WORD intersections[MAX_INTERSECTIONS];
    UWORD num_intersections;
    UWORD i;
    WORD y;

    BOOL use_locked = FALSE;
    BOOL use_simple_solid = FALSE;
    ULONG solid_pixel = 0;
    ULONG solid_color_rp = 0;

    ULONG *pixels = NULL;
    ULONG pitch_pixels = 0;
    struct RastPort *rp = NULL;
    ULONG *span_buffer = NULL;

    if (!rctx || !points || count < 3 || !brush) {
        return;
    }

    /* Determine rendering path */
    if (rctx->target_board) {
        struct DrawingBoard *board = rctx->target_board;

        if (board->pixels_locked && board->pixels &&
            (board->pixel_format == PIXFMT_ARGB32 ||
             board->pixel_format == PIXFMT_RGBA32)) {
            /* Locked DrawingBoard: direct pixel access */
            use_locked = TRUE;
            pixels = (ULONG *)board->pixels;
            pitch_pixels = board->pitch / 4;
        } else {
            /* Unlocked DrawingBoard: use its RastPort */
            rp = board->rastport;
        }
    } else {
        /* Direct RastPort rendering */
        rp = rctx->target_rastport;
    }

    if (!use_locked && !rp) {
        return;
    }

    /* Find bounding box */
    minX = maxX = points[0].x;
    minY = maxY = points[0].y;
    for (i = 1; i < count; i++) {
        if (points[i].x < minX) minX = points[i].x;
        if (points[i].x > maxX) maxX = points[i].x;
        if (points[i].y < minY) minY = points[i].y;
        if (points[i].y > maxY) maxY = points[i].y;
    }

    UWORD bbox_w = maxX - minX + 1;
    UWORD bbox_h = maxY - minY + 1;

    if (bbox_w == 0 || bbox_h == 0) return;

    /* Prepare brush for rendering (pre-computes gradient caches etc.) */
    PrepareBrushForRendering(rctx, brush, minX, minY, bbox_w, bbox_h);

    /* Check for simple solid color fast path */
    if (brush->type == ZUNE_BRUSH_TYPE_SOLID || brush->type == ZUNE_BRUSH_TYPE_PEN) {
        use_simple_solid = TRUE;
        if (use_locked) {
            solid_pixel = pack_argb32(255, brush->internal.color.r,
                                      brush->internal.color.g,
                                      brush->internal.color.b);
        } else {
            solid_color_rp = brush->internal.color.original_pixel;
        }
    }

    /* For non-solid brushes going to RastPort, allocate a span buffer */
    if (!use_simple_solid && !use_locked) {
        WORD max_span = maxX - minX + 1;
        if (max_span > 0) {
            span_buffer = malloc((ULONG)max_span * sizeof(ULONG));
            if (!span_buffer) {
                CleanupBrushInternalState(brush);
                return;
            }
        }
    }

    /* Scanline fill */
    for (y = minY; y <= maxY; y++) {
        num_intersections = 0;

        /* Find all edge intersections with this scanline */
        for (i = 0; i < count; i++) {
            UWORD j = (i + 1) % count;
            WORD y0 = points[i].y;
            WORD y1 = points[j].y;
            WORD x0 = points[i].x;
            WORD x1 = points[j].x;

            /* Skip horizontal edges */
            if (y0 == y1) continue;

            /* Ensure y0 < y1 */
            if (y0 > y1) {
                WORD tmp;
                tmp = y0; y0 = y1; y1 = tmp;
                tmp = x0; x0 = x1; x1 = tmp;
            }

            /* Check if scanline intersects this edge */
            if (y >= y0 && y < y1) {
                WORD ix = x0 + (LONG)(y - y0) * (x1 - x0) / (y1 - y0);

                if (num_intersections < MAX_INTERSECTIONS) {
                    intersections[num_intersections++] = ix;
                }
            }
        }

        if (num_intersections < 2) continue;

        /* Sort intersections by x */
        SortIntersections(intersections, num_intersections);

        /* Fill between pairs of intersections */
        for (i = 0; i + 1 < num_intersections; i += 2) {
            WORD x_start = intersections[i];
            WORD x_end = intersections[i + 1];
            WORD width = x_end - x_start;

            if (width <= 0) continue;

            if (use_simple_solid) {
                /* Solid color fast path */
                if (use_locked) {
                    FillSpanLocked(rctx, pixels, pitch_pixels,
                                   x_start, y, width, solid_pixel);
                } else {
                    CybergfxClipFillPixelArray(rctx, rp, x_start, y,
                                               width, 1, solid_color_rp);
                }
            } else {
                /* Gradient/texture/pattern brush */
                if (use_locked) {
                    FillSpanBrushLocked(rctx, pixels, pitch_pixels,
                                        brush, minX, minY, bbox_w, bbox_h,
                                        x_start, y, width);
                } else {
                    FillSpanBrushRastPort(rctx, rp, brush, span_buffer,
                                          minX, minY, bbox_w, bbox_h,
                                          x_start, y, width);
                }
            }
        }
    }

    /* Cleanup */
    if (span_buffer) {
        free(span_buffer);
    }

    CleanupBrushInternalState(brush);

    D(bug("CybergfxFillPolygon: Filled polygon with %d vertices\n", count));
}
