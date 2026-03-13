/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorRegisterWindow
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH5(struct CompositorWindow *, ZuneCompositorRegisterWindow,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),
    AROS_LHA(APTR, glContext, A2),
    AROS_LHA(struct DrawingBoard *, board, A3),
    AROS_LHA(UBYTE, alpha, D0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 111, zunegfx)

/*  FUNCTION
    Register a window for compositing.

INPUTS
    comp - The compositor
    window - The window to register
    glContext - GL context for the window
    board - DrawingBoard for the window
    alpha - Alpha value (0-255)

RESULT
    Pointer to CompositorWindow, or NULL on failure

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return CompositorRegisterWindowInternal(comp, window, glContext, board, alpha);

    AROS_LIBFUNC_EXIT
}
