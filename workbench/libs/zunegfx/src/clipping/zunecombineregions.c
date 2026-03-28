/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneCombineRegions
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
AROS_LH2(BOOL, ZuneCombineRegions,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct Region *, region, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 30, zunegfx)

/*  FUNCTION
    Combines a region with the RenderContext's existing clipping region using OR operation.

INPUTS
    rctx - RenderContext to modify clipping for
    region - Region to combine with existing clipping region

RESULT
    TRUE if successful, FALSE if operation failed or invalid parameters.

NOTES
    If the RenderContext has no existing clipping region, the provided region
    becomes the new clipping region. The operation uses OR logic to combine
    the regions, expanding the clippable area.

SEE ALSO
    ZuneSetClipRegion(), ZuneClearClipRegion()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!ValidateRenderContext(rctx))
    return FALSE;

  if (!region)
    return FALSE;

  if (!rctx->clip_region) {
    /* No existing region, create a new one by copying the input */
    rctx->clip_region = NewRegion();
    if (!rctx->clip_region)
      return FALSE;
    if (!OrRegionRegion(region, rctx->clip_region)) {
      DisposeRegion(rctx->clip_region);
      rctx->clip_region = NULL;
      return FALSE;
    }
    rctx->clipping_enabled = TRUE;
  } else {
    /* Combine with existing region */
    return !OrRegionRegion(region, rctx->clip_region);
  }

  D(bug("ZuneGfx: Combined region with RenderContext clipping\n"));

  return TRUE;

  AROS_LIBFUNC_EXIT
}
