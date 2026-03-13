/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateRenderContextA
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/view.h>
#include <utility/tagitem.h>

#include <clib/arossupport_protos.h>
#define DEBUG 0
#include <aros/debug.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "core_intern.h"

/*****************************************************************************

    NAME */
AROS_LH1(struct RenderContext *, ZuneCreateRenderContextA,

         /*  SYNOPSIS */
         AROS_LHA(struct TagItem *, tags, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 120, zunegfx)

/*  FUNCTION
    Creates a new RenderContext from a TagItem list.

INPUTS
    tags - TagItem list with the following tags:
        ZUNE_RenderContext_Window   (struct Window *)  - Required
        ZUNE_RenderContext_ColorMap (struct ColorMap *) - Required
        ZUNE_RenderContext_Backend  (UWORD)             - Default: BACKEND_BEST_AVAILABLE

RESULT
    Pointer to new RenderContext, or NULL on failure.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
  struct Window *window;
  struct ColorMap *colormap;
  UWORD backend_type;

  ENTER_FUNCTION("ZuneCreateRenderContextA");

  if (!tags) {
    D(bug("ZuneRenderer: ZuneCreateRenderContextA - NULL tags\n"));
    EXIT_FUNCTION("ZuneCreateRenderContextA");
    return NULL;
  }

  window       = (struct Window *)LibGetTagData(ZUNE_RenderContext_Window, 0, tags);
  colormap     = (struct ColorMap *)LibGetTagData(ZUNE_RenderContext_ColorMap, 0, tags);
  backend_type = (UWORD)LibGetTagData(ZUNE_RenderContext_Backend, BACKEND_BEST_AVAILABLE, tags);

  D(bug("ZuneRenderer: ZuneCreateRenderContextA(window=%p, colormap=%p, backend=%d)\n",
        window, colormap, backend_type));

  return CreateRenderContextForWindowInternal(base, window, colormap, backend_type);

  EXIT_FUNCTION("ZuneCreateRenderContextA");
  AROS_LIBFUNC_EXIT
}
