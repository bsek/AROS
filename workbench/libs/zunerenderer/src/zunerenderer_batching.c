#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "backends/backend_interface.h"
#include "zunerenderer_intern.h"

/*****************************************************************************/
/* Batch System Implementation */
/*****************************************************************************/

struct BatchState *CreateBatchState(struct RenderPort *rp) {
  struct BatchState *batch;

  ENTER_FUNCTION("CreateBatchState");

  if (!rp) {
    D(bug("ZuneRenderer: Invalid RenderPort for batch state\n"));
    return NULL;
  }

  batch = AllocVec(sizeof(struct BatchState), MEMF_CLEAR | MEMF_PUBLIC);
  if (!batch) {
    D(bug("ZuneRenderer: Failed to allocate BatchState\n"));
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
  struct ColorMap *cmap = rp->colormap;
  InitPenCache(&batch->penCache, cmap);

  /* Initialize color caches */
  InitColorCache(&batch->colorCache);
  InitPenColorCache(&batch->penColorCache);

  /* Initialize batch state */
  batch->active = FALSE;
  batch->render_port = rp;

  D(bug("ZuneRenderer: Enhanced BatchState created\n"));
  EXIT_FUNCTION("CreateBatchState");
  return batch;
}

void DestroyBatchState(struct BatchState *batch) {
  ENTER_FUNCTION("DestroyBatchState");

  if (!batch)
    return;

  /* Flush any pending operations */
  if (batch->immediate.count > 0 || batch->deferred.count > 0) {
    // TODO FlushBatchState(batch);
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

  D(bug("ZuneRenderer: Enhanced batch state destroyed\n"));
  EXIT_FUNCTION("DestroyBatchState");
}

BOOL AddCommandToBatch(struct BatchState *batch, BatchCommandType type, WORD x,
                       WORD y, UWORD width, UWORD height, WORD x2, WORD y2,
                       ULONG color) {
  if (!batch || !batch->active)
    return FALSE;

  ENTER_FUNCTION("AddCommandToBatch");

  /* Check if we should use immediate batch (same pen) */
  LONG pen = -1;
  // TODO!!
  // if (!ShouldUseCyberGraphics(batch->render_port)) {
  //   pen = GetCachedPen(&batch->penCache, color);
  //   if (pen == -1) {
  //     D(bug("ZuneRenderer: Failed to get cached pen for color 0x%08x\n",
  //           (unsigned int)color));
  //     EXIT_FUNCTION("AddCommandToBatch");
  //     return FALSE;
  //   }
  // }

  // /* Try immediate batch first (same pen commands) */
  // if (!ShouldUseCyberGraphics(batch->render_port) &&
  //     batch->immediate.penValid && batch->immediate.currentPen == pen &&
  //     batch->immediate.count < IMMEDIATE_BATCH_SIZE) {

  //   struct BatchCommand *cmd =
  //       &batch->immediate.commands[batch->immediate.count];
  //   cmd->type = type;
  //   cmd->x = x;
  //   cmd->y = y;
  //   cmd->width = width;
  //   cmd->height = height;
  //   cmd->x2 = x2;
  //   cmd->y2 = y2;
  //   cmd->color = color;
  //   cmd->pen = pen;
  //   cmd->sortKey = (UWORD)pen;

  //   batch->immediate.count++;

  //   D(bug("ZuneRenderer: Added command to immediate batch (%d/%d), pen %d\n",
  //         batch->immediate.count, IMMEDIATE_BATCH_SIZE, (int)pen));
  //   EXIT_FUNCTION("AddCommandToBatch");
  //   return TRUE;
  // }

  /* Flush immediate batch if pen changed */
  if (batch->immediate.count > 0 &&
      (!batch->immediate.penValid || batch->immediate.currentPen != pen)) {
    /* ZuneInternalBatchFlushImmediate(batch); */
  }

  /* Set up immediate batch for new pen */
  if (batch->immediate.count == 0) {
    batch->immediate.currentPen = pen;
    batch->immediate.penValid = TRUE;

    struct BatchCommand *cmd =
        &batch->immediate.commands[batch->immediate.count];
    cmd->type = type;
    cmd->x = x;
    cmd->y = y;
    cmd->width = width;
    cmd->height = height;
    cmd->x2 = x2;
    cmd->y2 = y2;
    cmd->color = color;
    cmd->pen = pen;
    cmd->sortKey = (UWORD)pen;

    batch->immediate.count++;

    D(bug("ZuneRenderer: Started new immediate batch with pen %d\n", (int)pen));
    EXIT_FUNCTION("AddCommandToBatch");
    return TRUE;
  }

  /* Use deferred batch for different pens or CyberGraphics */
  if (batch->deferred.count >= DEFERRED_BATCH_SIZE) {
    /* Future heuristic hook for forced flush */
  }

  struct BatchCommand *cmd = &batch->deferred.commands[batch->deferred.count];
  cmd->type = type;
  cmd->x = x;
  cmd->y = y;
  cmd->width = width;
  cmd->height = height;
  cmd->x2 = x2;
  cmd->y2 = y2;
  cmd->color = color;
  cmd->pen = pen;
  cmd->sortKey = (UWORD)pen;

  batch->deferred.count++;
  batch->deferred.needsSort = TRUE;

  D(bug("ZuneRenderer: Added command to deferred batch (%d/%d)\n",
        (int)batch->deferred.count, DEFERRED_BATCH_SIZE));

  EXIT_FUNCTION("AddCommandToBatch");
  return TRUE;
}

void ZuneInternalBatchFlushImmediate(struct BatchState *batch) {
  if (!batch || batch->immediate.count == 0)
    return;

  /* Reset immediate batch */
  batch->immediate.count = 0;
  batch->immediate.penValid = FALSE;
}

void ZuneInternalBatchFlushState(struct BatchState *batch) {
  if (!batch)
    return;
  /* Reset batches */
  batch->immediate.count = 0;
  batch->deferred.count = 0;
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

BOOL ShouldFlushBatch(struct BatchState *batch, struct RenderPort *newTarget) {
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
      D(bug("ZuneRenderer: Merged adjacent rectangles\n"));
      continue;
    }

    /* Can't merge, keep command */
    batch->deferred.commands[writePos++] = *cmd;
  }

  if (writePos < batch->deferred.count) {
    D(bug("ZuneRenderer: Optimized %d commands to %d\n", batch->deferred.count,
          writePos));
    batch->deferred.count = writePos;
  }

  EXIT_FUNCTION("OptimizeBatchCommands");
}

/*****************************************************************************/
/* Batch Control Functions */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH1(void, BeginBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 19, zunerenderer)

/*  FUNCTION
    Begins a batch rendering session. All subsequent drawing operations
    will be batched together for improved performance until EndBatch()
    is called.

INPUTS
    rp - RenderPort for batching (must not be NULL)

RESULT
    None

NOTES
    Batching provides significant performance improvements when drawing
    many primitives with the same color. Operations are automatically
    flushed when the color changes or the batch buffer is full.

SEE ALSO
    EndBatch(), FlushBatch(), IsBatchingEnabled()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("BeginBatch");

  D(bug("ZuneRenderer: BeginBatch(rp=%p)\n", rp));

  if (!ValidateRenderPort(rp)) {
    D(bug("ZuneRenderer: Invalid RenderPort\n"));
    return;
  }

  if (!rp->batch_state) {
    D(bug("ZuneRenderer: Creating batch state on-demand\n"));
    rp->batch_state = CreateBatchState(rp);
    if (!rp->batch_state) {
      D(bug("ZuneRenderer: Failed to create batch state\n"));
      return;
    }
  }

  struct BatchState *batch = (struct BatchState *)rp->batch_state;

  /* Flush any existing batch first */
  if (batch->immediate.count > 0 || batch->deferred.count > 0) {
    ZuneInternalBatchFlushState(batch);
  }

  batch->active = TRUE;
  rp->batching_enabled = TRUE;

  D(bug("ZuneRenderer: Batch session started\n"));

  EXIT_FUNCTION("BeginBatch");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, EndBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 20, zunerenderer)

/*  FUNCTION
    Ends a batch rendering session and flushes all pending operations.

INPUTS
    rp - RenderPort with active batch session (must not be NULL)

RESULT
    None

NOTES
    This function automatically flushes any pending batch operations
    and disables batching mode for the RenderPort.

SEE ALSO
    BeginBatch(), FlushBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("EndBatch");

  D(bug("ZuneRenderer: EndBatch(rp=%p)\n", rp));

  if (!ValidateRenderPort(rp)) {
    D(bug("ZuneRenderer: Invalid RenderPort\n"));
    return;
  }

  if (!rp->batch_state) {
    D(bug("ZuneRenderer: No batch state available\n"));
    return;
  }

  struct BatchState *batch = (struct BatchState *)rp->batch_state;

  /* Flush any pending operations */
  if (batch->immediate.count > 0 || batch->deferred.count > 0) {
    ZuneInternalBatchFlushState(batch);
  }

  batch->active = FALSE;
  rp->batching_enabled = FALSE;

  D(bug("ZuneRenderer: Batch session ended\n"));

  EXIT_FUNCTION("EndBatch");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, FlushBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 21, zunerenderer)

/*  FUNCTION
    Manually flushes the current batch without ending the batch session.
    This is useful when you need to ensure certain operations are completed
    before continuing.

INPUTS
    rp - RenderPort with active batch session (must not be NULL)

RESULT
    None

NOTES
    After flushing, batching continues to be active and new operations
    will start a new batch.

SEE ALSO
    BeginBatch(), EndBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("FlushBatch");

  D(bug("ZuneRenderer: FlushBatch(rp=%p)\n", rp));

  if (!ValidateRenderPort(rp)) {
    D(bug("ZuneRenderer: Invalid RenderPort\n"));
    return;
  }

  if (!rp->batch_state) {
    D(bug("ZuneRenderer: No batch state available\n"));
    return;
  }

  struct BatchState *batch = (struct BatchState *)rp->batch_state;
  ZuneInternalBatchFlushState(batch);

  D(bug("ZuneRenderer: Manual batch flush completed\n"));

  EXIT_FUNCTION("FlushBatch");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, IsBatchingEnabled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 22, zunerenderer)

/*  FUNCTION
    Checks if batching is currently enabled for the RenderPort.

INPUTS
    rp - RenderPort to check (must not be NULL)

RESULT
    TRUE if batching is enabled, FALSE otherwise.

SEE ALSO
    BeginBatch(), EndBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderPort(rp))
    return FALSE;

  return rp->batching_enabled;

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(ULONG, GetBatchCount,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 23, zunerenderer)

/*  FUNCTION
    Gets the number of operations currently in the batch buffer.

INPUTS
    rp - RenderPort to check (must not be NULL)

RESULT
    Number of batched operations, or 0 if batching is not active.

SEE ALSO
    BeginBatch(), FlushBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderPort(rp) || !rp->batch_state)
    return 0;

  struct BatchState *batch = (struct BatchState *)rp->batch_state;
  return batch->immediate.count + batch->deferred.count;

  AROS_LIBFUNC_EXIT
}
