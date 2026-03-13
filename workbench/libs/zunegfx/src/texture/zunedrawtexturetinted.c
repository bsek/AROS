/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawTextureTinted
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH4(void, ZuneDrawTextureTinted,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZunePoint *, position, A2),
         AROS_LHA(ULONG, tint_color, D0),
         struct Library *, ZuneGfxBase, 86, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureTinted");

    D(bug("ZuneRenderer: ZuneDrawTextureTinted(rctx=%p, texture=%p, position=%p, "
          "tint=0x%08x)\n",
          rctx, texture, position, tint_color));

    if (!ValidateRenderContext(rctx) || !texture || !position) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    struct InternalColor color = ZuneColorToInternal(rctx, tint_color, rctx->pixel_format);
    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, position->x, position->y, texture->width, texture->height, 0, 0, texture->width, texture->height,
                      &color);

    EXIT_FUNCTION("ZuneDrawTextureTinted");

    AROS_LIBFUNC_EXIT
}
