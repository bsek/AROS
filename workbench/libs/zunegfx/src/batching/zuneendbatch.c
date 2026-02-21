#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneEndBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 20, zunegfx)

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
    ZuneBeginBatch(), ZuneFlushBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneEndBatch");

  D(bug("ZuneRenderer: ZuneEndBatch(rp=%p)\n", rp));

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

  EXIT_FUNCTION("ZuneEndBatch");

  AROS_LIBFUNC_EXIT
}
