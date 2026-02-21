#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneFlushBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 21, zunegfx)

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
    ZuneBeginBatch(), ZuneEndBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneFlushBatch");

  D(bug("ZuneRenderer: ZuneFlushBatch(rp=%p)\n", rp));

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

  EXIT_FUNCTION("ZuneFlushBatch");

  AROS_LIBFUNC_EXIT
}
