/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorSetWindowAlpha
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneCompositorSetWindowAlpha,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),
    AROS_LHA(UBYTE, alpha, D0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 113, zunegfx)

/*  FUNCTION
    Set the alpha value for a window.

INPUTS
    comp - The compositor
    window - The window
    alpha - New alpha value (0-255)

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorSetWindowAlphaInternal(comp, window, alpha);

    AROS_LIBFUNC_EXIT
}
