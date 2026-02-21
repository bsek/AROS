#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneBeginBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 19, zunegfx)

/*  FUNCTION
    Begins a batch rendering session. All subsequent drawing operations
    will be batched together for improved performance until ZuneEndBatch()
    is called.

INPUTS
    rctx - RenderContext for batching (must not be NULL)

RESULT
    None

NOTES
    Batching provides significant performance improvements when drawing
    many primitives with the same color. Operations are automatically
    flushed when the color changes or the batch buffer is full.

SEE ALSO
    ZuneEndBatch(), ZuneFlushBatch(), ZuneIsBatchingEnabled()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneBeginBatch");

  D(bug("ZuneRenderer: ZuneBeginBatch(rctx=%p)\n", rctx));

  if (!ValidateRenderContext(rctx)) {
    D(bug("ZuneRenderer: Invalid RenderContext\n"));
    return;
  }

  if (!rctx->batch_state) {
    D(bug("ZuneRenderer: Creating batch state on-demand\n"));
    rctx->batch_state = CreateBatchState(rctx);
    if (!rctx->batch_state) {
      D(bug("ZuneRenderer: Failed to create batch state\n"));
      return;
    }
  }

  struct BatchState *batch = (struct BatchState *)rctx->batch_state;

  /* Flush any existing batch first */
  if (batch->immediate.count > 0 || batch->deferred.count > 0) {
    ZuneInternalBatchFlushState(batch);
  }

  batch->active = TRUE;
  rctx->batching_enabled = TRUE;

  /* Notify the backend (e.g. OpenGL defers swapbuffers) */
  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->BeginBatch) {
    backend->ops->BeginBatch(rctx);
  }

  D(bug("ZuneRenderer: Batch session started\n"));

  EXIT_FUNCTION("ZuneBeginBatch");

  AROS_LIBFUNC_EXIT
}
