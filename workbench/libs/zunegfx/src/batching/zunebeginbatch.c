#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, BeginBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 19, zunegfx)

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
