/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorUpdate
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneCompositorUpdate,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 116, zunegfx)

/*  FUNCTION
    Update all visible alpha windows.

INPUTS
    comp - The compositor

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorUpdateInternal(comp);

    AROS_LIBFUNC_EXIT
}
