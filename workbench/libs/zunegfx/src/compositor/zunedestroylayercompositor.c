/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDestroyLayerCompositor
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneDestroyLayerCompositor,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 108, zunegfx)

/*  FUNCTION
    Destroy a layer compositor.

INPUTS
    comp - The compositor to destroy

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    DestroyLayerCompositorInternal(comp);

    AROS_LIBFUNC_EXIT
}
