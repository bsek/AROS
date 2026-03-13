/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawTextureScaled
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH3(void, ZuneDrawTextureScaled,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZuneRect *, dest_rect, A2),
         struct Library *, ZuneGfxBase, 84, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureScaled");

    D(bug("ZuneRenderer: ZuneDrawTextureScaled(rctx=%p, texture=%p, dest_rect=%p)\n", rctx, texture, dest_rect));

    if (!ValidateRenderContext(rctx) || !texture || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, 0, 0, texture->width,
                      texture->height, NULL);

    EXIT_FUNCTION("ZuneDrawTextureScaled");

    AROS_LIBFUNC_EXIT
}
