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
         AROS_LHA(const struct ZuneTextStyle *, style, A3),

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

  struct InternalColor fg, bg;
  struct InternalColor *bg_ptr = NULL;
  WORD x, y;

  if (!rctx || !position || !string || count == 0 || !rctx->font || !style) {
    D(bug("ZuneDrawText: Invalid parameters\n"));
    return;
  }

  x = position->x;
  y = position->y + rctx->font->tf_Baseline;

  fg = ZuneColorToInternal(rctx, style->color, rctx->pixel_format);

  /* A zero background means none, so the glyphs go down in JAM1 */
  if (style->background) {
    bg = ZuneColorToInternal(rctx, style->background, rctx->pixel_format);
    bg_ptr = &bg;
  }

  ZUNE_BACKEND_CALL(rctx, DrawText, x, y, string, count, &fg, bg_ptr);

  AROS_LIBFUNC_EXIT
}
