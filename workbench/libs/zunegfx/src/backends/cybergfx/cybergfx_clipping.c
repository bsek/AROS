/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Clipping Implementation

    This file keeps the CyberGraphics backend within valid render targets.
    Drawing to RastPorts relies on AROS clipping, while DrawingBoards
    still clamp coordinates to their buffer bounds.
*/

#include <aros/debug.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/regions.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <libraries/cybergraphics.h>

#include "../backend_interface.h"
#include "cybergfx_backend.h"
#include "../../zunegfx_intern.h"

/*****************************************************************************
 * CybergfxSetupClipping
 *
 * CybergfxSetupClipping
 *
 * No special setup is needed; clipping is delegated to the RastPort or
 * handled implicitly when we clamp to DrawingBoard bounds.
 *****************************************************************************/
BOOL CybergfxSetupClipping(struct RenderContext *rctx, struct Region *region) {
    ENTER_FUNCTION("CybergfxSetupClipping");
    (void)region;

    if (!rctx) {
        D(bug("CybergfxSetupClipping: Invalid RenderContext\n"));
        EXIT_FUNCTION("CybergfxSetupClipping");
        return FALSE;
    }

    /* Clipping is handled by the target RastPort/DrawingBoard. Nothing to do. */
    D(bug("CybergfxSetupClipping: Using native RastPort clipping\n"));

    EXIT_FUNCTION("CybergfxSetupClipping");
    return TRUE;
}

/*****************************************************************************
 * CybergfxClipPixel
 *
 * Validates that a pixel write stays inside a DrawingBoard. When rendering
 * to a RastPort we trust the OS clipping and therefore always return TRUE.
 *****************************************************************************/
BOOL CybergfxClipPixel(struct RenderContext *rctx, WORD x, WORD y) {
    if (!rctx) {
        return FALSE;
    }

    /* For drawing boards we must stay inside the buffer bounds */
    if (rctx->target_board) {
        struct DrawingBoard *board = rctx->target_board;
        return (x >= 0 && y >= 0 && x < board->width && y < board->height);
    }

    /* Rendering directly to a RastPort - rely on the OS clipping. */
    return TRUE;
}

/*****************************************************************************
 * CybergfxClipRectangle
 *
 * Clamps a rectangle to the DrawingBoard bounds. For RastPort rendering the
 * rectangle is left untouched because the OS applies clipping later.
 *****************************************************************************/
BOOL CybergfxClipRectangle(struct RenderContext *rctx, WORD x, WORD y, WORD width, WORD height,
                          WORD *out_x, WORD *out_y, WORD *out_width, WORD *out_height) {
    if (!rctx || !out_x || !out_y || !out_width || !out_height) {
        return FALSE;
    }

    if (width <= 0 || height <= 0) {
        return FALSE;
    }

    /* Rendering directly to RastPort - clipping handled by AROS. */
    if (!rctx->target_board) {
        *out_x = x;
        *out_y = y;
        *out_width = width;
        *out_height = height;
        return TRUE;
    }

    struct DrawingBoard *board = rctx->target_board;
    WORD left = x;
    WORD top = y;
    WORD right = x + width - 1;
    WORD bottom = y + height - 1;

    WORD min_x = 0;
    WORD min_y = 0;
    WORD max_x = board->width - 1;
    WORD max_y = board->height - 1;

    if (right < min_x || bottom < min_y || left > max_x || top > max_y) {
        return FALSE;
    }

    if (left < min_x)
        left = min_x;
    if (top < min_y)
        top = min_y;
    if (right > max_x)
        right = max_x;
    if (bottom > max_y)
        bottom = max_y;

    *out_x = left;
    *out_y = top;
    *out_width = right - left + 1;
    *out_height = bottom - top + 1;
    return TRUE;
}

/*****************************************************************************
 * CybergfxClearClipping
 *
 * Clears any backend-specific clipping state. For cybergfx, this is mostly
 * a no-op since clipping state is managed by the core library.
 *****************************************************************************/
void CybergfxClearClipping(struct RenderContext *rctx) {
    ENTER_FUNCTION("CybergfxClearClipping");

    if (!rctx) {
        EXIT_FUNCTION("CybergfxClearClipping");
        return;
    }

    /* For software backends, clipping state is managed by the core library.
     * We don't need to do anything special here, but we could add debugging
     * or statistics tracking if needed.
     */

    D(bug("CybergfxClearClipping: Clipping cleared for RenderContext %p\n", rctx));

    EXIT_FUNCTION("CybergfxClearClipping");
}

/*****************************************************************************
 * CybergfxGetClipBounds
 *
 * CybergfxGetClipBounds
 *
 * Returns the buffer bounds for DrawingBoards; RastPorts report infinite
 * bounds because the system handles clipping.
 *****************************************************************************/
BOOL CybergfxGetClipBounds(struct RenderContext *rctx, WORD *min_x, WORD *min_y,
                          WORD *max_x, WORD *max_y) {
    if (!rctx || !min_x || !min_y || !max_x || !max_y) {
        return FALSE;
    }

    if (!rctx->target_board) {
        /* No explicit bounds; rely on RastPort clipping */
        *min_x = -32768;
        *min_y = -32768;
        *max_x = 32767;
        *max_y = 32767;
        return FALSE;
    }

    *min_x = 0;
    *min_y = 0;
    *max_x = rctx->target_board->width - 1;
    *max_y = rctx->target_board->height - 1;
    return TRUE;
}

/*****************************************************************************
 * CybergfxClipLine
 *
 * CybergfxClipLine
 *
 * Clamps line endpoints to DrawingBoard bounds using Cohen-Sutherland.
 * RastPort rendering skips clipping here.
 *****************************************************************************/
BOOL CybergfxClipLine(struct RenderContext *rctx, WORD *x1, WORD *y1, WORD *x2, WORD *y2) {
    if (!rctx || !x1 || !y1 || !x2 || !y2) {
        return FALSE;
    }

    if (!rctx->target_board) {
        /* Nothing to clip when rendering directly to RastPort */
        return TRUE;
    }

    WORD xmin = 0;
    WORD ymin = 0;
    WORD xmax = rctx->target_board->width - 1;
    WORD ymax = rctx->target_board->height - 1;

    /* Cohen-Sutherland outcodes */
    #define INSIDE 0  // 0000
    #define LEFT   1  // 0001
    #define RIGHT  2  // 0010
    #define BOTTOM 4  // 0100
    #define TOP    8  // 1000

    /* Helper function to compute Cohen-Sutherland outcodes */
    #define COMPUTE_OUTCODE(x, y) \
        (((x) < xmin ? LEFT : (x) > xmax ? RIGHT : 0) | \
         ((y) < ymin ? BOTTOM : (y) > ymax ? TOP : 0))

    WORD sx = *x1, sy = *y1;
    WORD ex = *x2, ey = *y2;
    UBYTE outcode1 = COMPUTE_OUTCODE(sx, sy);
    UBYTE outcode2 = COMPUTE_OUTCODE(ex, ey);
    BOOL accept = FALSE;

    while (TRUE) {
        if (!(outcode1 | outcode2)) {
            /* Both points inside */
            accept = TRUE;
            break;
        } else if (outcode1 & outcode2) {
            /* Both points outside same region */
            break;
        } else {
            /* At least one point outside */
            WORD x, y;
            UBYTE outcode_out = outcode1 ? outcode1 : outcode2;

            if (outcode_out & TOP) {
                x = sx + (ex - sx) * (ymax - sy) / (ey - sy);
                y = ymax;
            } else if (outcode_out & BOTTOM) {
                x = sx + (ex - sx) * (ymin - sy) / (ey - sy);
                y = ymin;
            } else if (outcode_out & RIGHT) {
                y = sy + (ey - sy) * (xmax - sx) / (ex - sx);
                x = xmax;
            } else if (outcode_out & LEFT) {
                y = sy + (ey - sy) * (xmin - sx) / (ex - sx);
                x = xmin;
            }

            if (outcode_out == outcode1) {
                sx = x;
                sy = y;
                outcode1 = COMPUTE_OUTCODE(sx, sy);
            } else {
                ex = x;
                ey = y;
                outcode2 = COMPUTE_OUTCODE(ex, ey);
            }
        }
    }

    if (accept) {
        *x1 = sx;
        *y1 = sy;
        *x2 = ex;
        *y2 = ey;
        return TRUE;
    }

    return FALSE;
}

/*****************************************************************************
 * CybergfxClipFillPixelArray
 *
 * CybergfxClipFillPixelArray
 *
 * Ensures FillPixelArray stays inside DrawingBoard bounds; RastPorts are
 * left to AROS clipping, but coordinates are still validated for boards.
 *****************************************************************************/
void CybergfxClipFillPixelArray(struct RenderContext *rctx, struct RastPort *rastport,
                               WORD x, WORD y, WORD width, WORD height, ULONG color) {
    if (!rctx || !rastport) {
        return;
    }

    WORD clipped_x = x;
    WORD clipped_y = y;
    WORD clipped_width = width;
    WORD clipped_height = height;

    if (!CybergfxClipRectangle(rctx, x, y, width, height,
                               &clipped_x, &clipped_y, &clipped_width, &clipped_height)) {
        return;
    }

    FillPixelArray(rastport, clipped_x, clipped_y, clipped_width, clipped_height, color);
}

/*****************************************************************************
 * CybergfxClipFillPixelArrayDirect
 *
 * CybergfxClipFillPixelArrayDirect
 *
 * Performs a buffer-safe fill for locked DrawingBoards by clamping to the
 * buffer dimensions. RastPort paths never call this helper.
 *****************************************************************************/
void CybergfxClipFillPixelArrayDirect(struct RenderContext *rctx, ULONG *pixels,
                                     UWORD pitch_pixels, UWORD buffer_width, UWORD buffer_height,
                                     WORD x, WORD y, WORD width, WORD height, ULONG pixel) {
    if (!rctx || !pixels) {
        return;
    }

    WORD clipped_x, clipped_y, clipped_width, clipped_height;
    if (!CybergfxClipRectangle(rctx, x, y, width, height,
                               &clipped_x, &clipped_y, &clipped_width, &clipped_height)) {
        return;
    }

    WORD end_x = clipped_x + clipped_width;
    WORD end_y = clipped_y + clipped_height;

    if (clipped_x < 0) clipped_x = 0;
    if (clipped_y < 0) clipped_y = 0;
    if (end_x > buffer_width) end_x = buffer_width;
    if (end_y > buffer_height) end_y = buffer_height;

    for (WORD py = clipped_y; py < end_y; py++) {
        for (WORD px = clipped_x; px < end_x; px++) {
            pixels[py * pitch_pixels + px] = pixel;
        }
    }
}

/*****************************************************************************
 * CybergfxWritePixelClamped
 *
 * Writes a single ARGB pixel into a locked DrawingBoard buffer while enforcing
 * buffer bounds. Intended for tight loops that previously repeated the same
 * bounds checks.
 *****************************************************************************/
void CybergfxWritePixelClamped(ULONG *pixels, UWORD pitch_pixels,
                               UWORD buffer_width, UWORD buffer_height,
                               WORD x, WORD y, ULONG pixel) {
    if (!pixels) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    if ((UWORD)x >= buffer_width || (UWORD)y >= buffer_height) {
        return;
    }

    pixels[(ULONG)y * pitch_pixels + (ULONG)x] = pixel;
}
