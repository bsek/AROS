/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneUpdateTextureData
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

AROS_LH4(BOOL, ZuneUpdateTextureData,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(APTR, data, A2),
         AROS_LHA(struct ZuneRect *, rect, A3),
         struct Library *, ZuneGfxBase, 77, zunegfx)
{
    AROS_LIBFUNC_INIT

    ULONG row, bytes_per_pixel;
    UBYTE *src_ptr, *dst_ptr;

    ENTER_FUNCTION("ZuneUpdateTextureData");

    D(bug("ZuneRenderer: ZuneUpdateTextureData(rctx=%p, texture=%p, data=%p, rect=%p)\n", rctx, texture, data, rect));

    if (!texture || !data || !rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return FALSE;
    }

    if (rect->x + rect->width > texture->width || rect->y + rect->height > texture->height) {
        D(bug("ZuneRenderer: Update region out of bounds\n"));
        return FALSE;
    }

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->UpdateTexture) {
        if (backend->ops->UpdateTexture(texture, data, rect->x, rect->y, rect->width, rect->height)) {
            EXIT_FUNCTION("ZuneUpdateTextureData");
            return TRUE;
        }
    }

    if (!texture->pixel_data) {
        D(bug("ZuneRenderer: No pixel_data for CPU update path\n"));
        return FALSE;
    }

    if (texture->pixels_locked) {
        D(bug("ZuneRenderer: Texture pixels are locked\n"));
        return FALSE;
    }

    bytes_per_pixel = GetTextureFormatBPP(texture->format) / 8;
    src_ptr = (UBYTE *)data;
    dst_ptr = (UBYTE *)texture->pixel_data + ((IPTR)rect->y * texture->pitch) + ((IPTR)rect->x * bytes_per_pixel);

    for (row = 0; row < rect->height; row++) {
        CopyMem(src_ptr, dst_ptr, (IPTR)rect->width * bytes_per_pixel);
        src_ptr += (IPTR)rect->width * bytes_per_pixel;
        dst_ptr += texture->pitch;
    }

    D(bug("ZuneRenderer: Texture data updated successfully (CPU fallback)\n"));

    EXIT_FUNCTION("ZuneUpdateTextureData");
    return TRUE;

    AROS_LIBFUNC_EXIT
}
