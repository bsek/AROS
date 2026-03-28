/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneTextFit
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
AROS_LH4(UWORD, ZuneTextFit,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(CONST_STRPTR, string, A1),
         AROS_LHA(UWORD, count, D0),
         AROS_LHA(UWORD, maxWidth, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 112, zunegfx)

/*  FUNCTION
    Determine how many characters from a string fit within a given pixel width.

INPUTS
    rctx     - RenderContext with a font set via ZuneSetFont()
    string   - Text string to measure
    count    - Maximum number of characters to consider
    maxWidth - Maximum pixel width available

RESULT
    Number of characters that fit within maxWidth pixels.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !string || count == 0 || maxWidth == 0) {
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

  struct TextExtent te;
  UWORD chars_fit = TextFit(rp, string, count, &te, NULL, 1, maxWidth, rctx->font->tf_YSize);

  return chars_fit;

  AROS_LIBFUNC_EXIT
}
