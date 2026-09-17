/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Accessors for the opaque objects

    RenderContext, DrawingBoard and ZuneTexture are opaque to callers, so the
    few fields that are genuinely useful from outside are reached through
    these.
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/rastport.h>

#include "libraries/zunegfx.h"

#define DEBUG 0
#include <aros/debug.h>

#include "../zunegfx_intern.h"

AROS_LH1(struct RastPort *, ZuneGetBoardRastPort,
         AROS_LHA(struct DrawingBoard *, board, A0),
         struct Library *, ZuneGfxBase, 120, zunegfx)
{
    AROS_LIBFUNC_INIT
    return board ? board->rastport : NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct BitMap *, ZuneGetBoardBitMap,
         AROS_LHA(struct DrawingBoard *, board, A0),
         struct Library *, ZuneGfxBase, 121, zunegfx)
{
    AROS_LIBFUNC_INIT
    return board ? board->bitmap : NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH3(void, ZuneGetBoardSize,
         AROS_LHA(struct DrawingBoard *, board, A0),
         AROS_LHA(UWORD *, width, A1),
         AROS_LHA(UWORD *, height, A2),
         struct Library *, ZuneGfxBase, 122, zunegfx)
{
    AROS_LIBFUNC_INIT

    if (width)
        *width = board ? board->width : 0;
    if (height)
        *height = board ? board->height : 0;

    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct RastPort *, ZuneGetTargetRastPort,
         AROS_LHA(struct RenderContext *, rctx, A0),
         struct Library *, ZuneGfxBase, 123, zunegfx)
{
    AROS_LIBFUNC_INIT
    return rctx ? rctx->target_rastport : NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct DrawingBoard *, ZuneGetTargetBoard,
         AROS_LHA(struct RenderContext *, rctx, A0),
         struct Library *, ZuneGfxBase, 124, zunegfx)
{
    AROS_LIBFUNC_INIT
    return rctx ? rctx->target_board : NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, ZuneGetBackendType,
         AROS_LHA(struct RenderContext *, rctx, A0),
         struct Library *, ZuneGfxBase, 125, zunegfx)
{
    AROS_LIBFUNC_INIT
    return rctx ? rctx->backend_type : 0;
    AROS_LIBFUNC_EXIT
}

AROS_LH3(void, ZuneGetTextureSize,
         AROS_LHA(struct ZuneTexture *, texture, A0),
         AROS_LHA(UWORD *, width, A1),
         AROS_LHA(UWORD *, height, A2),
         struct Library *, ZuneGfxBase, 126, zunegfx)
{
    AROS_LIBFUNC_INIT

    if (width)
        *width = texture ? texture->width : 0;
    if (height)
        *height = texture ? texture->height : 0;

    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, ZuneGetTextureFlags,
         AROS_LHA(struct ZuneTexture *, texture, A0),
         struct Library *, ZuneGfxBase, 127, zunegfx)
{
    AROS_LIBFUNC_INIT
    return texture ? texture->flags : 0;
    AROS_LIBFUNC_EXIT
}

/* ZuneDestroyTexture() is the matching release: it decrements and only frees
 * the texture once the last reference is gone. */
AROS_LH1(void, ZuneRetainTexture,
         AROS_LHA(struct ZuneTexture *, texture, A0),
         struct Library *, ZuneGfxBase, 128, zunegfx)
{
    AROS_LIBFUNC_INIT

    if (texture)
        texture->ref_count++;

    AROS_LIBFUNC_EXIT
}
