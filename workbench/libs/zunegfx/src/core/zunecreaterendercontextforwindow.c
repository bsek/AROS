/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneCreateRenderContextForWindow
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/view.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "core_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH3(struct RenderContext *, ZuneCreateRenderContextForWindow,

         /*  SYNOPSIS */
         AROS_LHA(struct Window *, window, A0),
         AROS_LHA(struct ColorMap *, colormap, A1),
         AROS_LHA(UWORD, backend_type, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 5, zunegfx)

/*  FUNCTION
    Creates a new RenderContext bound to a Window.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT
  ENTER_FUNCTION("ZuneCreateRenderContextForWindow");

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
  return CreateRenderContextForWindowInternal(base, window, colormap, backend_type);

  EXIT_FUNCTION("ZuneCreateRenderContextForWindow");
  AROS_LIBFUNC_EXIT
}
