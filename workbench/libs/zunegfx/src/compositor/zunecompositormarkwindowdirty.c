/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorMarkWindowDirty
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneCompositorMarkWindowDirty,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 114, zunegfx)

/*  FUNCTION
    Mark a window as dirty (needs re-read).

INPUTS
    comp - The compositor
    window - The window to mark dirty

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorMarkWindowDirtyInternal(comp, window);

    AROS_LIBFUNC_EXIT
}
