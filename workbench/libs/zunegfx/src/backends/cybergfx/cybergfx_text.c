/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Text Rendering

    Implements text drawing via the standard AROS graphics.library Text()
    function. This supports both bitmap fonts and TrueType/antialiased fonts
    transparently — Text() automatically detects CT_ANTIALIAS ColorTextFont
    and uses alpha-blended rendering when bitmap depth >= 15.
*/

#include <exec/types.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include "libraries/zunegfx.h"
#include "../backend_interface.h"
#include "../../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

extern struct GfxBase *GfxBase;

/*
 * ObtainPenForColor - Get a pen number for an ARGB color
 *
 * For TrueColor screens, ObtainBestPenA gives us a close match.
 * The caller must release the pen via ReleasePen when done.
 */
static LONG ObtainPenForColor(struct ColorMap *cm, struct InternalColor *color)
{
    if (!cm) {
        return 1; /* Default foreground pen */
    }

    ULONG r32 = ((ULONG)color->r << 24) | ((ULONG)color->r << 16) |
                ((ULONG)color->r << 8) | (ULONG)color->r;
    ULONG g32 = ((ULONG)color->g << 24) | ((ULONG)color->g << 16) |
                ((ULONG)color->g << 8) | (ULONG)color->g;
    ULONG b32 = ((ULONG)color->b << 24) | ((ULONG)color->b << 16) |
                ((ULONG)color->b << 8) | (ULONG)color->b;

    return ObtainBestPenA(cm, r32, g32, b32, NULL);
}

/*
 * CybergfxDrawText - Draw text using standard AROS Text() function
 *
 * Parameters:
 *   rctx     - RenderContext (font must be set)
 *   x, y     - Position (y is already at baseline)
 *   string   - Text to draw
 *   count    - Character count
 *   fg_color - Foreground color
 *   bg_color - Background color (NULL for transparent background)
 */
void CybergfxDrawText(struct RenderContext *rctx, WORD x, WORD y,
                      CONST_STRPTR string, UWORD count,
                      struct InternalColor *fg_color,
                      struct InternalColor *bg_color)
{
    struct RastPort *rp;
    struct ColorMap *cm;
    LONG fg_pen, bg_pen = -1;

    if (!rctx || !string || count == 0 || !fg_color || !rctx->font) {
        return;
    }

    /* Get target RastPort */
    rp = rctx->target_board ? rctx->target_board->rastport
                            : rctx->target_rastport;
    if (!rp) {
        return;
    }

    cm = rctx->colormap;

    /* Set font */
    SetFont(rp, rctx->font);

    /* Obtain pen for foreground color */
    fg_pen = ObtainPenForColor(cm, fg_color);
    if (fg_pen < 0) {
        fg_pen = 1;
    }
    SetAPen(rp, fg_pen);

    /* Set drawing mode and optional background */
    if (bg_color) {
        bg_pen = ObtainPenForColor(cm, bg_color);
        if (bg_pen < 0) {
            bg_pen = 0;
        }
        SetBPen(rp, bg_pen);
        SetDrMd(rp, JAM2);
    } else {
        SetDrMd(rp, JAM1);
    }

    /* Position and draw */
    Move(rp, x, y);
    Text(rp, string, count);

    /* Release pens */
    if (cm) {
        ReleasePen(cm, fg_pen);
        if (bg_pen >= 0) {
            ReleasePen(cm, bg_pen);
        }
    }

    D(bug("CybergfxDrawText: Drew %d chars at (%d,%d) font='%s'\n",
          count, x, y, rctx->font->tf_Message.mn_Node.ln_Name));
}
