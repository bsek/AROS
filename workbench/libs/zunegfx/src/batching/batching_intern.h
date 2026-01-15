#ifndef BATCHING_INTERN_H
#define BATCHING_INTERN_H

#include <exec/types.h>

/* Forward declarations */
struct BatchState;
struct RenderPort;
struct BatchCommand;

/* Internal batch functions */
struct BatchState *CreateBatchState(struct RenderPort *rp);
void DestroyBatchState(struct BatchState *batch);
BOOL AddCommandToBatch(struct BatchState *batch, BatchCommandType type, WORD x,
                       WORD y, UWORD width, UWORD height, WORD x2, WORD y2,
                       ULONG color);
void ZuneInternalBatchFlushImmediate(struct BatchState *batch);
void ZuneInternalBatchFlushState(struct BatchState *batch);
void ZuneInternalBatchSortCommandsByPen(struct BatchCommand *commands,
                                        UWORD count);
BOOL ShouldFlushBatch(struct BatchState *batch, struct RenderPort *newTarget);
void OptimizeBatchCommands(struct BatchState *batch);

#endif /* BATCHING_INTERN_H */
