/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateTextureFromDrawingBoard
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH2(struct ZuneTexture *, ZuneCreateTextureFromDrawingBoard,
         AROS_LHA(struct RenderContext *, rctx, A0), AROS_LHA(ULONG, flags, D0),
         struct Library *, ZuneGfxBase, 72, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);

    ENTER_FUNCTION("ZuneCreateTextureFromDrawingBoard");

    struct ZuneTexture *texture = CreateTextureFromDrawingBoardInternal(base, rctx, flags);

    EXIT_FUNCTION("ZuneCreateTextureFromDrawingBoard");
    return texture;

    AROS_LIBFUNC_EXIT
}
