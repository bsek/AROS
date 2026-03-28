/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawTextureScaledTinted
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH4(void, ZuneDrawTextureScaledTinted,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZuneRect *, dest_rect, A2),
         AROS_LHA(ULONG, tint_color, D0),
         struct Library *, ZuneGfxBase, 87, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct ZuneRect src_rect;

    ENTER_FUNCTION("ZuneDrawTextureScaledTinted");

    D(bug("ZuneGfx: ZuneDrawTextureScaledTinted(rctx=%p, texture=%p, "
          "dest_rect=%p, tint=0x%08x)\n",
          rctx, texture, dest_rect, tint_color));

    if (!ValidateRenderContext(rctx) || !texture || !dest_rect) {
        D(bug("ZuneGfx: Invalid parameters\n"));
        return;
    }

    struct InternalColor color = ZuneColorToInternal(rctx, tint_color, rctx->pixel_format);
    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, 0, 0, texture->width,
                      texture->height, &color);

    EXIT_FUNCTION("ZuneDrawTextureScaledTinted");

    AROS_LIBFUNC_EXIT
}
