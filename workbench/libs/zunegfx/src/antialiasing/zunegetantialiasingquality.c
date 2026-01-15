/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneGetAntialiasingQuality
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "antialiasing_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneGetAntialiasingQuality *********************
 *
 *   NAME
 *       ZuneGetAntialiasingQuality -- Get the current antialiasing quality
 *
 *   SYNOPSIS
 *       quality = ZuneGetAntialiasingQuality(rp);
 *       D0                                   A0
 *
 *       UBYTE ZuneGetAntialiasingQuality(struct RenderPort *);
 *
 *   FUNCTION
 *       Returns the current antialiasing quality level from the graphics
 *       backend. This reflects the global quality setting that affects
 *       all antialiased drawing operations.
 *
 *   INPUTS
 *       rp -- RenderPort (used to determine the backend)
 *
 *   RESULT
 *       quality -- Current quality level (0=fast, 1=good, 2=best)
 *                  Returns 0 if backend doesn't support quality control
 *
 ***************************************************************************/

AROS_LH1(UWORD, ZuneGetAntialiasingQuality,
         AROS_LHA(struct RenderPort *, rp, A0), struct Library *,
         ZuneGfxBase, 60, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rp) {
    return 0;
  }

  (void)rp;
  return g_aa_quality;

  AROS_LIBFUNC_EXIT
}
