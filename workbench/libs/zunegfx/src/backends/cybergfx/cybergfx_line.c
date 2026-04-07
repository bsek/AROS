/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - CyberGraphics Line Drawing Implementation

    This file implements line drawing functions for the CyberGraphics backend,
    including Bresenham line algorithm and optimized drawing paths for both
    locked and unlocked DrawingBoards.
*/

#include "cybergraphx/cybergraphics.h"
#include <exec/types.h>
#include <graphics/gfx.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../zunegfx_intern.h"
#include "../backend_interface.h"
#include "clib/arossupport_protos.h"
#include "clib/cybergraphics_protos.h"
#include "clib/graphics_protos.h"
#include "cybergfx_antialiasing.h"
#include "cybergfx_backend.h"
#include "graphics/rastport.h"
#include "inline/graphics.h"
#include "libraries/zunegfx.h"

static void CybergfxDrawLineWu(struct DrawingBoard *board, float x0, float y0, float x1, float y1, struct InternalColor *color);
static void CybergfxDrawLineAA(struct RenderContext *rctx, WORD x1, WORD y1, WORD x2, WORD y2, struct InternalColor *color);

/*****************************************************************************
 * Helper function for Bresenham's line algorithm
 * This function handles both direct pixel manipulation (for locked
 * DrawingBoards) and RastPort operations (for unlocked DrawingBoards and screen
 * rendering).
 *****************************************************************************/
static void draw_line_bresenham(struct RenderContext *rctx, UWORD start_x, UWORD start_y, UWORD end_x, UWORD end_y, struct InternalColor *color,
                                BOOL use_direct_pixels, UWORD line_width) {
    D(bug("[ZuneGfx] DrawLineBresenham: start(%d,%d), end(%d,%d), width=%d, direct=%d\n", start_x, start_y, end_x, end_y, line_width,
          use_direct_pixels));

    struct RastPort *rastport;
    if (!use_direct_pixels) {
        if (rctx->target_board != NULL) {
            rastport = rctx->target_board->rastport;
        } else {
            rastport = rctx->target_rastport;
        }
    }

    WORD dx = abs(end_x - start_x);
    WORD dy = abs(end_y - start_y);
    WORD sx = start_x < end_x ? 1 : -1;
    WORD sy = start_y < end_y ? 1 : -1;
    WORD err = dx - dy;
    WORD x = start_x;
    WORD y = start_y;

    /* Used for direct pixel manipulation for locked DrawingBoard */
    ULONG *pixels = NULL;
    ULONG pitch_pixels = 0;

    if (use_direct_pixels) {
        pixels = (ULONG *)rctx->target_board->pixels;
        pitch_pixels = rctx->target_board->pitch / 4;
    }

    /* Calculate perpendicular direction for line width */
    WORD perp_dx = 0, perp_dy = 0;
    if (line_width > 1) {
        /* Calculate perpendicular vector to the line direction */
        if (dx > dy) {
            /* More horizontal than vertical - perpendicular is vertical */
            perp_dx = 0;
            perp_dy = 1;
        } else {
            /* More vertical than horizontal - perpendicular is horizontal */
            perp_dx = 1;
            perp_dy = 0;
        }
    }

    WORD half_width = line_width / 2;

    while (1) {
        /* Set pixels for line width */
        if (line_width <= 1) {
            /* Single pixel line */
            if (use_direct_pixels) {
                /* Use pack_argb32 for correct format when writing directly to memory */
                ULONG pixel = pack_argb32(color->a, color->r, color->g, color->b);
                CybergfxWritePixelClamped(pixels, pitch_pixels,
                                          rctx->target_board->width,
                                          rctx->target_board->height, x, y,
                                          pixel);
            } else {
                WriteRGBPixel(rastport, x, y, color->original_pixel);
            }
        } else {
            /* Thick line - draw pixels perpendicular to line direction */
            WORD w_start = -half_width;
            WORD w_end = half_width;
            if (line_width % 2 == 0)
                w_end--; /* Adjust for even widths */

            for (WORD w = w_start; w <= w_end; w++) {
                WORD px = x + w * perp_dx;
                WORD py = y + w * perp_dy;

                if (use_direct_pixels) {
                    /* Use pack_argb32 for correct format when writing directly to memory */
                    ULONG pixel = pack_argb32(color->a, color->r, color->g, color->b);
                    CybergfxWritePixelClamped(pixels, pitch_pixels,
                                              rctx->target_board->width,
                                              rctx->target_board->height, px, py,
                                              pixel);
                } else {
                    WriteRGBPixel(rastport, px, py, color->original_pixel);
                }
            }
        }

        /* Check if we've reached the end poWORD */
        if (x == end_x && y == end_y)
            break;

        WORD e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

/*****************************************************************************
 * Helper function for drawing straight lines using FillPixelArray.
 * Handles both unlocked DrawingBoards and direct RastPort rendering.
 *****************************************************************************/
static void draw_straight_line_rastport(struct RenderContext *rctx, struct RastPort *rastport, WORD start_x, WORD start_y, WORD end_x, WORD end_y,
                                        UWORD width, struct InternalColor *color) {
    ENTER_FUNCTION("CybergfxDrawStraightLineFillPixelArray");

    if (!rctx || !rastport || !color) {
        EXIT_FUNCTION("CybergfxDrawStraightLineFillPixelArray");
        return;
    }

    WORD half_width = (WORD)(width / 2);

    if (start_x == end_x) {
        /* Vertical line */
        WORD x_start = start_x - half_width;
        WORD x_end = start_x + half_width;
        if (width % 2 == 0)
            x_end--; /* Adjust for even widths */
        WORD y_min = MIN(start_y, end_y);
        WORD y_max = MAX(start_y, end_y);
        CybergfxClipFillPixelArray(rctx, rastport, x_start, y_min, x_end - x_start + 1, y_max - y_min + 1, color->original_pixel);
    } else {
        /* Horizontal line */
        WORD y_start = start_y - half_width;
        WORD y_end = start_y + half_width;
        if (width % 2 == 0)
            y_end--; /* Adjust for even widths */
        WORD x_min = MIN(start_x, end_x);
        WORD x_max = MAX(start_x, end_x);
        CybergfxClipFillPixelArray(rctx, rastport, x_min, y_start, x_max - x_min + 1, y_end - y_start + 1, color->original_pixel);
    }

    EXIT_FUNCTION("CybergfxDrawStraightLineFillPixelArray");
}

static void draw_straight_line(struct RenderContext *rctx, UWORD start_x, UWORD start_y, UWORD end_x, UWORD end_y, UWORD line_width,
                               struct InternalColor *color) {
    ENTER_FUNCTION("draw_straight_line");

    if (!rctx || !color) {
        EXIT_FUNCTION("draw_straight_line");
        return;
    }

    /* Check what the RenderContext is targeting */
    if (rctx->target_board) {
        /* Rendering to DrawingBoard */
        struct DrawingBoard *board = rctx->target_board;

        if (board->pixels_locked && (board->pixel_format == PIXFMT_ARGB32 || board->pixel_format == PIXFMT_RGBA32)) {
            /* Direct pixel manipulation for locked DrawingBoard */
            ULONG *pixels = (ULONG *)board->pixels;
            ULONG pitch_pixels = board->pitch / 4;
            /* Use pack_argb32 for correct format when writing directly to memory */
            ULONG pixel = pack_argb32(color->a, color->r, color->g, color->b);

            if (start_x == end_x) {
                /* Vertical line */
                WORD y_min = MIN(start_y, end_y);
                WORD y_max = MAX(start_y, end_y);
                if (line_width == 1) {
                    for (WORD y = y_min; y <= y_max; y++) {
                        CybergfxWritePixelClamped(pixels, pitch_pixels, board->width,
                                                  board->height, start_x, y, pixel);
                    }
                } else {
                    WORD half_width = line_width / 2;
                    WORD x_start = start_x - half_width;
                    WORD x_end = start_x + half_width;
                    if (line_width % 2 == 0)
                        x_end--;

                    for (WORD y = y_min; y <= y_max; y++) {
                        for (WORD x = x_start; x <= x_end; x++) {
                            CybergfxWritePixelClamped(pixels, pitch_pixels, board->width,
                                                      board->height, x, y, pixel);
                        }
                    }
                }
            } else {
                /* Horizontal line */
                WORD x_min = MIN(start_x, end_x);
                WORD x_max = MAX(start_x, end_x);
                if (line_width == 1) {
                    for (WORD x = x_min; x <= x_max; x++) {
                        CybergfxWritePixelClamped(pixels, pitch_pixels, board->width,
                                                  board->height, x, start_y, pixel);
                    }
                } else {
                    WORD half_width = line_width / 2;
                    WORD y_start = start_y - half_width;
                    WORD y_end = start_y + half_width;
                    if (line_width % 2 == 0)
                        y_end--;

                    for (WORD x = x_min; x <= x_max; x++) {
                        for (WORD y = y_start; y <= y_end; y++) {
                            CybergfxWritePixelClamped(pixels, pitch_pixels, board->width,
                                                      board->height, x, y, pixel);
                        }
                    }
                }
            }

        } else {
            /* DrawingBoard pixels are unlocked, use FillPixelArray */
            draw_straight_line_rastport(rctx, board->rastport, start_x, start_y, end_x, end_y, line_width, color);
        }
    } else if (rctx->target_rastport) {
        /* Direct RastPort rendering using FillPixelArray */
        draw_straight_line_rastport(rctx, rctx->target_rastport, start_x, start_y, end_x, end_y, line_width, color);
    }

    EXIT_FUNCTION("draw_straight_line");
}

/*****************************************************************************
 * CybergfxDrawLine
 *
 * Line drawing implementation handling straight, diagonal lines with optional
 * thickness and anti-aliasing
 *****************************************************************************/
void CybergfxDrawLine(struct RenderContext *rctx, WORD start_x, WORD start_y, WORD end_x, WORD end_y, UWORD line_width, struct InternalColor *color,
                      BOOL antialias) {
    ENTER_FUNCTION("CybergfxDrawLine");

    if (!rctx || !color) {
        EXIT_FUNCTION("CybergfxDrawLine");
        return;
    }

    /* Clip line against clipping region - modifies coordinates if needed */
    WORD x1 = start_x, y1 = start_y, x2 = end_x, y2 = end_y;
    if (!CybergfxClipLine(rctx, &x1, &y1, &x2, &y2)) {
        /* Line is completely outside clipping region */
        EXIT_FUNCTION("CybergfxDrawLine");
        return;
    }

    /* Update coordinates with clipped values */
    start_x = x1;
    start_y = y1;
    end_x = x2;
    end_y = y2;

    /* Check if we have a straight line (horizontal or vertical) */
    /* For straight lines, we can use more optimized drawing functions */
    if (start_x == end_x || start_y == end_y) {
        draw_straight_line(rctx, start_x, start_y, end_x, end_y, line_width, color);
        EXIT_FUNCTION("CybergfxDrawLine");
        return;
    }

    if (antialias) {
        return CybergfxDrawLineAA(rctx, start_x, start_y, end_x, end_y, color);
    }

    /* Check what the RenderContext is targeting - check target_board first */
    if (rctx->target_board) {
        /* Rendering to DrawingBoard */
        struct DrawingBoard *board = rctx->target_board;
        if (board->pixels_locked && (board->pixel_format == PIXFMT_ARGB32 || board->pixel_format == PIXFMT_RGBA32)) {
            /* Use direct pixel manipulation for locked DrawingBoard */
            draw_line_bresenham(rctx, start_x, start_y, end_x, end_y, color, TRUE, line_width);
        } else {
            /* DrawingBoard pixels are unlocked, write to rastport */
            draw_line_bresenham(rctx, start_x, start_y, end_x, end_y, color, FALSE, line_width);
        }
    } else if (rctx->target_rastport) {
        draw_line_bresenham(rctx, start_x, start_y, end_x, end_y, color, FALSE, line_width);
    }

    EXIT_FUNCTION("CybergfxDrawLine");
}

/*****************************************************************************/
/* Wu Line Drawing Algorithm - Direct Pixel Access */
/*****************************************************************************/

static inline void write_wu_pixels(struct RastPort *rp, ULONG *pixels, ULONG pitch_pixels,
                                   ULONG x, ULONG y, float alpha, struct InternalColor *color,
                                   ULONG bitmap_width, ULONG bitmap_height) {
    /* Bounds check */
    if (x >= bitmap_width || y >= bitmap_height)
        return;

    if (rp != NULL) {
        ULONG bg = ReadRGBPixel(rp, x, y);
        UBYTE bg_a, bg_r, bg_g, bg_b;
        unpack_argb32(bg, &bg_a, &bg_r, &bg_g, &bg_b);
        blend_over(&bg_r, &bg_g, &bg_b, color->r, color->g, color->b, alpha);
        WriteRGBPixel(rp, x, y, pack_argb32(bg_a, bg_r, bg_g, bg_b));
    } else {
        ULONG *pixel_ptr = &pixels[y * pitch_pixels + x];
        ULONG bg = *pixel_ptr;
        UBYTE bg_a, bg_r, bg_g, bg_b;
        extract_pixel_value(bg, &bg_a, &bg_r, &bg_g, &bg_b);
        blend_over(&bg_r, &bg_g, &bg_b, color->r, color->g, color->b, alpha);
        *pixel_ptr = pack_argb32(bg_a, bg_r, bg_g, bg_b);
    }
}

/* Optimized Wu line drawing with direct pixel access */
void CybergfxDrawLineAA(struct RenderContext *rctx, WORD x1, WORD y1, WORD x2, WORD y2, struct InternalColor *color) {
    ENTER_FUNCTION("CybergfxDrawLineDrawingBoardAA");

    if (!rctx || !color) {
        EXIT_FUNCTION("CybergfxDrawLineDrawingBoardAA");
        return;
    }

    struct RastPort *rastport = NULL;
    ULONG bitmap_width, bitmap_height = 0;

    /* Set up direct pixel access */
    ULONG *pixels = NULL;
    ULONG pitch_pixels = 0;
    /* Ensure pixels are locked and we support the pixel format */
    if (rctx->target_board && rctx->target_board->pixels_locked) {
        pixels = (ULONG *)rctx->target_board->pixels;
        pitch_pixels = rctx->target_board->pitch / 4;
        bitmap_width = rctx->target_board->width;
        bitmap_height = rctx->target_board->height;
    } else if (rctx->target_board) {
        rastport = rctx->target_board->rastport;
        bitmap_width = GetBitMapAttr(rastport->BitMap, BMA_WIDTH);
        bitmap_height = GetBitMapAttr(rastport->BitMap, BMA_HEIGHT);
    } else {
        rastport = rctx->target_rastport;
        bitmap_width = GetBitMapAttr(rastport->BitMap, BMA_WIDTH);
        bitmap_height = GetBitMapAttr(rastport->BitMap, BMA_HEIGHT);
    }

    /* Wu's line algorithm parameters */
    float x0 = (float)x1, y0 = (float)y1;
    float x_end = (float)x2, y_end = (float)y2;

    BOOL steep = fabsf(y_end - y0) > fabsf(x_end - x0);

    if (steep) {
        float temp;
        temp = x0;
        x0 = y0;
        y0 = temp;
        temp = x_end;
        x_end = y_end;
        y_end = temp;
    }

    if (x0 > x_end) {
        float temp;
        temp = x0;
        x0 = x_end;
        x_end = temp;
        temp = y0;
        y0 = y_end;
        y_end = temp;
    }

    float dx = x_end - x0;
    float dy = y_end - y0;
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    /* Handle first endpoint */
    float xend = x0;
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5f);
    WORD xpxl1 = cybergfx_fast_ftoi(xend);
    WORD ypxl1 = cybergfx_fast_ftoi(yend);

    /* Draw first endpoint pixels */
    if (steep) {
        write_wu_pixels(rastport, pixels, pitch_pixels, ypxl1, xpxl1, rfpart(yend) * xgap, color, bitmap_width, bitmap_height);
        write_wu_pixels(rastport, pixels, pitch_pixels, ypxl1 + 1, xpxl1, fpart(yend) * xgap, color, bitmap_width, bitmap_height);
    } else {
        write_wu_pixels(rastport, pixels, pitch_pixels, xpxl1, ypxl1, rfpart(yend) * xgap, color, bitmap_width, bitmap_height);
        write_wu_pixels(rastport, pixels, pitch_pixels, xpxl1, ypxl1 + 1, fpart(yend) * xgap, color, bitmap_width, bitmap_height);
    }

    float intery = yend + gradient;

    /* Handle second endpoint */
    xend = x_end;
    yend = y_end + gradient * (xend - x_end);
    xgap = fpart(x_end + 0.5f);
    WORD xpxl2 = cybergfx_fast_ftoi(xend);
    WORD ypxl2 = cybergfx_fast_ftoi(yend);

    /* Draw second endpoint pixels */
    if (steep) {
        write_wu_pixels(rastport, pixels, pitch_pixels, ypxl2, xpxl2, rfpart(yend) * xgap, color, bitmap_width, bitmap_height);
        write_wu_pixels(rastport, pixels, pitch_pixels, ypxl2 + 1, xpxl2, fpart(yend) * xgap, color, bitmap_width, bitmap_height);
    } else {
        write_wu_pixels(rastport, pixels, pitch_pixels, xpxl2, ypxl2, rfpart(yend) * xgap, color, bitmap_width, bitmap_height);
        write_wu_pixels(rastport, pixels, pitch_pixels, xpxl2, ypxl2 + 1, fpart(yend) * xgap, color, bitmap_width, bitmap_height);
    }

    /* Main loop - draw antialiased pixels between endpoints */
    for (WORD x = xpxl1 + 1; x < xpxl2; x++) {
        WORD py = cybergfx_fast_ftoi(intery);
        if (steep) {
            write_wu_pixels(rastport, pixels, pitch_pixels, py, x, rfpart(intery), color, bitmap_width, bitmap_height);
            write_wu_pixels(rastport, pixels, pitch_pixels, py + 1, x, fpart(intery), color, bitmap_width, bitmap_height);
        } else {
            write_wu_pixels(rastport, pixels, pitch_pixels, x, py, rfpart(intery), color, bitmap_width, bitmap_height);
            write_wu_pixels(rastport, pixels, pitch_pixels, x, py + 1, fpart(intery), color, bitmap_width, bitmap_height);
        }
        intery += gradient;
    }

    EXIT_FUNCTION("CybergfxDrawLineDrawingBoardAA");
}
