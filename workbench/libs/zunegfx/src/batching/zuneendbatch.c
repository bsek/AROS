#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneEndBatch,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 20, zunegfx)

/*  FUNCTION
    Ends a batch rendering session and flushes all pending operations.

INPUTS
    rctx - RenderContext with active batch session (must not be NULL)

RESULT
    None

NOTES
    This function automatically flushes any pending batch operations
    and disables batching mode for the RenderContext.

SEE ALSO
    ZuneBeginBatch(), ZuneFlushBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneEndBatch");

  D(bug("ZuneGfx: ZuneEndBatch(rctx=%p)\n", rctx));

  if (!ValidateRenderContext(rctx)) {
    D(bug("ZuneGfx: Invalid RenderContext\n"));
    return;
  }

  if (!rctx->batch_state) {
    D(bug("ZuneGfx: No batch state available\n"));
    return;
  }

  struct BatchState *batch = (struct BatchState *)rctx->batch_state;

  /* Flush any pending operations */
  if (batch->immediate.count > 0 || batch->deferred.count > 0) {
    ZuneInternalBatchFlushState(batch);
  }

  batch->active = FALSE;
  rctx->batching_enabled = FALSE;

  /* Notify the backend (e.g. OpenGL does glFlush + swapbuffers) */
  ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
  if (backend && backend->ops && backend->ops->EndBatch) {
    backend->ops->EndBatch(rctx);
  }

  D(bug("ZuneGfx: Batch session ended\n"));

  EXIT_FUNCTION("ZuneEndBatch");

  AROS_LIBFUNC_EXIT
}
