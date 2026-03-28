/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneSetTexturePixel
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH4(void, ZuneSetTexturePixel,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZunePoint *, point, A2),
         AROS_LHA(ULONG, color, D0),
         struct Library *, ZuneGfxBase, 81, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneSetTexturePixel");

    if (!texture || !point) {
        return;
    }

    if (point->x < 0 || point->y < 0 || point->x >= texture->width || point->y >= texture->height) {
        return;
    }

    if (texture->pixels_locked) {
        return;
    }

    ZuneBackend *backend = GetTextureBackend(rctx, texture);
    if (backend && backend->ops && backend->ops->SetTexturePixel) {
        backend->ops->SetTexturePixel(texture, point->x, point->y,
                                      &((struct InternalColor){
                                          .a = (color >> 24) & 0xFF,
                                          .r = (color >> 16) & 0xFF,
                                          .g = (color >> 8) & 0xFF,
                                          .b = color & 0xFF,
                                      }));
        EXIT_FUNCTION("ZuneSetTexturePixel");
        return;
    }

    if (!texture->pixel_data) {
        return;
    }

    UBYTE *pixel_ptr;
    ULONG bytes_per_pixel;

    bytes_per_pixel = GetTextureFormatBPP(texture->format) / 8;
    pixel_ptr = (UBYTE *)texture->pixel_data + ((IPTR)point->y * texture->pitch) + ((IPTR)point->x * bytes_per_pixel);

    switch (texture->format) {
    case ZUNE_TEXTURE_FORMAT_ARGB32:
        *(ULONG *)pixel_ptr = color;
        break;
    case ZUNE_TEXTURE_FORMAT_RGB24:
        pixel_ptr[0] = (color >> 16) & 0xFF;
        pixel_ptr[1] = (color >> 8) & 0xFF;
        pixel_ptr[2] = color & 0xFF;
        break;
    case ZUNE_TEXTURE_FORMAT_ARGB16: {
        UBYTE a = (color >> 28) & 0x0F;
        UBYTE r = (color >> 20) & 0x0F;
        UBYTE g = (color >> 12) & 0x0F;
        UBYTE b = (color >> 4) & 0x0F;
        *(UWORD *)pixel_ptr = (a << 12) | (r << 8) | (g << 4) | b;
    } break;
    case ZUNE_TEXTURE_FORMAT_RGB16: {
        UBYTE r = (color >> 19) & 0x1F;
        UBYTE g = (color >> 10) & 0x3F;
        UBYTE b = (color >> 3) & 0x1F;
        *(UWORD *)pixel_ptr = (r << 11) | (g << 5) | b;
    } break;
    }

    EXIT_FUNCTION("ZuneSetTexturePixel");

    AROS_LIBFUNC_EXIT
}
