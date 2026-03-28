#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************/
/* Batch System Implementation */
/*****************************************************************************/

struct BatchState *CreateBatchState(struct RenderContext *rctx) {
  struct BatchState *batch;

  ENTER_FUNCTION("CreateBatchState");

  if (!rctx) {
    D(bug("ZuneGfx: Invalid RenderContext for batch state\n"));
    return NULL;
  }

  batch = AllocVec(sizeof(struct BatchState), MEMF_CLEAR | MEMF_PUBLIC);
  if (!batch) {
    D(bug("ZuneGfx: Failed to allocate BatchState\n"));
    return NULL;
  }

  /* Initialize immediate batch */
  batch->immediate.count = 0;
  batch->immediate.currentPen = -1;
  batch->immediate.penValid = FALSE;

  /* Initialize deferred batch */
  batch->deferred.count = 0;
  batch->deferred.needsSort = FALSE;

  /* Initialize pixel batch */
  batch->pixelBatch.pixelBuffer = NULL;
  batch->pixelBatch.dirtyCount = 0;
  batch->pixelBatch.needsFlush = FALSE;

  /* Initialize pen cache */
  struct ColorMap *cmap = rctx->colormap;
  InitPenCache(&batch->penCache, cmap);

  /* Initialize color caches */
  InitColorCache(&batch->colorCache);
  InitPenColorCache(&batch->penColorCache);

  /* Initialize batch state */
  batch->active = FALSE;
  batch->render_port = rctx;

  D(bug("ZuneGfx: Enhanced BatchState created\n"));
  EXIT_FUNCTION("CreateBatchState");
  return batch;
}

void DestroyBatchState(struct BatchState *batch) {
  ENTER_FUNCTION("DestroyBatchState");

  if (!batch)
    return;

  /* Flush any pending operations */
  if (batch->deferred.count > 0) {
    ExecuteBatchCommands(batch);
  }

  /* Cleanup pen cache */
  CleanupPenCache(&batch->penCache);

  /* Cleanup color caches */
  CleanupColorCache(&batch->colorCache);
  CleanupPenColorCache(&batch->penColorCache);

  /* Clear and free */
  /* Reset immediate batch */
  batch->immediate.count = 0;
  batch->immediate.penValid = FALSE;

  /* Reset deferred batch */
  batch->deferred.count = 0;
  batch->deferred.needsSort = FALSE;

  /* Reset pixel batch */
  batch->pixelBatch.dirtyCount = 0;
  batch->pixelBatch.needsFlush = FALSE;
  FreeVec(batch);

  D(bug("ZuneGfx: Enhanced batch state destroyed\n"));
  EXIT_FUNCTION("DestroyBatchState");
}

BOOL AddCommandToBatch(struct BatchState *batch, BatchCommandType type, WORD x,
                       WORD y, UWORD width, UWORD height, WORD x2, WORD y2,
                       ULONG color) {
  if (!batch || !batch->active)
    return FALSE;

  if (batch->deferred.count >= DEFERRED_BATCH_SIZE)
    return FALSE;

  struct BatchCommand *cmd = &batch->deferred.commands[batch->deferred.count];
  cmd->type = type;
  cmd->x = x;
  cmd->y = y;
  cmd->width = width;
  cmd->height = height;
  cmd->x2 = x2;
  cmd->y2 = y2;
  cmd->color = color;
  cmd->pen = -1;
  cmd->sortKey = color;
  cmd->border_width = 0;

  batch->deferred.count++;
  batch->deferred.needsSort = TRUE;

  D(bug("ZuneGfx: Batched command type %d (%d/%d)\n",
        (LONG)type, (LONG)batch->deferred.count, DEFERRED_BATCH_SIZE));

  return TRUE;
}

BOOL AddStyledCommandToBatch(struct BatchState *batch, BatchCommandType type,
                             WORD x, WORD y, UWORD width, UWORD height,
                             UBYTE border_width, ULONG color) {
  if (!batch || !batch->active)
    return FALSE;

  if (batch->deferred.count >= DEFERRED_BATCH_SIZE)
    return FALSE;

  struct BatchCommand *cmd = &batch->deferred.commands[batch->deferred.count];
  cmd->type = type;
  cmd->x = x;
  cmd->y = y;
  cmd->width = width;
  cmd->height = height;
  cmd->x2 = 0;
  cmd->y2 = 0;
  cmd->color = color;
  cmd->pen = -1;
  cmd->sortKey = color;
  cmd->border_width = border_width;

  batch->deferred.count++;
  batch->deferred.needsSort = TRUE;

  D(bug("ZuneGfx: Batched styled command type %d bw=%d (%d/%d)\n",
        (LONG)type, (LONG)border_width,
        (LONG)batch->deferred.count, DEFERRED_BATCH_SIZE));

  return TRUE;
}

void ZuneInternalBatchFlushImmediate(struct BatchState *batch) {
  if (!batch || batch->immediate.count == 0)
    return;

  /* Reset immediate batch */
  batch->immediate.count = 0;
  batch->immediate.penValid = FALSE;
}

void ExecuteBatchCommands(struct BatchState *batch) {
  if (!batch || !batch->render_port || batch->deferred.count == 0)
    return;

  struct RenderContext *rctx = batch->render_port;

  D(bug("ZuneGfx: Executing %d batched commands\n",
        (LONG)batch->deferred.count));

  /* Optimize: merge adjacent same-color fill rects */
  OptimizeBatchCommands(batch);

  /* Execute commands via backend */
  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (!backend || !backend->ops) {
    D(bug("ZuneGfx: No backend for batch execution\n"));
    batch->deferred.count = 0;
    return;
  }

  for (UWORD i = 0; i < batch->deferred.count; i++) {
    struct BatchCommand *cmd = &batch->deferred.commands[i];
    struct InternalColor ic =
        ZuneColorToInternal(rctx, cmd->color, rctx->pixel_format);

    switch (cmd->type) {
    case BATCH_CMD_FILL_RECT:
      if (backend->ops->DrawRectangle) {
        struct ZuneBrush fill_brush = ZUNE_BRUSH_LITERAL_SOLID(cmd->color);
        backend->ops->DrawRectangle(rctx, cmd->x, cmd->y, cmd->width,
                                    cmd->height, 0, 0, &fill_brush, &ic, TRUE, FALSE);
      }
      break;
    case BATCH_CMD_DRAW_RECT:
      if (backend->ops->DrawRectangle)
        backend->ops->DrawRectangle(rctx, cmd->x, cmd->y, cmd->width,
                                    cmd->height, 1, 0, NULL, &ic, FALSE,
                                    FALSE);
      break;
    case BATCH_CMD_DRAW_RECT_STYLED:
      if (backend->ops->DrawRectangle)
        backend->ops->DrawRectangle(rctx, cmd->x, cmd->y, cmd->width,
                                    cmd->height, cmd->border_width, 0, NULL,
                                    &ic, FALSE, FALSE);
      break;
    case BATCH_CMD_DRAW_LINE:
      if (backend->ops->DrawLine)
        backend->ops->DrawLine(rctx, cmd->x, cmd->y, cmd->x2, cmd->y2, 1, &ic,
                               FALSE);
      break;
    case BATCH_CMD_DRAW_PIXEL:
      if (backend->ops->DrawPixel)
        backend->ops->DrawPixel(rctx, cmd->x, cmd->y, &ic, FALSE);
      break;
    }
  }

  D(bug("ZuneGfx: Batch execution complete\n"));

  /* Reset counters */
  batch->immediate.count = 0;
  batch->deferred.count = 0;
}

void ZuneInternalBatchFlushState(struct BatchState *batch) {
  ExecuteBatchCommands(batch);
}

/*****************************************************************************/
/* Batch Optimization Functions */
/*****************************************************************************/

void ZuneInternalBatchSortCommandsByPen(struct BatchCommand *commands,
                                        UWORD count) {
  /* Simple sort placeholder - for now just return */
  (void)commands;
  (void)count;
}

BOOL ShouldFlushBatch(struct BatchState *batch, struct RenderContext *newTarget) {
  if (!batch || !batch->active)
    return FALSE;

  /* Flush if target changed */
  if (batch->render_port != newTarget)
    return TRUE;

  /* Flush if deferred batch is full */
  if (batch->deferred.count >= DEFERRED_BATCH_SIZE)
    return TRUE;

  /* Flush if immediate batch is full and we can't flush it separately */
  if (batch->immediate.count >= IMMEDIATE_BATCH_SIZE)
    return TRUE;

  return FALSE;
}

void OptimizeBatchCommands(struct BatchState *batch) {
  if (!batch || batch->deferred.count <= 1)
    return;

  ENTER_FUNCTION("OptimizeBatchCommands");

  /* Simple optimization: merge adjacent fill rectangles with same color */
  UWORD writePos = 0;

  for (UWORD readPos = 0; readPos < batch->deferred.count; readPos++) {
    struct BatchCommand *cmd = &batch->deferred.commands[readPos];

    if (readPos == 0) {
      batch->deferred.commands[writePos++] = *cmd;
      continue;
    }

    struct BatchCommand *lastCmd = &batch->deferred.commands[writePos - 1];

    /* Try to merge adjacent rectangles */
    if (cmd->type == BATCH_CMD_FILL_RECT &&
        lastCmd->type == BATCH_CMD_FILL_RECT && cmd->color == lastCmd->color &&
        cmd->pen == lastCmd->pen && cmd->y == lastCmd->y &&
        cmd->height == lastCmd->height &&
        cmd->x == lastCmd->x + lastCmd->width) {

      /* Merge rectangles */
      lastCmd->width += cmd->width;
      D(bug("ZuneGfx: Merged adjacent rectangles\n"));
      continue;
    }

    /* Can't merge, keep command */
    batch->deferred.commands[writePos++] = *cmd;
  }

  if (writePos < batch->deferred.count) {
    D(bug("ZuneGfx: Optimized %d commands to %d\n", batch->deferred.count,
          writePos));
    batch->deferred.count = writePos;
  }

  EXIT_FUNCTION("OptimizeBatchCommands");
}
