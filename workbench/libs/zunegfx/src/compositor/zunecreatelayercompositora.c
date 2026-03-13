/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateLayerCompositorA
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>

#include <clib/arossupport_protos.h>
#define DEBUG 1
#include <aros/debug.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(struct LayerCompositor *, ZuneCreateLayerCompositorA,

/*  SYNOPSIS */
    AROS_LHA(struct TagItem *, tags, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 123, zunegfx)

/*  FUNCTION
    Create a layer compositor from a TagItem list.

    This is the tag-based equivalent of ZuneCreateLayerCompositor() and
    ZuneCreateLayerCompositorShared(). If ZUNE_Compositor_MasterGLContext
    is provided, creates a shared compositor.

INPUTS
    tags - TagItem list with the following tags:
        ZUNE_Compositor_Screen         (struct Screen *) - Required
        ZUNE_Compositor_MasterGLContext (APTR)            - Optional

RESULT
    Pointer to LayerCompositor, or NULL on failure.

SEE ALSO
    ZuneCreateLayerCompositor(), ZuneCreateLayerCompositorShared(),
    ZuneDestroyLayerCompositor()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Screen *screen;
    APTR masterGLContext;

    if (!tags) {
        D(bug("ZuneRenderer: ZuneCreateLayerCompositorA - NULL tags\n"));
        return NULL;
    }

    screen         = (struct Screen *)LibGetTagData(ZUNE_Compositor_Screen, 0, tags);
    masterGLContext = (APTR)LibGetTagData(ZUNE_Compositor_MasterGLContext, 0, tags);

    D(bug("ZuneRenderer: ZuneCreateLayerCompositorA(screen=%p, masterGL=%p)\n",
          screen, masterGLContext));

    if (!screen) {
        D(bug("ZuneRenderer: ZuneCreateLayerCompositorA - NULL screen\n"));
        return NULL;
    }

    if (masterGLContext) {
        return CreateLayerCompositorSharedInternal(screen, masterGLContext);
    } else {
        return CreateLayerCompositorInternal(screen);
    }

    AROS_LIBFUNC_EXIT
}
