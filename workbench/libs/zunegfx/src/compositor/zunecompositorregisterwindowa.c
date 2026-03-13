/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCompositorRegisterWindowA
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>

#include <clib/arossupport_protos.h>
#define DEBUG 1
#include <aros/debug.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "compositor_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(struct CompositorWindow *, ZuneCompositorRegisterWindowA,

/*  SYNOPSIS */
    AROS_LHA(struct TagItem *, tags, A0),

/*  LOCATION */
    struct Library *, ZuneGfxBase, 124, zunegfx)

/*  FUNCTION
    Register a window for compositing from a TagItem list.

    This is the tag-based equivalent of ZuneCompositorRegisterWindow().

INPUTS
    tags - TagItem list with the following tags:
        ZUNE_CompositorWin_Compositor  (struct LayerCompositor *) - Required
        ZUNE_CompositorWin_Window      (struct Window *)          - Required
        ZUNE_CompositorWin_GLContext   (APTR)                     - Default: NULL
        ZUNE_CompositorWin_DrawingBoard (struct DrawingBoard *)   - Default: NULL
        ZUNE_CompositorWin_Alpha       (UBYTE)                    - Default: 255

RESULT
    Pointer to CompositorWindow, or NULL on failure.

SEE ALSO
    ZuneCompositorRegisterWindow(), ZuneCompositorUnregisterWindow()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct LayerCompositor *comp;
    struct Window *window;
    APTR glContext;
    struct DrawingBoard *board;
    UBYTE alpha;

    if (!tags) {
        D(bug("ZuneRenderer: ZuneCompositorRegisterWindowA - NULL tags\n"));
        return NULL;
    }

    comp      = (struct LayerCompositor *)LibGetTagData(ZUNE_CompositorWin_Compositor, 0, tags);
    window    = (struct Window *)LibGetTagData(ZUNE_CompositorWin_Window, 0, tags);
    glContext = (APTR)LibGetTagData(ZUNE_CompositorWin_GLContext, 0, tags);
    board     = (struct DrawingBoard *)LibGetTagData(ZUNE_CompositorWin_DrawingBoard, 0, tags);
    alpha     = (UBYTE)LibGetTagData(ZUNE_CompositorWin_Alpha, 255, tags);

    D(bug("ZuneRenderer: ZuneCompositorRegisterWindowA(comp=%p, win=%p, gl=%p, board=%p, alpha=%d)\n",
          comp, window, glContext, board, alpha));

    if (!comp || !window) {
        D(bug("ZuneRenderer: ZuneCompositorRegisterWindowA - missing required params\n"));
        return NULL;
    }

    return CompositorRegisterWindowInternal(comp, window, glContext, board, alpha);

    AROS_LIBFUNC_EXIT
}
