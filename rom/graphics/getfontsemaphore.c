/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Return the semaphore arbitrating GfxBase->TextFonts.
          Private function for diskfont.library support.
*/

#include "graphics_intern.h"

AROS_LH0(struct SignalSemaphore *, GetFontSemaphore,
         struct GfxBase *, GfxBase, 202, Graphics)
{
    AROS_LIBFUNC_INIT

    /*
     * The documented "Forbid() to walk gb_TextFonts" contract no longer
     * excludes other cores under SMP. In-tree walkers (diskfont.library's
     * AvailFonts iterator and its low-memory handler) arbitrate with the
     * same semaphore AddFont/OpenFont/CloseFont/RemFont take, obtained
     * through this private call.
     */
    return &PrivGBase(GfxBase)->fontsem;

    AROS_LIBFUNC_EXIT
}
