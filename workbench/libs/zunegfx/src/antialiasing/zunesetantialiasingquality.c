/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneSetAntialiasingQuality
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "antialiasing_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/****** zunegfx.library/ZuneSetAntialiasingQuality *********************
 *
 *   NAME
 *       ZuneSetAntialiasingQuality -- Set the antialiasing quality level
 *
 *   SYNOPSIS
 *       ZuneSetAntialiasingQuality(rp, quality);
 *                                  A0  D0
 *
 *       VOID ZuneSetAntialiasingQuality(struct RenderPort *, UBYTE);
 *
 *   FUNCTION
 *       Sets the antialiasing quality level for the graphics backend.
 *       This is a global setting that affects all subsequent antialiased
 *       drawing operations.
 *
 *   INPUTS
 *       rp -- RenderPort (used to determine the backend)
 *       quality -- Quality level (0=fast, 1=good, 2=best)
 *
 ***************************************************************************/

AROS_LH2(VOID, ZuneSetAntialiasingQuality,
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(UBYTE, quality, D0),
         struct Library *, ZuneGfxBase, 59, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rp) {
    return;
  }

  (void)rp;
  g_aa_quality = quality;

  AROS_LIBFUNC_EXIT
}
