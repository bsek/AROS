/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneTextLength
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH3(UWORD, ZuneTextLength,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(CONST_STRPTR, string, A1),
         AROS_LHA(UWORD, count, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 111, zunegfx)

/*  FUNCTION
    Measure the pixel width of a string using the current font.

INPUTS
    rctx   - RenderContext with a font set via ZuneSetFont()
    string - Text string to measure
    count  - Number of characters to measure

RESULT
    Pixel width of the string, or 0 on error.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !string || count == 0) {
    return 0;
  }

  struct RastPort *rp = rctx->target_board
                            ? rctx->target_board->rastport
                            : rctx->target_rastport;
  if (!rp) {
    return 0;
  }

  /* Ensure font is set on RastPort */
  if (rctx->font && rp->Font != rctx->font) {
    SetFont(rp, rctx->font);
  }

  WORD width = TextLength(rp, string, count);

  return (UWORD)(width > 0 ? width : 0);

  AROS_LIBFUNC_EXIT
}
