/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneClearClipRegion
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneClearClipRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 29, zunegfx)

/*  FUNCTION
    Clears the clipping region for a RenderContext, disabling clipping.

INPUTS
    rctx - RenderContext to clear clipping for

RESULT
    None

NOTES
    This function automatically frees the existing region if present.
    After calling this function, all drawing operations will be unclipped.

SEE ALSO
    ZuneSetClipRegion(), ZuneCombineRegions()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneClearClipRegion");

  if (!ValidateRenderContext(rctx)) {
    EXIT_FUNCTION("ZuneClearClipRegion");
    return;
  }

  /* Deactivate clipping in the backend first */
  {
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
    if (backend && backend->ops && backend->ops->ClearClipping) {
      backend->ops->ClearClipping(rctx);
    }
  }

  rctx->clipping_enabled = FALSE;
  if (rctx->clip_region) {
    DisposeRegion(rctx->clip_region);
    rctx->clip_region = NULL;
  }

  D(bug("ZuneGfx: Clipping disabled\n"));

  EXIT_FUNCTION("ZuneClearClipRegion");

  AROS_LIBFUNC_EXIT
}
