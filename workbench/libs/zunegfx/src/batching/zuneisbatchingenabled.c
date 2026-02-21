#include <aros/debug.h>
#include <aros/libcall.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "batching_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ZuneIsBatchingEnabled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 22, zunegfx)

/*  FUNCTION
    Checks if batching is currently enabled for the RenderPort.

INPUTS
    rp - RenderPort to check (must not be NULL)

RESULT
    TRUE if batching is enabled, FALSE otherwise.

SEE ALSO
    ZuneBeginBatch(), ZuneEndBatch()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderPort(rp))
    return FALSE;

  return rp->batching_enabled;

  AROS_LIBFUNC_EXIT
}
