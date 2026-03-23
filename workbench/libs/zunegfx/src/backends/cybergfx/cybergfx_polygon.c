/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Polygon Fill

    Scanline fill algorithm for arbitrary polygons.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include "libraries/zunegfx.h"
#include "../backend_interface.h"
#include "../../zunegfx_intern.h"

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
 * CybergfxFillPolygon - Fill a polygon using scanline algorithm
 *
 * For each scanline, finds all edge intersections, sorts them,
 * and fills between pairs using FillPixelArray.
 */
void CybergfxFillPolygon(struct RenderContext *rctx, struct ZunePoint *points,
                         UWORD count, struct ZuneBrush *brush, BOOL antialias)
{
    struct RastPort *rp;
    WORD minY, maxY;
    WORD intersections[MAX_INTERSECTIONS];
    UWORD num_intersections;
    UWORD i;
    WORD y;
    ULONG fill_color;

    if (!rctx || !points || count < 3 || !brush) {
        return;
    }

    /* Currently only support solid brush */
    if (brush->type == ZUNE_BRUSH_TYPE_SOLID) {
        fill_color = brush->data.solid.color;
    } else if (brush->type == ZUNE_BRUSH_TYPE_PEN) {
        /* Convert pen to ARGB — approximate */
        fill_color = ZUNE_COLOR_RGB24(128, 128, 128);
    } else {
        return;
    }

    rp = rctx->target_board ? rctx->target_board->rastport
                            : rctx->target_rastport;
    if (!rp) {
        return;
    }

    /* Find vertical bounding box */
    minY = maxY = points[0].y;
    for (i = 1; i < count; i++) {
        if (points[i].y < minY) minY = points[i].y;
        if (points[i].y > maxY) maxY = points[i].y;
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
                /* Calculate x intersection using integer math */
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
            UWORD width = x_end - x_start;

            if (width > 0) {
                FillPixelArray(rp, x_start, y, width, 1, fill_color);
            }
        }
    }

    D(bug("CybergfxFillPolygon: Filled polygon with %d vertices\n", count));
}
