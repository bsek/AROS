/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneDrawText
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/text.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawText,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZunePoint *, position, A1),
         AROS_LHA(CONST_STRPTR, string, A2),
         AROS_LHA(UWORD, count, D0),
         AROS_LHA(ULONG, color, D1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 113, zunegfx)

/*  FUNCTION
    Draw text at a position with foreground color, transparent background.
    Position.y is the TOP of the text line (baseline is added internally).

INPUTS
    rctx     - RenderContext with a font set via ZuneSetFont()
    position - Top-left position of the text
    string   - Text string to draw
    count    - Number of characters to draw
    color    - Foreground color in ARGB format

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !position || !string || count == 0 || !rctx->font) {
    D(bug("ZuneDrawText: Invalid parameters\n"));
    return;
  }

  WORD x = position->x;
  WORD y = position->y + rctx->font->tf_Baseline;

  struct InternalColor fg_color = ZuneColorToInternal(rctx, color, rctx->pixel_format);

  ZUNE_BACKEND_CALL(rctx, DrawText, x, y, string, count, &fg_color, NULL);

  AROS_LIBFUNC_EXIT
}
