/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorFindWindow
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH2(struct CompositorWindow *, ZuneCompositorFindWindow,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 115, zunegfx)

/*  FUNCTION
    Find a registered window.

INPUTS
    comp - The compositor
    window - The window to find

RESULT
    Pointer to CompositorWindow, or NULL if not found

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return CompositorFindWindowInternal(comp, window);

    AROS_LIBFUNC_EXIT
}
