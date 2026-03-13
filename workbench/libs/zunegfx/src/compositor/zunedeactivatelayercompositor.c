/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDeactivateLayerCompositor
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneDeactivateLayerCompositor,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 110, zunegfx)

/*  FUNCTION
    Deactivate the layer compositor.

INPUTS
    comp - The compositor to deactivate

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    DeactivateLayerCompositorInternal(comp);

    AROS_LIBFUNC_EXIT
}
