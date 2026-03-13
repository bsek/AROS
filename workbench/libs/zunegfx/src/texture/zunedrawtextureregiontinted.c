/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawTextureRegionTinted
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH5(void, ZuneDrawTextureRegionTinted,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3),
         AROS_LHA(ULONG, tint_color, D0),
         struct Library *, ZuneGfxBase, 88, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureRegionTinted");

    D(bug("ZuneRenderer: ZuneDrawTextureRegionTinted(rctx=%p, texture=%p, "
          "src_rect=%p, dest_rect=%p, tint=0x%08x)\n",
          rctx, texture, src_rect, dest_rect, tint_color));

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

    struct InternalColor color = ZuneColorToInternal(rctx, tint_color, rctx->pixel_format);
    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, src_rect->x, src_rect->y,
                      src_rect->width, src_rect->height, &color);

    EXIT_FUNCTION("ZuneDrawTextureRegionTinted");

    AROS_LIBFUNC_EXIT
}
