/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <string.h>

#include "render_hal.h"
#include "muimaster_intern.h"
#include "support.h"
#include "mui.h"

/* Initial batch capacity */
#define INITIAL_BATCH_CAPACITY  16
#define BATCH_GROWTH_FACTOR     2

/* Create a new draw batch */
struct MUI_DrawBatch *MUI_CreateBatch(LONG initial_capacity)
{
    struct MUI_DrawBatch *batch;
    LONG capacity = (initial_capacity > 0) ? initial_capacity : INITIAL_BATCH_CAPACITY;

    batch = AllocVec(sizeof(struct MUI_DrawBatch), MEMF_CLEAR);
    if (!batch)
        return NULL;

    /* Allocate arrays */
    batch->rects = AllocVec(capacity * sizeof(struct Rectangle), MEMF_ANY);
    batch->colors = AllocVec(capacity * sizeof(ULONG), MEMF_ANY);
    batch->operations = AllocVec(capacity * sizeof(UBYTE), MEMF_ANY);
    batch->alphas = AllocVec(capacity * sizeof(UBYTE), MEMF_ANY);

    if (!batch->rects || !batch->colors || !batch->operations || !batch->alphas) {
        MUI_FreeBatch(batch);
        return NULL;
    }

    batch->capacity = capacity;
    batch->count = 0;

    return batch;
}

/* Free a draw batch */
void MUI_FreeBatch(struct MUI_DrawBatch *batch)
{
    if (!batch)
        return;

    if (batch->rects)
        FreeVec(batch->rects);
    if (batch->colors)
        FreeVec(batch->colors);
    if (batch->operations)
        FreeVec(batch->operations);
    if (batch->alphas)
        FreeVec(batch->alphas);

    FreeVec(batch);
}

/* Expand batch capacity */
static BOOL MUI_ExpandBatch(struct MUI_DrawBatch *batch)
{
    LONG new_capacity = batch->capacity * BATCH_GROWTH_FACTOR;
    struct Rectangle *new_rects;
    ULONG *new_colors;
    UBYTE *new_operations;
    UBYTE *new_alphas;

    /* Allocate new arrays */
    new_rects = AllocVec(new_capacity * sizeof(struct Rectangle), MEMF_ANY);
    new_colors = AllocVec(new_capacity * sizeof(ULONG), MEMF_ANY);
    new_operations = AllocVec(new_capacity * sizeof(UBYTE), MEMF_ANY);
    new_alphas = AllocVec(new_capacity * sizeof(UBYTE), MEMF_ANY);

    if (!new_rects || !new_colors || !new_operations || !new_alphas) {
        if (new_rects) FreeVec(new_rects);
        if (new_colors) FreeVec(new_colors);
        if (new_operations) FreeVec(new_operations);
        if (new_alphas) FreeVec(new_alphas);
        return FALSE;
    }

    /* Copy existing data */
    CopyMem(batch->rects, new_rects, batch->count * sizeof(struct Rectangle));
    CopyMem(batch->colors, new_colors, batch->count * sizeof(ULONG));
    CopyMem(batch->operations, new_operations, batch->count * sizeof(UBYTE));
    CopyMem(batch->alphas, new_alphas, batch->count * sizeof(UBYTE));

    /* Free old arrays */
    FreeVec(batch->rects);
    FreeVec(batch->colors);
    FreeVec(batch->operations);
    FreeVec(batch->alphas);

    /* Update batch structure */
    batch->rects = new_rects;
    batch->colors = new_colors;
    batch->operations = new_operations;
    batch->alphas = new_alphas;
    batch->capacity = new_capacity;

    return TRUE;
}

/* Add a rectangle operation to the batch */
void MUI_AddRectToBatch(struct MUI_DrawBatch *batch, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color, UBYTE operation)
{
    if (!batch)
        return;

    /* Expand batch if needed */
    if (batch->count >= batch->capacity) {
        if (!MUI_ExpandBatch(batch))
            return; /* Out of memory - operation is lost */
    }

    /* Add the operation */
    batch->rects[batch->count].MinX = x1;
    batch->rects[batch->count].MinY = y1;
    batch->rects[batch->count].MaxX = x2;
    batch->rects[batch->count].MaxY = y2;
    batch->colors[batch->count] = color;
    batch->operations[batch->count] = operation;
    batch->alphas[batch->count] = 255; /* Default to fully opaque */

    batch->count++;
}

/* Add a blended rectangle operation to the batch */
void MUI_AddBlendRectToBatch(struct MUI_DrawBatch *batch, LONG x1, LONG y1, LONG x2, LONG y2, ULONG rgba_color, UBYTE alpha)
{
    if (!batch || alpha == 0)
        return;

    /* Expand batch if needed */
    if (batch->count >= batch->capacity) {
        if (!MUI_ExpandBatch(batch))
            return; /* Out of memory - operation is lost */
    }

    /* Add the blend operation */
    batch->rects[batch->count].MinX = x1;
    batch->rects[batch->count].MinY = y1;
    batch->rects[batch->count].MaxX = x2;
    batch->rects[batch->count].MaxY = y2;
    batch->colors[batch->count] = rgba_color;
    batch->operations[batch->count] = BATCH_OP_BLEND;
    batch->alphas[batch->count] = alpha;

    batch->count++;
}

/* Optimize batch by merging adjacent rectangles with same properties */
static void MUI_OptimizeBatch(struct MUI_DrawBatch *batch)
{
    LONG i, j, write_pos;

    if (!batch || batch->count <= 1)
        return;

    /* Simple optimization: merge horizontally adjacent rectangles */
    write_pos = 0;

    for (i = 0; i < batch->count; i++) {
        BOOL merged = FALSE;

        /* Try to merge with previous rectangles */
        for (j = 0; j < write_pos; j++) {
            if (batch->operations[i] == batch->operations[j] &&
                batch->colors[i] == batch->colors[j] &&
                batch->alphas[i] == batch->alphas[j] &&
                batch->rects[i].MinY == batch->rects[j].MinY &&
                batch->rects[i].MaxY == batch->rects[j].MaxY) {

                /* Check for horizontal adjacency */
                if (batch->rects[i].MinX == batch->rects[j].MaxX + 1) {
                    /* Merge to the right */
                    batch->rects[j].MaxX = batch->rects[i].MaxX;
                    merged = TRUE;
                    break;
                } else if (batch->rects[i].MaxX + 1 == batch->rects[j].MinX) {
                    /* Merge to the left */
                    batch->rects[j].MinX = batch->rects[i].MinX;
                    merged = TRUE;
                    break;
                }
            }
        }

        /* If not merged, copy to write position */
        if (!merged) {
            if (write_pos != i) {
                batch->rects[write_pos] = batch->rects[i];
                batch->colors[write_pos] = batch->colors[i];
                batch->operations[write_pos] = batch->operations[i];
                batch->alphas[write_pos] = batch->alphas[i];
            }
            write_pos++;
        }
    }

    batch->count = write_pos;
}

/* Sort batch operations for optimal rendering order */
static void MUI_SortBatch(struct MUI_DrawBatch *batch)
{
    LONG i, j;
    struct Rectangle temp_rect;
    ULONG temp_color;
    UBYTE temp_op, temp_alpha;

    if (!batch || batch->count <= 1)
        return;

    /* Simple bubble sort - sorts by operation type first, then by Y coordinate */
    for (i = 0; i < batch->count - 1; i++) {
        for (j = 0; j < batch->count - 1 - i; j++) {
            BOOL should_swap = FALSE;

            /* Primary sort: operation type (fills before blends) */
            if (batch->operations[j] > batch->operations[j + 1]) {
                should_swap = TRUE;
            }
            /* Secondary sort: Y coordinate (top to bottom) */
            else if (batch->operations[j] == batch->operations[j + 1] &&
                     batch->rects[j].MinY > batch->rects[j + 1].MinY) {
                should_swap = TRUE;
            }

            if (should_swap) {
                /* Swap all arrays */
                temp_rect = batch->rects[j];
                batch->rects[j] = batch->rects[j + 1];
                batch->rects[j + 1] = temp_rect;

                temp_color = batch->colors[j];
                batch->colors[j] = batch->colors[j + 1];
                batch->colors[j + 1] = temp_color;

                temp_op = batch->operations[j];
                batch->operations[j] = batch->operations[j + 1];
                batch->operations[j + 1] = temp_op;

                temp_alpha = batch->alphas[j];
                batch->alphas[j] = batch->alphas[j + 1];
                batch->alphas[j + 1] = temp_alpha;
            }
        }
    }
}

/* Execute batch using traditional rastport operations */
static void MUI_ExecuteBatchTraditional(struct MUI_RenderInfo *mri, struct MUI_DrawBatch *batch)
{
    LONG i;
    struct RastPort *rp = mri->mri_RastPort;

    if (!rp)
        return;

    for (i = 0; i < batch->count; i++) {
        struct Rectangle *rect = &batch->rects[i];
        ULONG color = batch->colors[i];
        UBYTE operation = batch->operations[i];

        switch (operation) {
            case BATCH_OP_FILL:
                if (mri->mri_HAL) {
                    mri->mri_HAL->fill_rect(rp, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY, color);
                } else {
                    SetAPen(rp, color);
                    RectFill(rp, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);
                }
                break;

            case BATCH_OP_PATTERN:
                /* Pattern operations would need additional data */
                /* For now, fall back to solid fill */
                if (mri->mri_HAL) {
                    mri->mri_HAL->fill_rect(rp, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY, color);
                } else {
                    SetAPen(rp, color);
                    RectFill(rp, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);
                }
                break;

            case BATCH_OP_BLEND:
                /* Alpha blending - use pixel buffer if available */
                if (mri->mri_HAL && mri->mri_HAL->pb_blend_rect &&
                    (mri->mri_HAL->capabilities & RENDER_CAP_PIXELBUFFER)) {

                    MUI_PixelBuffer *pb = MUI_AcquirePixelBuffer(mri, rect->MaxX + 1, rect->MaxY + 1);
                    if (pb) {
                        mri->mri_HAL->pb_blend_rect(pb->buffer, pb->width,
                            rect->MinX, rect->MinY, rect->MaxX, rect->MaxY,
                            color, batch->alphas[i]);
                        MUI_UpdateDirtyRect(&pb->dirty_rect, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);
                        pb->dirty = TRUE;
                    } else {
                        /* Fallback to opaque fill if alpha > 128 */
                        if (batch->alphas[i] > 128) {
                            SetAPen(rp, color & 0xFF); /* Simplified color conversion */
                            RectFill(rp, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);
                        }
                    }
                } else {
                    /* No blending support - fallback to opaque */
                    if (batch->alphas[i] > 128) {
                        SetAPen(rp, color & 0xFF);
                        RectFill(rp, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);
                    }
                }
                break;
        }
    }
}

/* Execute batch using HAL batch operations */
static void MUI_ExecuteBatchHAL(struct MUI_RenderInfo *mri, struct MUI_DrawBatch *batch)
{
    LONG i, fill_start, fill_count, blend_start, blend_count;

    if (!mri->mri_HAL)
        return;

    /* Process operations by type for better batching */
    i = 0;
    while (i < batch->count) {
        UBYTE current_op = batch->operations[i];

        if (current_op == BATCH_OP_FILL) {
            /* Count consecutive fill operations */
            fill_start = i;
            fill_count = 0;
            while (i < batch->count && batch->operations[i] == BATCH_OP_FILL) {
                fill_count++;
                i++;
            }

            /* Execute batch fill */
            if (mri->mri_HAL->batch_fill_rects && fill_count > 1) {
                mri->mri_HAL->batch_fill_rects(mri->mri_RastPort,
                    &batch->rects[fill_start], &batch->colors[fill_start], fill_count);
            } else {
                /* Fall back to individual fills */
                LONG j;
                for (j = fill_start; j < fill_start + fill_count; j++) {
                    mri->mri_HAL->fill_rect(mri->mri_RastPort,
                        batch->rects[j].MinX, batch->rects[j].MinY,
                        batch->rects[j].MaxX, batch->rects[j].MaxY,
                        batch->colors[j]);
                }
            }

        } else if (current_op == BATCH_OP_BLEND) {
            /* Count consecutive blend operations */
            blend_start = i;
            blend_count = 0;
            while (i < batch->count && batch->operations[i] == BATCH_OP_BLEND) {
                blend_count++;
                i++;
            }

            /* Execute batch blend */
            if (mri->mri_HAL->batch_blend_rects && blend_count > 1) {
                mri->mri_HAL->batch_blend_rects(mri->mri_RastPort,
                    &batch->rects[blend_start], &batch->colors[blend_start],
                    &batch->alphas[blend_start], blend_count);
            } else {
                /* Fall back to individual blends via pixel buffer */
                LONG j;
                for (j = blend_start; j < blend_start + blend_count; j++) {
                    MUI_PixelBuffer *pb = MUI_AcquirePixelBuffer(mri,
                        batch->rects[j].MaxX + 1, batch->rects[j].MaxY + 1);
                    if (pb && mri->mri_HAL->pb_blend_rect) {
                        mri->mri_HAL->pb_blend_rect(pb->buffer, pb->width,
                            batch->rects[j].MinX, batch->rects[j].MinY,
                            batch->rects[j].MaxX, batch->rects[j].MaxY,
                            batch->colors[j], batch->alphas[j]);
                        MUI_UpdateDirtyRect(&pb->dirty_rect,
                            batch->rects[j].MinX, batch->rects[j].MinY,
                            batch->rects[j].MaxX, batch->rects[j].MaxY);
                        pb->dirty = TRUE;
                    }
                }
            }

        } else {
            /* Skip unknown operations */
            i++;
        }
    }
}

/* Flush batch operations to the screen */
void MUI_FlushBatch(struct MUI_RenderInfo *mri, struct MUI_DrawBatch *batch)
{
    if (!mri || !batch || batch->count == 0)
        return;

    /* Optimize batch before execution */
    MUI_OptimizeBatch(batch);
    MUI_SortBatch(batch);

    /* Execute batch using best available method */
    if (mri->mri_HAL && (mri->mri_HAL->capabilities & RENDER_CAP_BATCH)) {
        MUI_ExecuteBatchHAL(mri, batch);
    } else {
        MUI_ExecuteBatchTraditional(mri, batch);
    }

    /* Clear batch */
    batch->count = 0;

    /* Flush any pixel buffer changes */
    if (mri->mri_PixelBuffer.dirty) {
        MUI_FlushPixelBuffer(mri);
    }
}

/* Clear batch without executing */
void MUI_ClearBatch(struct MUI_DrawBatch *batch)
{
    if (batch) {
        batch->count = 0;
    }
}

/* Check if batch has operations */
BOOL MUI_BatchHasOperations(struct MUI_DrawBatch *batch)
{
    return (batch && batch->count > 0);
}

/* Get batch statistics */
LONG MUI_GetBatchCount(struct MUI_DrawBatch *batch)
{
    return batch ? batch->count : 0;
}

LONG MUI_GetBatchCapacity(struct MUI_DrawBatch *batch)
{
    return batch ? batch->capacity : 0;
}
