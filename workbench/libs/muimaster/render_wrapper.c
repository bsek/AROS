/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <string.h>

#include "render_hal.h"
#include "render_batch.h"
#include "muimaster_intern.h"
#include "support.h"
#include "mui.h"

/* Create a transparent RastPort wrapper */
struct MUI_RastPortWrapper *MUI_CreateRastPortWrapper(struct MUI_RenderInfo *mri, struct RastPort *original)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!mri || !original)
        return NULL;

    wrapper = AllocVec(sizeof(struct MUI_RastPortWrapper), MEMF_CLEAR);
    if (!wrapper)
        return NULL;

    /* Copy original rastport */
    CopyMem(original, &wrapper->rp, sizeof(struct RastPort));

    /* Set wrapper-specific fields */
    wrapper->magic = MUI_RASTPORT_MAGIC;
    wrapper->mri = mri;
    wrapper->batch = NULL;
    wrapper->immediate_mode = TRUE;  /* Start in immediate mode */
    wrapper->use_pixelbuffer = FALSE;
    wrapper->current_pen = original->FgPen;
    wrapper->current_bpen = original->BgPen;
    wrapper->current_drmd = original->DrawMode;

    return wrapper;
}

/* Free RastPort wrapper */
void MUI_FreeRastPortWrapper(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper) {
        /* Flush any pending operations */
        if (wrapper->batch && wrapper->batch->count > 0) {
            MUI_FlushBatch(wrapper->mri, wrapper->batch);
        }

        /* Release pixel buffer if in use */
        if (wrapper->use_pixelbuffer) {
            MUI_ReleasePixelBuffer(wrapper->mri);
        }

        FreeVec(wrapper);
    }
}

/* Set wrapper to batch mode */
void MUI_EnableBatchMode(struct MUI_RastPortWrapper *wrapper, struct MUI_DrawBatch *batch)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        wrapper->immediate_mode = FALSE;
        wrapper->batch = batch;
    }
}

/* Set wrapper to immediate mode */
void MUI_DisableBatchMode(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        /* Flush any pending operations first */
        if (wrapper->batch && wrapper->batch->count > 0) {
            MUI_FlushBatch(wrapper->mri, wrapper->batch);
        }

        wrapper->immediate_mode = TRUE;
        wrapper->batch = NULL;
    }
}

/* Enable pixel buffer mode */
void MUI_EnablePixelBuffer(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        wrapper->use_pixelbuffer = TRUE;
    }
}

/* Disable pixel buffer mode */
void MUI_DisablePixelBuffer(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        /* Flush pixel buffer if it was in use */
        if (wrapper->use_pixelbuffer) {
            MUI_FlushPixelBuffer(wrapper->mri);
        }
        wrapper->use_pixelbuffer = FALSE;
    }
}

/* Transparent RectFill implementation */
void MUI_RectFill(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!rp)
        return;

    /* Check if this is our wrapped rastport */
    if (!IS_MUI_RASTPORT(rp)) {
        /* Not our wrapper, call original function */
        RectFill(rp, x1, y1, x2, y2);
        return;
    }

    wrapper = GET_WRAPPER(rp);
    if (!wrapper->mri || !wrapper->mri->mri_HAL) {
        /* No HAL available, fall back */
        RectFill(rp, x1, y1, x2, y2);
        return;
    }

    /* Calculate area for decision making */
    LONG area = (x2 - x1 + 1) * (y2 - y1 + 1);

    /* Decide rendering path */
    if (wrapper->use_pixelbuffer || MUI_ShouldUsePixelBuffer(NULL, OP_FILL, area)) {
        /* Use pixel buffer path */
        MUI_PixelBuffer *pb = MUI_AcquirePixelBuffer(wrapper->mri, x2 + 1, y2 + 1);
        if (pb) {
            ULONG rgba_color = wrapper->mri->mri_HAL->pen_to_rgba32(wrapper->current_pen, wrapper->mri);
            wrapper->mri->mri_HAL->pb_fill_rect(pb->buffer, pb->width, x1, y1, x2, y2, rgba_color);
            MUI_UpdateDirtyRect(&pb->dirty_rect, x1, y1, x2, y2);
            pb->dirty = TRUE;
            return;
        }
    }

    if (!wrapper->immediate_mode && wrapper->batch) {
        /* Add to batch */
        MUI_AddRectToBatch(wrapper->batch, x1, y1, x2, y2, wrapper->current_pen, BATCH_OP_FILL);
    } else {
        /* Use HAL for immediate drawing */
        wrapper->mri->mri_HAL->fill_rect(rp, x1, y1, x2, y2, wrapper->current_pen);
    }
}

/* Transparent SetAPen implementation */
void MUI_SetAPen(struct RastPort *rp, ULONG pen)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!rp) {
        return;
    }

    /* Always set the pen in the rastport */
    SetAPen(rp, pen);

    /* If this is our wrapper, update our tracking */
    if (IS_MUI_RASTPORT(rp)) {
        wrapper = GET_WRAPPER(rp);
        wrapper->current_pen = pen;

        /* If we have HAL, also update through it */
        if (wrapper->mri && wrapper->mri->mri_HAL) {
            wrapper->mri->mri_HAL->set_pen(rp, pen);
        }
    }
}

/* Transparent SetBPen implementation */
void MUI_SetBPen(struct RastPort *rp, ULONG pen)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!rp) {
        return;
    }

    /* Always set the pen in the rastport */
    SetBPen(rp, pen);

    /* If this is our wrapper, update our tracking */
    if (IS_MUI_RASTPORT(rp)) {
        wrapper = GET_WRAPPER(rp);
        wrapper->current_bpen = pen;
    }
}

/* Transparent SetABPenDrMd implementation */
void MUI_SetABPenDrMd(struct RastPort *rp, ULONG apen, ULONG bpen, UBYTE drawmode)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!rp) {
        return;
    }

    /* Always set in the rastport */
    SetABPenDrMd(rp, apen, bpen, drawmode);

    /* If this is our wrapper, update our tracking and HAL */
    if (IS_MUI_RASTPORT(rp)) {
        wrapper = GET_WRAPPER(rp);
        wrapper->current_pen = apen;
        wrapper->current_bpen = bpen;
        wrapper->current_drmd = drawmode;

        if (wrapper->mri && wrapper->mri->mri_HAL) {
            wrapper->mri->mri_HAL->set_ab_pen_drmd(rp, apen, bpen, drawmode);
        }
    }
}

/* Transparent SetDrMd implementation */
void MUI_SetDrMd(struct RastPort *rp, UBYTE drawmode)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!rp) {
        return;
    }

    /* Always set in the rastport */
    SetDrMd(rp, drawmode);

    /* If this is our wrapper, update our tracking */
    if (IS_MUI_RASTPORT(rp)) {
        wrapper = GET_WRAPPER(rp);
        wrapper->current_drmd = drawmode;
    }
}

/* Pattern fill with potential batching */
void MUI_RectFillPattern(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *pattern)
{
    struct MUI_RastPortWrapper *wrapper;

    if (!rp || !pattern)
        return;

    if (!IS_MUI_RASTPORT(rp)) {
        /* Not our wrapper, use traditional method */
        SetAfPt(rp, pattern, 1);
        RectFill(rp, x1, y1, x2, y2);
        SetAfPt(rp, NULL, 0);
        return;
    }

    wrapper = GET_WRAPPER(rp);
    if (!wrapper->mri || !wrapper->mri->mri_HAL) {
        /* Fall back to traditional */
        SetAfPt(rp, pattern, 1);
        RectFill(rp, x1, y1, x2, y2);
        SetAfPt(rp, NULL, 0);
        return;
    }

    /* Use HAL pattern drawing */
    if (!wrapper->immediate_mode && wrapper->batch) {
        /* For batching, we'd need to store pattern info - simplified for now */
        MUI_AddRectToBatch(wrapper->batch, x1, y1, x2, y2, wrapper->current_pen, BATCH_OP_PATTERN);
    } else {
        wrapper->mri->mri_HAL->draw_pattern(rp, x1, y1, x2, y2, pattern,
                                          wrapper->current_pen, wrapper->current_bpen);
    }
}

/* Alpha blending with potential pixel buffer acceleration */
void MUI_BlendRect(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, ULONG rgba_color, UBYTE alpha)
{
    struct MUI_RastPortWrapper *wrapper;
    LONG area;

    if (!rp || alpha == 0)
        return;

    area = (x2 - x1 + 1) * (y2 - y1 + 1);

    if (!IS_MUI_RASTPORT(rp)) {
        /* Not our wrapper - can't do advanced blending easily */
        /* Fall back to opaque fill */
        ULONG pen = 1; /* TODO: Convert RGBA to closest pen */
        SetAPen(rp, pen);
        RectFill(rp, x1, y1, x2, y2);
        return;
    }

    wrapper = GET_WRAPPER(rp);
    if (!wrapper->mri || !wrapper->mri->mri_HAL) {
        /* No HAL - fall back */
        ULONG pen = 1; /* TODO: Convert RGBA to closest pen */
        SetAPen(rp, pen);
        RectFill(rp, x1, y1, x2, y2);
        return;
    }

    /* Use pixel buffer for blending if beneficial */
    if (wrapper->use_pixelbuffer || MUI_ShouldUsePixelBuffer(NULL, OP_BLEND, area)) {
        MUI_PixelBuffer *pb = MUI_AcquirePixelBuffer(wrapper->mri, x2 + 1, y2 + 1);
        if (pb) {
            wrapper->mri->mri_HAL->pb_blend_rect(pb->buffer, pb->width, x1, y1, x2, y2, rgba_color, alpha);
            MUI_UpdateDirtyRect(&pb->dirty_rect, x1, y1, x2, y2);
            pb->dirty = TRUE;
            return;
        }
    }

    /* Fall back to opaque drawing if no pixel buffer available */
    if (alpha >= 128) {  /* More than half opaque */
        ULONG pen = wrapper->mri->mri_HAL->pen_to_rgba32(rgba_color, wrapper->mri);
        /* TODO: Convert RGBA back to pen number */
        pen = 1; /* Simplified */

        if (!wrapper->immediate_mode && wrapper->batch) {
            MUI_AddRectToBatch(wrapper->batch, x1, y1, x2, y2, pen, BATCH_OP_FILL);
        } else {
            wrapper->mri->mri_HAL->fill_rect(rp, x1, y1, x2, y2, pen);
        }
    }
    /* Skip drawing if too transparent */
}

/* Sync wrapper state with actual rastport */
void MUI_SyncWrapper(struct MUI_RastPortWrapper *wrapper)
{
    if (!wrapper || !IS_MUI_RASTPORT(&wrapper->rp))
        return;

    /* Update our tracking from the rastport */
    wrapper->current_pen = wrapper->rp.FgPen;
    wrapper->current_bpen = wrapper->rp.BgPen;
    wrapper->current_drmd = wrapper->rp.DrawMode;
}

/* Get current wrapper state */
ULONG MUI_GetWrapperPen(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        return wrapper->current_pen;
    }
    return 1; /* Default pen */
}

ULONG MUI_GetWrapperBPen(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        return wrapper->current_bpen;
    }
    return 0; /* Default background pen */
}

UBYTE MUI_GetWrapperDrawMode(struct MUI_RastPortWrapper *wrapper)
{
    if (wrapper && IS_MUI_RASTPORT(&wrapper->rp)) {
        return wrapper->current_drmd;
    }
    return JAM1; /* Default draw mode */
}
