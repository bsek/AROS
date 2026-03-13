/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateTexture
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

AROS_LH6(struct ZuneTexture *, ZuneCreateTexture,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(UWORD, width, D0), AROS_LHA(UWORD, height, D1), AROS_LHA(UBYTE, depth, D2), AROS_LHA(ULONG, format, D3), AROS_LHA(ULONG, flags, D4),
         struct Library *, ZuneGfxBase, 70, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
    struct ZuneTexture *texture;

    ENTER_FUNCTION("ZuneCreateTexture");

    D(bug("ZuneRenderer: ZuneCreateTexture(rctx=%p, width=%d, height=%d, depth=%d, "
          "format=0x%08x, flags=0x%08x)\n",
          rctx, width, height, depth, format, flags));

    if (width == 0 || height == 0) {
        D(bug("ZuneRenderer: Invalid texture dimensions\n"));
        return NULL;
    }

    texture = AllocateTexture();
    if (!texture) {
        D(bug("ZuneRenderer: Failed to allocate texture structure\n"));
        return NULL;
    }

    InitializeTexture(texture, width, height, depth, format, flags);

    AllocateTextureData(texture); /* best-effort for CPU path */

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->InitTexture) {
        if (backend->ops->InitTexture(texture)) {
            texture->backend_type = backend->ops->type;
        }
    }

    AddTextureToList(base, texture);

    D(bug("ZuneRenderer: Texture created successfully (%p)\n", texture));

    EXIT_FUNCTION("ZuneCreateTexture");
    return texture;

    AROS_LIBFUNC_EXIT
}
