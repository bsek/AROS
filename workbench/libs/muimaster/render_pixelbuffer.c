/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <string.h>

#include "render_hal.h"
#include "muimaster_intern.h"
#include "support.h"
#include "mui.h"

/* Pixel buffer thresholds */
#define MIN_PIXELBUFFER_AREA    1000    /* Minimum area to justify pixel buffer */
#define PIXELBUFFER_GROWTH      1.2f    /* Growth factor for buffer reallocation */
#define MAX_PIXELBUFFER_SIZE    (1920*1080*4) /* Maximum buffer size (Full HD RGBA) */

/* Decision logic for pixel buffer usage */
BOOL MUI_ShouldUsePixelBuffer(Object *obj, ULONG operation_type, LONG area_size)
{
    struct MUI_RenderInfo *mri;

    if (!obj)
        return FALSE;

    mri = muiRenderInfo(obj);
    if (!mri || !mri->mri_HAL)
        return FALSE;

    /* Only use pixel buffer if supported */
    if (!(mri->mri_HAL->capabilities & RENDER_CAP_PIXELBUFFER))
        return FALSE;

    /* Use pixel buffer for large operations */
    if (area_size > MIN_PIXELBUFFER_AREA)
        return TRUE;

    /* Use for gradient operations regardless of size */
    if (operation_type == OP_GRADIENT)
        return TRUE;

    /* Use if we're already batching multiple operations */
    if (mri->mri_CurrentBatch && mri->mri_CurrentBatch->count > 3)
        return TRUE;

    /* Use for blend operations if hardware supports it */
    if (operation_type == OP_BLEND && (mri->mri_HAL->capabilities & RENDER_CAP_BLEND))
        return TRUE;

    return FALSE;
}

/* Acquire a pixel buffer of at least the specified dimensions */
MUI_PixelBuffer *MUI_AcquirePixelBuffer(struct MUI_RenderInfo *mri, ULONG min_width, ULONG min_height)
{
    MUI_PixelBuffer *pb;
    ULONG required_size;
    ULONG alloc_width, alloc_height;

    if (!mri)
        return NULL;

    pb = &mri->mri_PixelBuffer;
    required_size = min_width * min_height * sizeof(ULONG);

    /* Check if we need to allocate or reallocate */
    if (!pb->buffer || pb->width < min_width || pb->height < min_height) {
        /* Calculate new buffer size with some growth */
        alloc_width = MAX(min_width, (ULONG)(pb->width * PIXELBUFFER_GROWTH));
        alloc_height = MAX(min_height, (ULONG)(pb->height * PIXELBUFFER_GROWTH));

        /* Sanity check - don't allocate huge buffers */
        if (alloc_width * alloc_height * sizeof(ULONG) > MAX_PIXELBUFFER_SIZE) {
            alloc_width = min_width;
            alloc_height = min_height;

            /* If still too large, fail */
            if (alloc_width * alloc_height * sizeof(ULONG) > MAX_PIXELBUFFER_SIZE)
                return NULL;
        }

        /* Free old buffer if it exists */
        if (pb->buffer) {
            FreeVec(pb->buffer);
            pb->buffer = NULL;
        }

        /* Allocate new buffer */
        pb->alloc_size = alloc_width * alloc_height * sizeof(ULONG);
        pb->buffer = AllocVec(pb->alloc_size, MEMF_FAST | MEMF_CLEAR);

        if (!pb->buffer) {
            /* Allocation failed, reset structure */
            memset(pb, 0, sizeof(MUI_PixelBuffer));
            return NULL;
        }

        pb->width = alloc_width;
        pb->height = alloc_height;
        pb->format = 0; /* RGBA32 format */
    }

    /* Clear the dirty flag and reset dirty rect */
    pb->dirty = FALSE;
    pb->dirty_rect.MinX = pb->width;
    pb->dirty_rect.MinY = pb->height;
    pb->dirty_rect.MaxX = -1;
    pb->dirty_rect.MaxY = -1;

    return pb;
}

/* Release pixel buffer (doesn't actually free, just marks as unused) */
void MUI_ReleasePixelBuffer(struct MUI_RenderInfo *mri)
{
    if (mri && mri->mri_PixelBuffer.buffer) {
        /* Flush any pending changes */
        if (mri->mri_PixelBuffer.dirty) {
            MUI_FlushPixelBuffer(mri);
        }
    }
}

/* Flush pixel buffer contents to the rastport */
void MUI_FlushPixelBuffer(struct MUI_RenderInfo *mri)
{
    MUI_PixelBuffer *pb;

    if (!mri)
        return;

    pb = &mri->mri_PixelBuffer;

    if (!pb->buffer || !pb->dirty || !mri->mri_RastPort)
        return;

    /* Only flush the dirty area if it's valid */
    if (pb->dirty_rect.MaxX >= pb->dirty_rect.MinX &&
        pb->dirty_rect.MaxY >= pb->dirty_rect.MinY) {

        /* Use HAL function to copy to rastport */
        if (mri->mri_HAL && mri->mri_HAL->pb_copy_to_rastport) {
            mri->mri_HAL->pb_copy_to_rastport(pb->buffer, mri->mri_RastPort,
                                            pb->width, pb->height, &pb->dirty_rect);
        }
    }

    /* Clear dirty flag */
    pb->dirty = FALSE;
}

/* Update dirty rectangle to include new area */
void MUI_UpdateDirtyRect(struct Rectangle *dirty, LONG x1, LONG y1, LONG x2, LONG y2)
{
    if (!dirty)
        return;

    /* If this is the first dirty area, initialize bounds */
    if (dirty->MaxX < dirty->MinX || dirty->MaxY < dirty->MinY) {
        dirty->MinX = x1;
        dirty->MinY = y1;
        dirty->MaxX = x2;
        dirty->MaxY = y2;
    } else {
        /* Expand bounds to include new area */
        if (x1 < dirty->MinX) dirty->MinX = x1;
        if (y1 < dirty->MinY) dirty->MinY = y1;
        if (x2 > dirty->MaxX) dirty->MaxX = x2;
        if (y2 > dirty->MaxY) dirty->MaxY = y2;
    }
}

/* Check if two rectangles intersect */
BOOL MUI_RectIntersect(struct Rectangle *a, struct Rectangle *b, struct Rectangle *result)
{
    LONG left, top, right, bottom;

    if (!a || !b)
        return FALSE;

    left = MAX(a->MinX, b->MinX);
    top = MAX(a->MinY, b->MinY);
    right = MIN(a->MaxX, b->MaxX);
    bottom = MIN(a->MaxY, b->MaxY);

    if (left <= right && top <= bottom) {
        if (result) {
            result->MinX = left;
            result->MinY = top;
            result->MaxX = right;
            result->MaxY = bottom;
        }
        return TRUE;
    }

    return FALSE;
}

/* Enhanced pixel buffer operations that take advantage of the buffer */

/* Fill rectangle in pixel buffer with SIMD-friendly operation */
void MUI_PixelBuffer_FillRect(MUI_PixelBuffer *pb, LONG x1, LONG y1, LONG x2, LONG y2, ULONG rgba_color)
{
    LONG x, y, width, height;
    ULONG *line;

    if (!pb || !pb->buffer)
        return;

    /* Clamp coordinates to buffer bounds */
    x1 = MAX(0, MIN(x1, (LONG)pb->width - 1));
    y1 = MAX(0, MIN(y1, (LONG)pb->height - 1));
    x2 = MAX(0, MIN(x2, (LONG)pb->width - 1));
    y2 = MAX(0, MIN(y2, (LONG)pb->height - 1));

    if (x1 > x2 || y1 > y2)
        return;

    width = x2 - x1 + 1;
    height = y2 - y1 + 1;

    /* Simple line-by-line fill */
    for (y = y1; y <= y2; y++) {
        line = pb->buffer + y * pb->width + x1;
        for (x = 0; x < width; x++) {
            line[x] = rgba_color;
        }
    }

    /* Update dirty rectangle */
    MUI_UpdateDirtyRect(&pb->dirty_rect, x1, y1, x2, y2);
    pb->dirty = TRUE;
}

/* Blend rectangle in pixel buffer */
void MUI_PixelBuffer_BlendRect(MUI_PixelBuffer *pb, LONG x1, LONG y1, LONG x2, LONG y2, ULONG rgba_color, UBYTE alpha)
{
    LONG x, y, width;
    ULONG *line;
    UBYTE src_r, src_g, src_b, dst_r, dst_g, dst_b, new_r, new_g, new_b;
    UBYTE inv_alpha = 255 - alpha;

    if (!pb || !pb->buffer || alpha == 0)
        return;

    /* Clamp coordinates to buffer bounds */
    x1 = MAX(0, MIN(x1, (LONG)pb->width - 1));
    y1 = MAX(0, MIN(y1, (LONG)pb->height - 1));
    x2 = MAX(0, MIN(x2, (LONG)pb->width - 1));
    y2 = MAX(0, MIN(y2, (LONG)pb->height - 1));

    if (x1 > x2 || y1 > y2)
        return;

    /* If fully opaque, use faster fill */
    if (alpha == 255) {
        MUI_PixelBuffer_FillRect(pb, x1, y1, x2, y2, rgba_color);
        return;
    }

    width = x2 - x1 + 1;

    src_r = (rgba_color >> 24) & 0xFF;
    src_g = (rgba_color >> 16) & 0xFF;
    src_b = (rgba_color >> 8) & 0xFF;

    /* Blend line by line */
    for (y = y1; y <= y2; y++) {
        line = pb->buffer + y * pb->width + x1;
        for (x = 0; x < width; x++) {
            ULONG dst = line[x];

            dst_r = (dst >> 24) & 0xFF;
            dst_g = (dst >> 16) & 0xFF;
            dst_b = (dst >> 8) & 0xFF;

            new_r = (src_r * alpha + dst_r * inv_alpha) / 255;
            new_g = (src_g * alpha + dst_g * inv_alpha) / 255;
            new_b = (src_b * alpha + dst_b * inv_alpha) / 255;

            line[x] = (new_r << 24) | (new_g << 16) | (new_b << 8) | 0xFF;
        }
    }

    /* Update dirty rectangle */
    MUI_UpdateDirtyRect(&pb->dirty_rect, x1, y1, x2, y2);
    pb->dirty = TRUE;
}

/* Clear pixel buffer */
void MUI_PixelBuffer_Clear(MUI_PixelBuffer *pb, ULONG rgba_color)
{
    if (!pb || !pb->buffer)
        return;

    /* Fast clear using memset if color is uniform */
    if ((rgba_color & 0xFF) == ((rgba_color >> 8) & 0xFF) &&
        ((rgba_color >> 8) & 0xFF) == ((rgba_color >> 16) & 0xFF) &&
        ((rgba_color >> 16) & 0xFF) == ((rgba_color >> 24) & 0xFF)) {

        memset(pb->buffer, rgba_color & 0xFF, pb->alloc_size);
    } else {
        /* Fill pixel by pixel */
        ULONG *pixels = pb->buffer;
        ULONG total_pixels = pb->width * pb->height;
        ULONG i;

        for (i = 0; i < total_pixels; i++) {
            pixels[i] = rgba_color;
        }
    }

    /* Mark entire buffer as dirty */
    pb->dirty_rect.MinX = 0;
    pb->dirty_rect.MinY = 0;
    pb->dirty_rect.MaxX = pb->width - 1;
    pb->dirty_rect.MaxY = pb->height - 1;
    pb->dirty = TRUE;
}
