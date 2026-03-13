/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateLayerCompositor
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
AROS_LH1(struct LayerCompositor *, ZuneCreateLayerCompositor,

/*  SYNOPSIS */
    AROS_LHA(struct Screen *, screen, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 106, zunegfx)

/*  FUNCTION
    Create a layer compositor for a screen.

INPUTS
    screen - The screen to create a compositor for

RESULT
    Pointer to LayerCompositor, or NULL on failure

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return CreateLayerCompositorInternal(screen);

    AROS_LIBFUNC_EXIT
}
