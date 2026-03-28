/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneGetTexturePixel
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH3(ULONG, ZuneGetTexturePixel,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZunePoint *, point, A2),
         struct Library *, ZuneGfxBase, 80, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneGetTexturePixel");

    if (!texture || !point) {
        return 0;
    }

    if (point->x < 0 || point->y < 0 || point->x >= texture->width || point->y >= texture->height) {
        return 0;
    }

    if (texture->pixels_locked) {
        return 0;
    }

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->GetTexturePixel) {
        return backend->ops->GetTexturePixel(texture, point->x, point->y);
    }

    if (!texture->pixel_data) {
        return 0;
    }

    UBYTE *pixel_ptr;
    ULONG color = 0;
    ULONG bytes_per_pixel;

    bytes_per_pixel = GetTextureFormatBPP(texture->format) / 8;
    pixel_ptr = (UBYTE *)texture->pixel_data + ((IPTR)point->y * texture->pitch) + ((IPTR)point->x * bytes_per_pixel);

    switch (texture->format) {
    case ZUNE_TEXTURE_FORMAT_ARGB32:
        color = *(ULONG *)pixel_ptr;
        break;
    case ZUNE_TEXTURE_FORMAT_RGB24:
        color = 0xFF000000 | (pixel_ptr[0] << 16) | (pixel_ptr[1] << 8) | pixel_ptr[2];
        break;
    case ZUNE_TEXTURE_FORMAT_ARGB16: {
        UWORD pixel = *(UWORD *)pixel_ptr;
        UBYTE a = (pixel >> 12) & 0x0F;
        UBYTE r = (pixel >> 8) & 0x0F;
        UBYTE g = (pixel >> 4) & 0x0F;
        UBYTE b = pixel & 0x0F;
        color = (a << 28) | (r << 20) | (g << 12) | (b << 4);
    } break;
    case ZUNE_TEXTURE_FORMAT_RGB16: {
        UWORD pixel = *(UWORD *)pixel_ptr;
        UBYTE r = (pixel >> 11) & 0x1F;
        UBYTE g = (pixel >> 5) & 0x3F;
        UBYTE b = pixel & 0x1F;
        color = 0xFF000000 | (r << 19) | (g << 10) | (b << 3);
    } break;
    }

    EXIT_FUNCTION("ZuneGetTexturePixel");
    return color;

    AROS_LIBFUNC_EXIT
}
