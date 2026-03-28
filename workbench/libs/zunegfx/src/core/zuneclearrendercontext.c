/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneClearRenderContext
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneClearRenderContext,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0), AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 8, zunegfx)

/*  FUNCTION
    Clears the entire RenderContext with the specified color.

INPUTS
    rctx - RenderContext to clear (must not be NULL)
    color - Clear color in ARGB format (0xAARRGGBB)

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneClearRenderContext");

  D(bug("ZuneGfx: ZuneClearRenderContext(rctx=%p, color=0x%08x)\n", rctx, color));

  if (!ValidateRenderContext(rctx)) {
    D(bug("ZuneGfx: Invalid RenderContext\n"));
    return;
  }

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, ClearRenderContext, &internal_color);

  EXIT_FUNCTION("ZuneClearRenderContext");

  AROS_LIBFUNC_EXIT
}
