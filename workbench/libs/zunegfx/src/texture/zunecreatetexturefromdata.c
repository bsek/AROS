/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateTextureFromData
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/exec.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH8(struct ZuneTexture *, ZuneCreateTextureFromData,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(APTR, data, A1), AROS_LHA(UWORD, width, D0), AROS_LHA(UWORD, height, D1), AROS_LHA(UBYTE, depth, D2), AROS_LHA(ULONG, format, D3),
         AROS_LHA(ULONG, pitch, D4), AROS_LHA(ULONG, flags, D5),
         struct Library *, ZuneGfxBase, 71, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
    struct ZuneTexture *texture = NULL;

    ENTER_FUNCTION("ZuneCreateTextureFromData");

    D(bug("ZuneRenderer: ZuneCreateTextureFromData(rctx=%p, data=%p, width=%d, height=%d, "
          "depth=%d, format=0x%08x, pitch=%d, flags=0x%08x)\n",
          rctx, data, width, height, depth, format, pitch, flags));

    texture = CreateTextureFromDataInternal(data, width, height, depth, format, pitch, flags);

    if (texture) {
        ZuneBackend *backend = GetTextureBackend(rctx, texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    EXIT_FUNCTION("ZuneCreateTextureFromData");
    return texture;

    AROS_LIBFUNC_EXIT
}
