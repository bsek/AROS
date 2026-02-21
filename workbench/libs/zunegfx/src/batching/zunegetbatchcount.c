#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(ULONG, ZuneGetBatchCount,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 23, zunegfx)

/*  FUNCTION
    Gets the number of operations currently in the batch buffer.

INPUTS
    rp - RenderPort to check (must not be NULL)

RESULT
    Number of batched operations, or 0 if batching is not active.

SEE ALSO
    ZuneBeginBatch(), ZuneFlushBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderPort(rp) || !rp->batch_state)
    return 0;

  struct BatchState *batch = (struct BatchState *)rp->batch_state;
  return batch->immediate.count + batch->deferred.count;

  AROS_LIBFUNC_EXIT
}
