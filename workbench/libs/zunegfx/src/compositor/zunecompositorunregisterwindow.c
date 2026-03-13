/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorUnregisterWindow
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneCompositorUnregisterWindow,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 112, zunegfx)

/*  FUNCTION
    Unregister a window from the compositor.

INPUTS
    comp - The compositor
    window - The window to unregister

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorUnregisterWindowInternal(comp, window);

    AROS_LIBFUNC_EXIT
}
