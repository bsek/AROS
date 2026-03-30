/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneSetClipRegion
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
AROS_LH2(void, ZuneSetClipRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct Region *, region, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 28, zunegfx)

/*  FUNCTION
    Sets a clipping region for a RenderContext.

INPUTS
    rctx - RenderContext to set clipping for
    region - Clipping region to be installed

RESULT
    None

NOTES
    All subsequent drawing operations will be clipped to this region.
    The region defines the area where drawing is allowed.

SEE ALSO
    ZuneClearClipRegion(), ZuneCombineRegions()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneSetClipRegion");

  if (!ValidateRenderContext(rctx)) {
    D(bug("ZuneGfx: Invalid RenderContext\n"));
    EXIT_FUNCTION("ZuneSetClipRegion");
    return;
  }

  if (!region) {
    D(bug("ZuneGfx: Invalid region\n"));
    EXIT_FUNCTION("ZuneSetClipRegion");
    return;
  }

  /* Clear existing region if any */
  if (rctx->clip_region) {
    DisposeRegion(rctx->clip_region);
    rctx->clip_region = NULL;
  }

  /* Create new region and copy input */
  rctx->clip_region = NewRegion();
  if (!rctx->clip_region) {
    D(bug("ZuneGfx: Failed to allocate new region\n"));
    rctx->clipping_enabled = FALSE;
    EXIT_FUNCTION("ZuneSetClipRegion");
    return;
  }

  if (OrRegionRegion(region, rctx->clip_region)) {
    rctx->clipping_enabled = TRUE;

    /* Activate clipping in the backend */
    {
      ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
      if (backend && backend->ops && backend->ops->SetupClipping) {
        backend->ops->SetupClipping(rctx, rctx->clip_region);
      }
    }

    D(bug("ZuneGfx: Clipping region installed successfully\n"));
  } else {
    D(bug("ZuneGfx: Failed to copy region\n"));
    DisposeRegion(rctx->clip_region);
    rctx->clip_region = NULL;
    rctx->clipping_enabled = FALSE;
  }

  EXIT_FUNCTION("ZuneSetClipRegion");

  AROS_LIBFUNC_EXIT
}
