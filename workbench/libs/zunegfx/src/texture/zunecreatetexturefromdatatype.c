/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateTextureFromDatatype
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>
#include <proto/exec.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH3(struct ZuneTexture *, ZuneCreateTextureFromDatatype,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(APTR, dt_object, A1), AROS_LHA(ULONG, flags, D0),
         struct Library *, ZuneGfxBase, 73, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
    struct ZuneTexture *texture = CreateTextureFromDatatypeInternal(dt_object, flags);

    if (texture) {
        ZuneBackend *backend = GetTextureBackend(rctx, texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    return texture;

    AROS_LIBFUNC_EXIT
}
