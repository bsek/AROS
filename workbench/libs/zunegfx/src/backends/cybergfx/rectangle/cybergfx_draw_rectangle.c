#include "../cybergfx_brush_sampler.h"
#include "cybergfx_rectangle_internal.h"

#include <aros/macros.h>
#define DEBUG 0
#include <aros/debug.h>

#include <proto/intuition.h>
#include <intuition/intuition.h>

struct timer { /* Timer struct used to record times   */
    ULONG seconds, micros;
};

/**
 * delta()
 *     This little cluge calculates the delta-time
 * and prints it, along with a '\n'
 */
void delta(struct timer *finish, struct timer *start) {
    LONG dsec, dmic;

    dmic = finish->micros - start->micros;
    dsec = finish->seconds - start->seconds;

    if (dmic < 0) {
        dmic += 1000000;
        dsec--;
    }
    D(bug("%d.%06d sec (%d us)\n", dsec, dmic, dmic));
}

/*****************************************************************************
 * CybergfxDrawRectangle
 *
 * Main rectangle drawing function for the CyberGraphics backend.
 * Supports both regular and rounded rectangles with comprehensive clipping
 * and optimized rendering paths for different target types.
 *****************************************************************************/
void CybergfxDrawRectangle(struct RenderContext *rctx, WORD x, WORD y, UWORD width, UWORD height, UBYTE border_width, UBYTE border_radius,
                           struct ZuneBrush *fill_brush, struct InternalColor *border_color, BOOL filled, BOOL antialias) {
    ENTER_FUNCTION("CybergfxDrawRectangle");

    struct timer start, finish;

    CurrentTime(&start.seconds, &start.micros);
    if (!rctx) {
        EXIT_FUNCTION("CybergfxDrawRectangle");
        return;
    }

    /* Apply clipping to rectangle */
    WORD clipped_x, clipped_y, clipped_width, clipped_height;
    if (!CybergfxClipRectangle(rctx, x, y, width, height, &clipped_x, &clipped_y, &clipped_width, &clipped_height)) {
        /* Rectangle is completely outside clipping region */
        EXIT_FUNCTION("CybergfxDrawRectangle");
        return;
    }

    /* Update coordinates with clipped values */
    x = clipped_x;
    y = clipped_y;
    width = clipped_width;
    height = clipped_height;

    /* Check for zero-sized rectangles after clipping */
    if (width == 0 || height == 0) {
        D(bug("CybergfxDrawRectangle: Zero-sized rectangle after clipping - skipping\n"));
        EXIT_FUNCTION("CybergfxDrawRectangle");
        return;
    }

    if (fill_brush) {
        PrepareBrushForRendering(rctx, fill_brush, x, y, width, height);
    }

    if (rctx->target_board && rctx->target_board->pixels_locked) {
        /* Use direct pixel manipulation for locked DrawingBoard */
        /* Use pack_argb32 for correct format when writing directly to memory */
        ULONG border_pixel = border_color ? pack_argb32(border_color->a, border_color->r, border_color->g, border_color->b) : 0;
        if (border_radius) {
            if (antialias) {
                CybergfxAARectangleDrawingBoard(rctx->target_board, x, y, width, height, border_radius, border_width, fill_brush, border_color,
                                                border_color, filled, border_width > 0);
            } else {
                CybergfxDrawRoundedRectangleToLockedDrawingBoard(rctx, x, y, width, height, border_width, fill_brush, border_pixel, border_radius,
                                                                 filled);
            }
        } else {
            CybergfxDrawRectangleToLockedDrawingBoard(rctx, x, y, width, height, border_width, fill_brush, border_pixel, filled);
        }
    } else {
        ULONG border_pixel = border_color ? border_color->original_pixel : 0;
        if (border_radius) {
            if (antialias) {
                CybergfxAARectangleRasterPort(rctx->target_rastport, x, y, width, height, border_radius, border_width, fill_brush, border_color, border_color,
                                              filled, border_width > 0);
            } else {
                CybergfxDrawRoundedRectangleToRasterPort(rctx, rctx->target_rastport, x, y, width, height, border_width, fill_brush, border_pixel,
                                                         border_radius, filled);
            }
        } else {
            CybergfxDrawRectangleToRasterPort(rctx, rctx->target_rastport, x, y, width, height, border_width, fill_brush, border_pixel, filled);
        }
    }

    CurrentTime(&finish.seconds, &finish.micros);

    // D(bug("Rectangle timing: start=%ld.%ld, finish=%ld.%ld\n",
    //      start.seconds, start.micros, finish.seconds, finish.micros));

    D(bug("DrawRectangle (w,h)(%d,%d) radius: %d took: \n", width, height, border_radius)); delta(&finish,&start);

    EXIT_FUNCTION("CybergfxDrawRectangle");
}
