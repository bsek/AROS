/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneIsTextureValid
*/

#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "texture_intern.h"

AROS_LH1(BOOL, ZuneIsTextureValid,
         AROS_LHA(struct ZuneTexture *, texture, A0),
         struct Library *, ZuneGfxBase, 91, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneIsTextureValid");

    BOOL result = ValidateTexture(texture);

    EXIT_FUNCTION("ZuneIsTextureValid");
    return result;

    AROS_LIBFUNC_EXIT
}
