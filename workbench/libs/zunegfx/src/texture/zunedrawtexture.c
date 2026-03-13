/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawTexture
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH3(void, ZuneDrawTexture,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZunePoint *, position, A2),
         struct Library *, ZuneGfxBase, 83, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTexture");

    D(bug("ZuneRenderer: ZuneDrawTexture(rctx=%p, texture=%p, position=%p)\n", rctx, texture, position));

    if (!ValidateRenderContext(rctx) || !texture || !position) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    UWORD width = texture->width;
    UWORD height = texture->height;
    UWORD x = position->x;
    UWORD y = position->y;

    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, x, y, width, height, 0, 0, width, height, NULL);

    EXIT_FUNCTION("ZuneDrawTexture");

    AROS_LIBFUNC_EXIT
}
