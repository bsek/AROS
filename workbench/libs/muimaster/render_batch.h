/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.
*/

#ifndef _MUIMASTER_RENDER_BATCH_H
#define _MUIMASTER_RENDER_BATCH_H

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif
#ifndef GRAPHICS_RASTPORT_H
#   include <graphics/rastport.h>
#endif

#include "render_hal.h"

/* Forward declarations */
struct MUI_RenderInfo;
struct MUI_DrawBatch;
struct Rectangle;

/* Batch management functions */
struct MUI_DrawBatch *MUI_CreateBatch(LONG initial_capacity);
void MUI_FreeBatch(struct MUI_DrawBatch *batch);

/* Adding operations to batch */
void MUI_AddRectToBatch(struct MUI_DrawBatch *batch, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color, UBYTE operation);
void MUI_AddBlendRectToBatch(struct MUI_DrawBatch *batch, LONG x1, LONG y1, LONG x2, LONG y2, ULONG rgba_color, UBYTE alpha);

/* Batch execution */
void MUI_FlushBatch(struct MUI_RenderInfo *mri, struct MUI_DrawBatch *batch);
void MUI_ClearBatch(struct MUI_DrawBatch *batch);

/* Batch information */
BOOL MUI_BatchHasOperations(struct MUI_DrawBatch *batch);
LONG MUI_GetBatchCount(struct MUI_DrawBatch *batch);
LONG MUI_GetBatchCapacity(struct MUI_DrawBatch *batch);

/* Wrapper control functions */
void MUI_EnableBatchMode(struct MUI_RastPortWrapper *wrapper, struct MUI_DrawBatch *batch);
void MUI_DisableBatchMode(struct MUI_RastPortWrapper *wrapper);
void MUI_EnablePixelBuffer(struct MUI_RastPortWrapper *wrapper);
void MUI_DisablePixelBuffer(struct MUI_RastPortWrapper *wrapper);

/* Enhanced drawing functions */
void MUI_RectFillPattern(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *pattern);
void MUI_BlendRect(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, ULONG rgba_color, UBYTE alpha);

/* Wrapper state management */
void MUI_SyncWrapper(struct MUI_RastPortWrapper *wrapper);
ULONG MUI_GetWrapperPen(struct MUI_RastPortWrapper *wrapper);
ULONG MUI_GetWrapperBPen(struct MUI_RastPortWrapper *wrapper);
UBYTE MUI_GetWrapperDrawMode(struct MUI_RastPortWrapper *wrapper);

#endif /* _MUIMASTER_RENDER_BATCH_H */
