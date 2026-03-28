/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneLockTexturePixels
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH3(APTR, ZuneLockTexturePixels,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(ULONG *, pitch, A2),
         struct Library *, ZuneGfxBase, 78, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneLockTexturePixels");

    D(bug("ZuneGfx: ZuneLockTexturePixels(rctx=%p, texture=%p, pitch=%p)\n", rctx, texture, pitch));

    if (!texture) {
        D(bug("ZuneGfx: Invalid texture\n"));
        return NULL;
    }

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->LockTexturePixels) {
        APTR ptr = backend->ops->LockTexturePixels(texture, pitch);
        if (ptr)
            return ptr;
    }

    if (texture->pixels_locked) {
        D(bug("ZuneGfx: Texture pixels already locked\n"));
        return NULL;
    }

    if (!texture->pixel_data) {
        D(bug("ZuneGfx: No pixel data available to lock\n"));
        return NULL;
    }

    texture->pixels_locked = TRUE;

    if (pitch) {
        *pitch = texture->pitch;
    }

    D(bug("ZuneGfx: Texture pixels locked (pitch=%d)\n", texture->pitch));

    EXIT_FUNCTION("ZuneLockTexturePixels");
    return texture->pixel_data;

    AROS_LIBFUNC_EXIT
}
