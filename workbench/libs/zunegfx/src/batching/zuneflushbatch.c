#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneFlushBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 21, zunegfx)

/*  FUNCTION
    Manually flushes the current batch without ending the batch session.
    This is useful when you need to ensure certain operations are completed
    before continuing.

INPUTS
    rctx - RenderContext with active batch session (must not be NULL)

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

  D(bug("ZuneGfx: ZuneFlushBatch(rctx=%p)\n", rctx));

  if (!ValidateRenderContext(rctx)) {
    D(bug("ZuneGfx: Invalid RenderContext\n"));
    return;
  }

  if (!rctx->batch_state) {
    D(bug("ZuneGfx: No batch state available\n"));
    return;
  }

  struct BatchState *batch = (struct BatchState *)rctx->batch_state;
  ZuneInternalBatchFlushState(batch);

  /* Notify the backend (e.g. OpenGL does glFlush + swapbuffers) */
  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->FlushBatch) {
    backend->ops->FlushBatch(rctx);
  }

  D(bug("ZuneGfx: Manual batch flush completed\n"));

  EXIT_FUNCTION("ZuneFlushBatch");

  AROS_LIBFUNC_EXIT
}
