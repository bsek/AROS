/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDestroyTexture
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

AROS_LH2(void, ZuneDestroyTexture,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         struct Library *, ZuneGfxBase, 75, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);

    ENTER_FUNCTION("ZuneDestroyTexture");

    D(bug("ZuneGfx: ZuneDestroyTexture(rctx=%p, texture=%p)\n", rctx, texture));

    if (!texture) {
        D(bug("ZuneGfx: NULL texture, nothing to destroy\n"));
        return;
    }

    /* Decrement reference count */
    texture->ref_count--;
    if (texture->ref_count > 0) {
        D(bug("ZuneGfx: Texture still has %d references\n", texture->ref_count));
        return;
    }

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->CleanupTexture) {
        backend->ops->CleanupTexture(texture);
    }

    /* Mark as invalid */
    texture->valid = FALSE;

    /* Unlock pixels if locked */
    if (texture->pixels_locked) {
        UnlockTexturePixelsInternal(texture);
    }

    /* Remove from tracking list */
    RemoveTextureFromList(base, texture);

    /* Free texture data */
    FreeTextureData(texture);

    /* Free the structure */
    FreeVec(texture);

    D(bug("ZuneGfx: Texture destroyed\n"));

    EXIT_FUNCTION("ZuneDestroyTexture");

    AROS_LIBFUNC_EXIT
}
