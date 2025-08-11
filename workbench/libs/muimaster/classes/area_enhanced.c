/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.

    Enhanced Area class with HAL integration and batch drawing support.
    This file provides the infrastructure for transparent optimization
    while maintaining full compatibility with existing MUI classes.
*/

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "support_classes.h"
#include "render_hal.h"
#include "render_batch.h"

/* Enhanced render info structure for Area objects */
struct MUI_AreaRenderData {
    struct MUI_RastPortWrapper *wrapper;    /* Wrapped rastport for interception */
    struct MUI_DrawBatch *batch;           /* Drawing batch for this object */
    BOOL batch_mode_active;                /* Is batch mode currently active */
    BOOL pixelbuffer_mode_active;          /* Is pixel buffer mode active */
    ULONG optimization_threshold;          /* Area size threshold for optimization */
    ULONG last_draw_operation_count;       /* Number of operations in last draw */
};

/* Decision thresholds */
#define AREA_BATCH_THRESHOLD        5      /* Min operations to enable batching */
#define AREA_PIXELBUFFER_THRESHOLD  2000   /* Min area size for pixel buffer */
#define COMPLEX_OBJECT_THRESHOLD    10     /* Min children for complex object */

/* Forward declarations */
static BOOL MUI_ShouldOptimizeArea(Object *obj);
static void MUI_StartAreaOptimization(Object *obj);
static void MUI_EndAreaOptimization(Object *obj);
static struct MUI_AreaRenderData *MUI_GetAreaRenderData(Object *obj);
static void MUI_InitAreaRenderData(Object *obj);
static void MUI_CleanupAreaRenderData(Object *obj);

/* Enhanced Area Setup method */
IPTR Area_Enhanced__MUIM_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
    IPTR result;
    struct MUI_RenderInfo *mri;

    /* Call original setup first */
    result = DoSuperMethodA(cl, obj, (Msg)msg);
    if (!result)
        return FALSE;

    /* Initialize render HAL if not already done */
    mri = muiRenderInfo(obj);
    if (mri && !mri->mri_HAL) {
        mri->mri_HAL = MUI_DetectRenderCapabilities();
        if (!mri->mri_HAL) {
            /* HAL initialization failed - continue without optimization */
            return result;
        }

        /* Initialize pixel buffer structure */
        memset(&mri->mri_PixelBuffer, 0, sizeof(MUI_PixelBuffer));
        mri->mri_CurrentBatch = NULL;
        mri->mri_BatchMode = FALSE;
    }

    /* Initialize area-specific render data */
    MUI_InitAreaRenderData(obj);

    return result;
}

/* Enhanced Area Cleanup method */
IPTR Area_Enhanced__MUIM_Cleanup(struct IClass *cl, Object *obj, struct MUIP_Cleanup *msg)
{
    struct MUI_RenderInfo *mri;

    /* Cleanup area-specific render data */
    MUI_CleanupAreaRenderData(obj);

    /* Cleanup render HAL if this is the last object using it */
    mri = muiRenderInfo(obj);
    if (mri && mri->mri_HAL) {
        /* Flush any pending pixel buffer operations */
        if (mri->mri_PixelBuffer.dirty) {
            MUI_FlushPixelBuffer(mri);
        }

        /* Free pixel buffer */
        if (mri->mri_PixelBuffer.buffer) {
            FreeVec(mri->mri_PixelBuffer.buffer);
            mri->mri_PixelBuffer.buffer = NULL;
        }

        /* Note: Don't free HAL here as it might be shared between objects */
        /* The window cleanup should handle HAL cleanup */
    }

    /* Call original cleanup */
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* Enhanced Area Show method with rastport wrapping */
IPTR Area_Enhanced__MUIM_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg)
{
    IPTR result;
    struct MUI_RenderInfo *mri;
    struct MUI_AreaRenderData *ard;

    /* Call original show method first */
    result = DoSuperMethodA(cl, obj, (Msg)msg);
    if (!result)
        return FALSE;

    /* Set up rastport wrapper for transparent interception */
    mri = muiRenderInfo(obj);
    ard = MUI_GetAreaRenderData(obj);

    if (mri && mri->mri_HAL && ard && _rp(obj)) {
        /* Create wrapper around the rastport */
        ard->wrapper = MUI_CreateRastPortWrapper(mri, _rp(obj));
        if (ard->wrapper) {
            /* Replace the object's rastport with our wrapper */
            /* Note: This is a delicate operation that requires careful implementation */
            /* For now, we'll use the wrapper selectively in draw operations */
        }
    }

    return result;
}

/* Enhanced Area Hide method */
IPTR Area_Enhanced__MUIM_Hide(struct IClass *cl, Object *obj, struct MUIP_Hide *msg)
{
    struct MUI_AreaRenderData *ard;

    /* Clean up wrapper */
    ard = MUI_GetAreaRenderData(obj);
    if (ard && ard->wrapper) {
        MUI_FreeRastPortWrapper(ard->wrapper);
        ard->wrapper = NULL;
    }

    /* Call original hide method */
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* Enhanced Area Draw method with optimization */
IPTR Area_Enhanced__MUIM_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
    IPTR result;
    struct MUI_RenderInfo *mri;
    struct MUI_AreaRenderData *ard;
    BOOL should_optimize;
    ULONG operation_count_before = 0;

    mri = muiRenderInfo(obj);
    ard = MUI_GetAreaRenderData(obj);

    if (!mri || !ard) {
        /* No optimization possible - use original method */
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }

    /* Decide if we should optimize this draw operation */
    should_optimize = MUI_ShouldOptimizeArea(obj);

    if (should_optimize) {
        /* Count operations before */
        if (ard->batch) {
            operation_count_before = MUI_GetBatchCount(ard->batch);
        }

        /* Start optimization */
        MUI_StartAreaOptimization(obj);
    }

    /* Call the original/superclass draw method */
    /* This will execute the actual drawing code from derived classes */
    result = DoSuperMethodA(cl, obj, (Msg)msg);

    if (should_optimize) {
        /* Track operation count for future decisions */
        if (ard->batch) {
            ard->last_draw_operation_count = MUI_GetBatchCount(ard->batch) - operation_count_before;
        }

        /* End optimization and flush operations */
        MUI_EndAreaOptimization(obj);
    }

    return result;
}

/* Initialize area-specific render data */
static void MUI_InitAreaRenderData(Object *obj)
{
    struct MUI_AreaRenderData *ard;

    /* Allocate render data structure */
    ard = AllocVec(sizeof(struct MUI_AreaRenderData), MEMF_CLEAR);
    if (!ard)
        return;

    /* Initialize defaults */
    ard->wrapper = NULL;
    ard->batch = NULL;
    ard->batch_mode_active = FALSE;
    ard->pixelbuffer_mode_active = FALSE;
    ard->optimization_threshold = AREA_PIXELBUFFER_THRESHOLD;
    ard->last_draw_operation_count = 0;

    /* Store in object's user data (this is simplified - real implementation
       would need proper storage mechanism) */
    /* TODO: Integrate with existing MUI_AreaData structure */
}

/* Get area render data from object */
static struct MUI_AreaRenderData *MUI_GetAreaRenderData(Object *obj)
{
    /* TODO: Retrieve from object's user data or MUI_AreaData structure */
    /* For now, return NULL to disable optimization */
    return NULL;
}

/* Cleanup area-specific render data */
static void MUI_CleanupAreaRenderData(Object *obj)
{
    struct MUI_AreaRenderData *ard = MUI_GetAreaRenderData(obj);

    if (!ard)
        return;

    /* Cleanup wrapper */
    if (ard->wrapper) {
        MUI_FreeRastPortWrapper(ard->wrapper);
    }

    /* Cleanup batch */
    if (ard->batch) {
        MUI_FreeBatch(ard->batch);
    }

    /* Free the structure */
    FreeVec(ard);

    /* TODO: Clear from object's user data */
}

/* Decide if area should be optimized */
static BOOL MUI_ShouldOptimizeArea(Object *obj)
{
    struct MUI_RenderInfo *mri;
    struct MUI_AreaRenderData *ard;
    LONG area_size;
    LONG child_count = 0;

    mri = muiRenderInfo(obj);
    ard = MUI_GetAreaRenderData(obj);

    if (!mri || !mri->mri_HAL || !ard)
        return FALSE;

    /* Don't optimize if HAL doesn't support advanced features */
    if (!(mri->mri_HAL->capabilities & (RENDER_CAP_BATCH | RENDER_CAP_PIXELBUFFER)))
        return FALSE;

    /* Calculate area size */
    area_size = _width(obj) * _height(obj);

    /* Check if area is large enough */
    if (area_size > ard->optimization_threshold)
        return TRUE;

    /* Check if object has many children (complex layout) */
    get(obj, MUIA_Group_ChildCount, &child_count);
    if (child_count > COMPLEX_OBJECT_THRESHOLD)
        return TRUE;

    /* Check if previous draw had many operations */
    if (ard->last_draw_operation_count > AREA_BATCH_THRESHOLD)
        return TRUE;

    /* Check if this is a known complex object type */
    if (muiAreaData(obj)->mad_Frame != MUIV_Frame_None)
        return TRUE;

    return FALSE;
}

/* Start area optimization */
static void MUI_StartAreaOptimization(Object *obj)
{
    struct MUI_RenderInfo *mri;
    struct MUI_AreaRenderData *ard;
    LONG area_size;

    mri = muiRenderInfo(obj);
    ard = MUI_GetAreaRenderData(obj);

    if (!mri || !ard)
        return;

    area_size = _width(obj) * _height(obj);

    /* Create batch if not exists */
    if (!ard->batch) {
        ard->batch = MUI_CreateBatch(INITIAL_BATCH_CAPACITY);
        if (!ard->batch)
            return; /* Out of memory - continue without batching */
    }

    /* Enable batching if supported */
    if (mri->mri_HAL->capabilities & RENDER_CAP_BATCH) {
        if (ard->wrapper) {
            MUI_EnableBatchMode(ard->wrapper, ard->batch);
            ard->batch_mode_active = TRUE;
        }

        /* Set global batch mode for this render info */
        mri->mri_CurrentBatch = ard->batch;
        mri->mri_BatchMode = TRUE;
    }

    /* Enable pixel buffer for large areas */
    if ((mri->mri_HAL->capabilities & RENDER_CAP_PIXELBUFFER) &&
        area_size > AREA_PIXELBUFFER_THRESHOLD) {

        if (ard->wrapper) {
            MUI_EnablePixelBuffer(ard->wrapper);
            ard->pixelbuffer_mode_active = TRUE;
        }
    }
}

/* End area optimization and flush operations */
static void MUI_EndAreaOptimization(Object *obj)
{
    struct MUI_RenderInfo *mri;
    struct MUI_AreaRenderData *ard;

    mri = muiRenderInfo(obj);
    ard = MUI_GetAreaRenderData(obj);

    if (!mri || !ard)
        return;

    /* Disable batch mode */
    if (ard->batch_mode_active && ard->wrapper) {
        MUI_DisableBatchMode(ard->wrapper);
        ard->batch_mode_active = FALSE;
    }

    /* Disable pixel buffer mode */
    if (ard->pixelbuffer_mode_active && ard->wrapper) {
        MUI_DisablePixelBuffer(ard->wrapper);
        ard->pixelbuffer_mode_active = FALSE;
    }

    /* Flush batch operations */
    if (ard->batch && MUI_BatchHasOperations(ard->batch)) {
        MUI_FlushBatch(mri, ard->batch);
    }

    /* Flush pixel buffer */
    if (mri->mri_PixelBuffer.dirty) {
        MUI_FlushPixelBuffer(mri);
    }

    /* Clear global batch mode */
    mri->mri_CurrentBatch = NULL;
    mri->mri_BatchMode = FALSE;
}

/* Enhanced drawing helper functions that can be used by derived classes */

/* Enhanced RectFill with automatic optimization */
void MUI_Enhanced_RectFill(Object *obj, LONG x1, LONG y1, LONG x2, LONG y2, ULONG pen)
{
    struct RastPort *rp = _rp(obj);

    if (!rp)
        return;

    /* Use our transparent wrapper function */
    MUI_SetAPen(rp, pen);
    MUI_RectFill(rp, x1, y1, x2, y2);
}

/* Enhanced pattern fill */
void MUI_Enhanced_RectFillPattern(Object *obj, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *pattern, ULONG fg, ULONG bg)
{
    struct RastPort *rp = _rp(obj);

    if (!rp || !pattern)
        return;

    /* Use our transparent wrapper function */
    MUI_SetABPenDrMd(rp, fg, bg, JAM2);
    MUI_RectFillPattern(rp, x1, y1, x2, y2, pattern);
}

/* Enhanced alpha blending */
void MUI_Enhanced_BlendRect(Object *obj, LONG x1, LONG y1, LONG x2, LONG y2, UBYTE r, UBYTE g, UBYTE b, UBYTE alpha)
{
    struct RastPort *rp = _rp(obj);
    struct MUI_RenderInfo *mri;
    ULONG rgba_color;

    if (!rp || alpha == 0)
        return;

    mri = muiRenderInfo(obj);
    if (!mri || !mri->mri_HAL)
        return;

    /* Convert to RGBA32 format */
    rgba_color = mri->mri_HAL->rgb_to_rgba32(r, g, b, alpha);

    /* Use our transparent wrapper function */
    MUI_BlendRect(rp, x1, y1, x2, y2, rgba_color, alpha);
}

/* Query optimization status */
BOOL MUI_Enhanced_IsOptimizationActive(Object *obj)
{
    struct MUI_AreaRenderData *ard = MUI_GetAreaRenderData(obj);

    return (ard && (ard->batch_mode_active || ard->pixelbuffer_mode_active));
}

/* Get optimization statistics */
ULONG MUI_Enhanced_GetLastOperationCount(Object *obj)
{
    struct MUI_AreaRenderData *ard = MUI_GetAreaRenderData(obj);

    return ard ? ard->last_draw_operation_count : 0;
}
