/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateLayerCompositorShared
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/screens.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(struct LayerCompositor *, ZuneCreateLayerCompositorShared,

/*  SYNOPSIS */
    AROS_LHA(struct Screen *, screen, A0),
    AROS_LHA(APTR, masterGLContext, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 107, zunegfx)

/*  FUNCTION
    Create a layer compositor with a shared GL context.

INPUTS
    screen - The screen to create a compositor for
    masterGLContext - Master GL context to share resources with

RESULT
    Pointer to LayerCompositor, or NULL on failure

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return CreateLayerCompositorSharedInternal(screen, masterGLContext);

    AROS_LIBFUNC_EXIT
}
