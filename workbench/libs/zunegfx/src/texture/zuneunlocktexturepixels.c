/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneUnlockTexturePixels
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH2(void, ZuneUnlockTexturePixels,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         struct Library *, ZuneGfxBase, 79, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneUnlockTexturePixels");

    D(bug("ZuneGfx: ZuneUnlockTexturePixels(rctx=%p, texture=%p)\n", rctx, texture));

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->UnlockTexturePixels) {
        backend->ops->UnlockTexturePixels(texture);
        D(bug("ZuneGfx: Texture pixels unlocked (backend)\n"));
        EXIT_FUNCTION("ZuneUnlockTexturePixels");
        return;
    }

    UnlockTexturePixelsInternal(texture);

    D(bug("ZuneGfx: Texture pixels unlocked\n"));

    EXIT_FUNCTION("ZuneUnlockTexturePixels");

    AROS_LIBFUNC_EXIT
}
