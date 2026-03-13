/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawTextureRegion
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH4(void, ZuneDrawTextureRegion,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3),
         struct Library *, ZuneGfxBase, 85, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureRegion");

    D(bug("ZuneRenderer: ZuneDrawTextureRegion(rctx=%p, texture=%p, src_rect=%p, "
          "dest_rect=%p)\n",
          rctx, texture, src_rect, dest_rect));

    if (!ValidateRenderContext(rctx) || !texture || !src_rect || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    if (src_rect->x + src_rect->width > texture->width || src_rect->y + src_rect->height > texture->height) {
        D(bug("ZuneRenderer: Source region out of bounds\n"));
        return;
    }

    if (dest_rect->width == 0 || dest_rect->height == 0) {
        D(bug("ZuneRenderer: Invalid destination dimensions\n"));
        return;
    }

    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, src_rect->x, src_rect->y,
                      src_rect->width, src_rect->height, NULL);

    EXIT_FUNCTION("ZuneDrawTextureRegion");

    AROS_LIBFUNC_EXIT
}
