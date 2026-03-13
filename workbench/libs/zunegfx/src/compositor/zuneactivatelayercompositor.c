/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneActivateLayerCompositor
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ZuneActivateLayerCompositor,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 109, zunegfx)

/*  FUNCTION
    Activate the layer compositor.

INPUTS
    comp - The compositor to activate

RESULT
    TRUE if successful

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return ActivateLayerCompositorInternal(comp);

    AROS_LIBFUNC_EXIT
}
