/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Unified shape and texture drawing

    The backend already takes fill, border, corner radius and antialiasing in
    one call per geometry, so these three entry points are all the public
    surface shape drawing needs.
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "libraries/zunegfx.h"

#define DEBUG 0
#include <aros/debug.h>

#include "../backends/backend_interface.h"
#include "../backends/cybergfx/cybergfx_backend.h"
#include "../zunegfx_intern.h"

AROS_LH3(void, ZuneDrawRect,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(const struct ZuneRect *, rect, A1),
         AROS_LHA(const struct ZuneShapeStyle *, style, A2),
         struct Library *, ZuneGfxBase, 130, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct InternalColor border;
    struct InternalColor *border_ptr = NULL;
    struct ZuneBrush *fill;
    BOOL aa;

    if (!ValidateRenderContext(rctx) || !rect || !style)
        return;
    if (rect->width == 0 || rect->height == 0)
        return;

    fill = (struct ZuneBrush *)style->fill;
    aa = (style->flags & ZUNE_SHAPE_ANTIALIAS) != 0;

    if (style->border_width > 0) {
        border = ZuneColorToInternal(rctx, style->border_color,
                                     rctx->pixel_format);
        border_ptr = &border;
    }

    if (!fill && !border_ptr)
        return;

    ZUNE_BACKEND_CALL(rctx, DrawRectangle, rect->x, rect->y, rect->width,
                      rect->height, (UBYTE)style->border_width,
                      style->corner_radius, fill, border_ptr,
                      fill != NULL, aa);

    AROS_LIBFUNC_EXIT
}

AROS_LH4(void, ZuneDrawCircle,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(const struct ZunePoint *, center, A1),
         AROS_LHA(UWORD, radius, D0),
         AROS_LHA(const struct ZuneShapeStyle *, style, A2),
         struct Library *, ZuneGfxBase, 131, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct InternalColor border;
    struct InternalColor *border_ptr = NULL;
    struct ZuneBrush *fill;
    BOOL aa;

    if (!ValidateRenderContext(rctx) || !center || !style || radius == 0)
        return;

    fill = (struct ZuneBrush *)style->fill;
    aa = (style->flags & ZUNE_SHAPE_ANTIALIAS) != 0;

    if (style->border_width > 0) {
        border = ZuneColorToInternal(rctx, style->border_color,
                                     rctx->pixel_format);
        border_ptr = &border;
    }

    if (!fill && !border_ptr)
        return;

    ZUNE_BACKEND_CALL(rctx, DrawCircle, center->x, center->y, radius,
                      (UBYTE)style->border_width, fill, border_ptr,
                      fill != NULL, aa);

    AROS_LIBFUNC_EXIT
}

AROS_LH4(void, ZuneDrawTextureStyled,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(const struct ZuneRect *, dest, A2),
         AROS_LHA(const struct ZuneTextureStyle *, style, A3),
         struct Library *, ZuneGfxBase, 132, zunegfx)
{
    AROS_LIBFUNC_INIT

    static const struct ZuneTextureStyle plain = { NULL, 0, 0 };
    struct InternalColor tint;
    struct InternalColor *tint_ptr = NULL;
    WORD src_x = 0, src_y = 0;
    UWORD src_w, src_h, dst_w, dst_h;

    if (!ValidateRenderContext(rctx) || !texture || !dest)
        return;
    if (!ValidateTexture(texture))
        return;

    if (!style)
        style = &plain;

    src_w = texture->width;
    src_h = texture->height;
    if (src_w == 0 || src_h == 0)
        return;

    if (style->source) {
        src_x = style->source->x;
        src_y = style->source->y;
        if (style->source->width)
            src_w = style->source->width;
        if (style->source->height)
            src_h = style->source->height;
    }

    /* A dest without an extent takes the source's */
    dst_w = dest->width ? dest->width : src_w;
    dst_h = dest->height ? dest->height : src_h;

    if (style->tint) {
        tint = ZuneColorToInternal(rctx, style->tint, rctx->pixel_format);
        tint_ptr = &tint;
    }

    if (style->flags & ZUNE_TEXTURE_TILE) {
        /* Tiling has its own fast path in the cybergfx backend */
        if (CybergfxDrawTextureTiledFast(rctx, texture, dest->x, dest->y,
                                        dst_w, dst_h))
            return;
    }

    ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, dest->x, dest->y, dst_w,
                      dst_h, src_x, src_y, src_w, src_h, tint_ptr);

    AROS_LIBFUNC_EXIT
}
