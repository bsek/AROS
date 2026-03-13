/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorRefresh
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneCompositorRefresh,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 117, zunegfx)

/*  FUNCTION
    Force a full refresh of all composited windows.

INPUTS
    comp - The compositor

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorRefreshInternal(comp);

    AROS_LIBFUNC_EXIT
}
