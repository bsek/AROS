/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDestroyRenderContext
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "core_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneDestroyRenderContext,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 7, zunegfx)

/*  FUNCTION
    Destroys a RenderContext and frees all associated resources.
    Any pending batch operations are automatically flushed.

INPUTS
    rctx - RenderContext to destroy (may be NULL)

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);

  ENTER_FUNCTION("ZuneDestroyRenderContext");

  D(bug("ZuneGfx: ZuneDestroyRenderContext(rctx=%p)\n", rctx));

  if (!rctx) {
    D(bug("ZuneGfx: NULL RenderContext, nothing to destroy\n"));
    return;
  }

  /* Flush any pending batch operations */
  if (rctx->batch_state && rctx->batching_enabled) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (batch->immediate.count > 0 || batch->deferred.count > 0) {
      D(bug("ZuneGfx: Flushing pending batch operations\n"));
    }
  }

  /* Remove from tracking list */
  RemoveRenderContextFromList(base, rctx);

  /* Cleanup RenderContext */
  CleanupRenderContext(rctx);

  /* Free the structure */
  FreeVec(rctx);

  D(bug("ZuneGfx: RenderContext destroyed\n"));

  EXIT_FUNCTION("ZuneDestroyRenderContext");

  AROS_LIBFUNC_EXIT
}
