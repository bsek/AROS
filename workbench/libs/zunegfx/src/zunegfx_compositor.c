/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Layer Compositor Library Wrappers

    This file provides AROS library function wrappers for the compositor
    implementation in compositor/layer_compositor.c
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#define DEBUG 1
#include <aros/debug.h>

#include "zunegfx_intern.h"
#include "include/zunegfx.h"

/* Forward declarations of internal functions from layer_compositor.c */
struct LayerCompositor *CreateLayerCompositorInternal(struct Screen *screen);
struct LayerCompositor *CreateLayerCompositorSharedInternal(struct Screen *screen,
                                                            APTR masterGLContext);
void DestroyLayerCompositorInternal(struct LayerCompositor *comp);
BOOL ActivateLayerCompositorInternal(struct LayerCompositor *comp);
void DeactivateLayerCompositorInternal(struct LayerCompositor *comp);
struct CompositorWindow *CompositorRegisterWindowInternal(
    struct LayerCompositor *comp,
    struct Window *window,
    APTR glContext,
    struct DrawingBoard *board,
    UBYTE alpha);
void CompositorUnregisterWindowInternal(struct LayerCompositor *comp,
                                        struct Window *window);
void CompositorSetWindowAlphaInternal(struct LayerCompositor *comp,
                                      struct Window *window,
                                      UBYTE alpha);
void CompositorMarkWindowDirtyInternal(struct LayerCompositor *comp,
                                       struct Window *window);
struct CompositorWindow *CompositorFindWindowInternal(struct LayerCompositor *comp,
                                                      struct Window *window);
void CompositorUpdateInternal(struct LayerCompositor *comp);
void CompositorRefreshInternal(struct LayerCompositor *comp);
void CompositorSetShadowInternal(struct LayerCompositor *comp,
                                 WORD offsetX, WORD offsetY,
                                 UWORD blur, UBYTE alpha);

/*****************************************************************************

    NAME */
AROS_LH1(struct LayerCompositor *, CreateLayerCompositor,

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

/*****************************************************************************

    NAME */
AROS_LH2(struct LayerCompositor *, CreateLayerCompositorShared,

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

/*****************************************************************************

    NAME */
AROS_LH1(void, DestroyLayerCompositor,

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

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ActivateLayerCompositor,

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

/*****************************************************************************

    NAME */
AROS_LH1(void, DeactivateLayerCompositor,

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

/*****************************************************************************

    NAME */
AROS_LH5(struct CompositorWindow *, CompositorRegisterWindow,

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

/*****************************************************************************

    NAME */
AROS_LH2(void, CompositorUnregisterWindow,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 112, zunegfx)

/*  FUNCTION
    Unregister a window from the compositor.

INPUTS
    comp - The compositor
    window - The window to unregister

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorUnregisterWindowInternal(comp, window);

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, CompositorSetWindowAlpha,

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

/*****************************************************************************

    NAME */
AROS_LH2(void, CompositorMarkWindowDirty,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 114, zunegfx)

/*  FUNCTION
    Mark a window as dirty (needs re-read).

INPUTS
    comp - The compositor
    window - The window to mark dirty

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    CompositorMarkWindowDirtyInternal(comp, window);

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(struct CompositorWindow *, CompositorFindWindow,

/*  SYNOPSIS */
    AROS_LHA(struct LayerCompositor *, comp, A0),
    AROS_LHA(struct Window *, window, A1),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 115, zunegfx)

/*  FUNCTION
    Find a registered window.

INPUTS
    comp - The compositor
    window - The window to find

RESULT
    Pointer to CompositorWindow, or NULL if not found

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    return CompositorFindWindowInternal(comp, window);

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, CompositorUpdate,

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

/*****************************************************************************

    NAME */
AROS_LH1(void, CompositorRefresh,

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

/*****************************************************************************

    NAME */
AROS_LH5(void, CompositorSetShadow,

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
