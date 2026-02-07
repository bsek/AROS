/*
    Copyright (C) 2010-2025, The AROS Development Team. All rights reserved.

    Desc: Display mode management for GLCompositor.
          Identical logic to software Compositor - finds best HIDD mode
          for the current bitmap stack configuration.
*/

#include <aros/debug.h>

#include <proto/utility.h>
#include "glcompositor_intern.h"

static HIDDT_ModeID FindBestHiddMode(struct HIDDCompositorData *compdata, ULONG width, ULONG height, ULONG depth, ULONG *res_depth)
{
    HIDDT_ModeID mode = vHidd_ModeID_Invalid;
    OOP_Object *sync, *pf;
    IPTR w, h, d;
    ULONG dw, dh, delta;
    ULONG found_delta  = -1;
    ULONG found_width  = 0;
    ULONG found_height = 0;
    ULONG found_depth  = 0;
    HIDDT_ModeID found_mode = vHidd_ModeID_Invalid;

    D(bug("[GLCompositor] %s: Finding best match for mode %ux%ux%u\n", __func__, width, height, depth));

    while ((mode = HIDD_Gfx_NextModeID(compdata->gfx, mode, &sync, &pf)) != vHidd_ModeID_Invalid)
    {
        BOOL match;

        if (OOP_GET(pf, aHidd_PixFmt_ColorModel) != vHidd_ColorModel_TrueColor)
            continue;

        OOP_GetAttr(sync, aHidd_Sync_HDisp, &w);
        OOP_GetAttr(sync, aHidd_Sync_VDisp, &h);
        OOP_GetAttr(pf, aHidd_PixFmt_Depth, &d);

        dw = w > width  ? w - width  : w < width  ? width  - w : 1;
        dh = h > height ? h - height : h < height ? height - h : 1;
        delta = dw * dh;

        match = FALSE;
        if (delta < found_delta)
        {
            found_delta  = delta;
            found_width  = w;
            found_height = h;
            match = TRUE;
        }
        else if (delta == found_delta)
        {
            if (found_depth > depth)
            {
                if ((d < found_depth) && (d >= depth))
                    match = TRUE;
            }
            else if (found_depth < depth)
            {
                if (d > found_depth)
                    match = TRUE;
            }
        }

        if (match)
        {
            found_depth = d;
            found_mode  = mode;
        }
    }

    compdata->displayrect.MinX = 0;
    compdata->displayrect.MinY = 0;
    compdata->displayrect.MaxX = found_width  - 1;
    compdata->displayrect.MaxY = found_height - 1;
    *res_depth = found_depth;

    return found_mode;
}

static void CalculateParametersAlpha(struct HIDDCompositorData *compdata, ULONG *wantedwidth,
        ULONG *wantedheight, ULONG *wanteddepth)
{
    struct StackBitMapNode  *n;
    OOP_Object              *sync, *pf;
    IPTR                    curwidth, curheight, curdepth, modeid;

    for (n = (struct StackBitMapNode *)compdata->bitmapstack.mlh_TailPred;
             n->n.mln_Pred; n = (struct StackBitMapNode *)n->n.mln_Pred)
    {
        if ((!(n->sbmflags & COMPF_ALPHA)) && (n->sbmflags & STACKNODEF_DISPLAYABLE))
        {
            OOP_GetAttr(n->bm, aHidd_BitMap_ModeID, &modeid);
            HIDD_Gfx_GetMode(compdata->gfx, modeid, &sync, &pf);

            if (sync)
            {
                OOP_GetAttr(sync, aHidd_Sync_HDisp, &curwidth);
                OOP_GetAttr(sync, aHidd_Sync_VDisp, &curheight);
            }
            else
            {
                OOP_GetAttr(n->bm, aHidd_BitMap_Width, &curwidth);
                OOP_GetAttr(n->bm, aHidd_BitMap_Height, &curheight);
            }

            if (OOP_GET(pf, aHidd_PixFmt_ColorModel) == vHidd_ColorModel_TrueColor)
            {
                OOP_GetAttr(pf, aHidd_PixFmt_Depth, &curdepth);
            }
            else
            {
                if (compdata->flags & COMPSTATEF_DEEPLUT)
                    curdepth = 24;
                else
                    curdepth = 16;
            }
        }
        else
        {
            OOP_GetAttr(n->bm, aHidd_BitMap_Width, &curwidth);
            OOP_GetAttr(n->bm, aHidd_BitMap_Height, &curheight);

            if (n->sbmflags & COMPF_ALPHA)
                curdepth = 24;
            else
            {
                OOP_GetAttr(n->bm, aHidd_BitMap_PixFmt, (IPTR *)&pf);
                if (OOP_GET(pf, aHidd_PixFmt_ColorModel) == vHidd_ColorModel_TrueColor)
                {
                    OOP_GetAttr(pf, aHidd_PixFmt_Depth, &curdepth);
                }
                else
                {
                    if (compdata->flags & COMPSTATEF_DEEPLUT)
                        curdepth = 24;
                    else
                        curdepth = 16;
                }
            }
        }
        if ((ULONG)curwidth > *wantedwidth)
            *wantedwidth = (ULONG)curwidth;
        if ((ULONG)curheight > *wantedheight)
            *wantedheight = (ULONG)curheight;
        if ((ULONG)curdepth > *wanteddepth)
            *wanteddepth = (ULONG)curdepth;
    }
}

static void CalculateParametersRegular(struct HIDDCompositorData *compdata, ULONG *wantedwidth,
        ULONG *wantedheight, ULONG *wanteddepth)
{
    struct StackBitMapNode  *n;
    OOP_Object              *sync, *pf;
    IPTR                    width, height, depth, modeid;

    for (n = (struct StackBitMapNode *)compdata->bitmapstack.mlh_TailPred;
         n->n.mln_Pred; n = (struct StackBitMapNode *)n->n.mln_Pred)
    {
        OOP_GetAttr(n->bm, aHidd_BitMap_ModeID, &modeid);
        HIDD_Gfx_GetMode(compdata->gfx, modeid, &sync, &pf);

        if (OOP_GET(pf, aHidd_PixFmt_ColorModel) == vHidd_ColorModel_TrueColor)
        {
            OOP_GetAttr(pf, aHidd_PixFmt_Depth, &depth);
            if ((ULONG)depth > *wanteddepth)
                *wanteddepth = (ULONG)depth;
        }
        else
        {
            *wanteddepth = 24;
        }
    }

    OOP_GetAttr(sync, aHidd_Sync_HDisp, &width);
    OOP_GetAttr(sync, aHidd_Sync_VDisp, &height);

    *wantedwidth   = (ULONG)width;
    *wantedheight  = (ULONG)height;
}

void UpdateDisplayMode(struct HIDDCompositorData *compdata)
{
    ULONG wantedwidth = 0, wantedheight = 0, wanteddepth = 16;
    ULONG found_depth;
    IPTR  modeid;

    D(bug("[GLCompositor] %s()\n", __func__));

    if (compdata->flags & COMPSTATEF_HASALPHA)
        CalculateParametersAlpha(compdata, &wantedwidth, &wantedheight, &wanteddepth);
    else
        CalculateParametersRegular(compdata, &wantedwidth, &wantedheight, &wanteddepth);

    D(bug("[GLCompositor] %s: Preferred mode %ldx%ldx%d\n", __func__, wantedwidth, wantedheight, wanteddepth));

    modeid = FindBestHiddMode(compdata, wantedwidth, wantedheight, wanteddepth, &found_depth);
    D(bug("[GLCompositor] %s: Composition Display ModeID 0x%08X [current 0x%08X]\n", __func__, modeid, compdata->displaymode));

    if (modeid != compdata->displaymode)
    {
        struct TagItem gctags[] =
        {
            { aHidd_GC_Foreground, 0x99999999 },
            { TAG_DONE           , 0          }
        };

        compdata->displaymode = modeid;
        compdata->displaydepth = wanteddepth;
        compdata->modeschanged = TRUE;

        if (found_depth < 24)
            gctags[0].ti_Data = 0x9492;

        OOP_SetAttrs(compdata->gc, gctags);
    }
}
