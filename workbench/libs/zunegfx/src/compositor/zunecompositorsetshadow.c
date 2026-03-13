/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorSetShadow
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneCompositorSetShadow,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(WORD, offsetX, D0),
    AROS_LHA(WORD, offsetY, D1),
    AROS_LHA(UWORD, blur, D2),
    AROS_LHA(UBYTE, alpha, D3),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 118, zunegfx)

/*  FUNCTION
    Configure shadow parameters.

INPUTS
    comp - The compositor
    offsetX - Shadow X offset
    offsetY - Shadow Y offset
    blur - Shadow blur radius
    alpha - Shadow alpha (0-255)

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorSetShadowInternal(comp, offsetX, offsetY, blur, alpha);

    AROS_LIBFUNC_EXIT
}
