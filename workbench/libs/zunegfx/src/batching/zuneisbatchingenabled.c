#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ZuneIsBatchingEnabled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 22, zunegfx)

/*  FUNCTION
    Checks if batching is currently enabled for the RenderContext.

INPUTS
    rctx - RenderContext to check (must not be NULL)

RESULT
    TRUE if batching is enabled, FALSE otherwise.

SEE ALSO
    ZuneBeginBatch(), ZuneEndBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderContext(rctx))
    return FALSE;

  return rctx->batching_enabled;

  AROS_LIBFUNC_EXIT
}
