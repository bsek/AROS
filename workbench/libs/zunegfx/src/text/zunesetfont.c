/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneSetFont
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/text.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneSetFont,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct TextFont *, font, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 110, zunegfx)

/*  FUNCTION
    Set the current font for text operations on this RenderContext.
    Accepts any TextFont* including TrueType fonts loaded via OpenDiskFont().

INPUTS
    rctx - RenderContext
    font - TextFont to use for subsequent text operations

RESULT
    None

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !font) {
    D(bug("ZuneSetFont: Invalid parameters (rctx=%p, font=%p)\n", rctx, font));
    return;
  }

  rctx->font = font;

  /* Apply font to the target RastPort for measurement functions */
  struct RastPort *rp = rctx->target_board
                            ? rctx->target_board->rastport
                            : rctx->target_rastport;
  if (rp) {
    SetFont(rp, font);
  }

  D(bug("ZuneSetFont: Set font '%s' size=%d on rctx=%p\n",
        font->tf_Message.mn_Node.ln_Name, font->tf_YSize, rctx));

  AROS_LIBFUNC_EXIT
}
