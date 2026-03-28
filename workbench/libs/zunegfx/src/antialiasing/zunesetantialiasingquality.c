/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneSetAntialiasingQuality
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
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
 *       ZuneSetAntialiasingQuality(rctx, quality);
 *                                  A0  D0
 *
 *       VOID ZuneSetAntialiasingQuality(struct RenderContext *, UBYTE);
 *
 *   FUNCTION
 *       Sets the antialiasing quality level for the graphics backend.
 *       This is a global setting that affects all subsequent antialiased
 *       drawing operations.
 *
 *   INPUTS
 *       rctx -- RenderContext (used to determine the backend)
 *       quality -- Quality level (0=fast, 1=good, 2=best)
 *
 ***************************************************************************/

AROS_LH2(VOID, ZuneSetAntialiasingQuality,
         AROS_LHA(struct RenderContext *, rctx, A0), AROS_LHA(UBYTE, quality, D0),
         struct Library *, ZuneGfxBase, 59, zunegfx)

{
  AROS_LIBFUNC_INIT

  if (!rctx) {
    return;
  }

  (void)rctx;
  g_aa_quality = quality;

  AROS_LIBFUNC_EXIT
}
