/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Misc font help funcs
*/

#include <proto/exec.h>
#include <proto/oop.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include <exec/memory.h>
#include <graphics/text.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"
#include "fontsupport.h"

/****************************************************************************************/

ULONG CalcHashIndex(IPTR n, UWORD hashsize)
{
    UBYTE Index = (n        & 0xff)
                  + ((n >>  8) & 0xff)
                  + ((n >> 16) & 0xff)
                  + ((n >> 24) & 0xff)
#if __WORDSIZE == 64
                  + ((n >> 32) & 0xff)
                  + ((n >> 40) & 0xff)
                  + ((n >> 48) & 0xff)
                  + ((n >> 56) & 0xff)
#endif
                  ;

    Index &= (hashsize - 1);

    return Index;
}
/****************************************************************************************/

/* Functions for solving the TextFontExtension problem using hashes */

#define GFBI(x) ((struct GfxBase_intern *)x)

/****************************************************************************************/

struct tfe_hashnode *tfe_hashlookup(struct TextFont *tf, struct GfxBase *GfxBase)
{

    ULONG idx = CalcHashIndex((IPTR)tf, TFE_HASHTABSIZE);
    struct tfe_hashnode *n = NULL;


    ObtainSemaphoreShared(&GFBI(GfxBase)->tfe_hashtab_sema);

    for(n = GFBI(GfxBase)->tfe_hashtab[idx]; n; n = n->next) {
        if(n->back == tf) {
            break;
        }
    }

    ReleaseSemaphore(&GFBI(GfxBase)->tfe_hashtab_sema);

    return n;
}

/****************************************************************************************/

struct tfe_hashnode *tfe_hashnode_create(struct GfxBase *GfxBase)
{
    struct tfe_hashnode *n;

    n = AllocMem(sizeof(struct tfe_hashnode), MEMF_ANY | MEMF_CLEAR);

    return n;
}

/****************************************************************************************/

void tfe_hashadd(struct tfe_hashnode *hn
                 , struct TextFont *tf
                 , struct TextFontExtension      *etf
                 , struct GfxBase *GfxBase)

{
    ULONG idx = CalcHashIndex((IPTR)tf, TFE_HASHTABSIZE);

    hn->back = tf;
    hn->ext  = etf;

    ObtainSemaphore(&GFBI(GfxBase)->tfe_hashtab_sema);

    hn->next = GFBI(GfxBase)->tfe_hashtab[idx];
    GFBI(GfxBase)->tfe_hashtab[idx] = hn;

    ReleaseSemaphore(&GFBI(GfxBase)->tfe_hashtab_sema);

    return;
}

/****************************************************************************************/

void tfe_hashdelete(struct TextFont *tf, struct GfxBase *GfxBase)
{
    ULONG idx = CalcHashIndex((IPTR)tf, TFE_HASHTABSIZE);
    struct tfe_hashnode *n, *last = NULL;

    ObtainSemaphore(&GFBI(GfxBase)->tfe_hashtab_sema);

    for(n = GFBI(GfxBase)->tfe_hashtab[idx]; n; n = n->next) {
        if(n->back == tf) {
            if(last)
                last->next = n->next;
            else
                GFBI(GfxBase)->tfe_hashtab[idx] = n->next;

            FreeMem(n, sizeof(struct tfe_hashnode));
            break;
        }
        last = n;
    }

    ReleaseSemaphore(&GFBI(GfxBase)->tfe_hashtab_sema);

    return;

}

/****************************************************************************************/

/* Malformed sequences take one byte and decode to U+FFFD */
ULONG utf8_decode(CONST_STRPTR s, ULONG len, ULONG *cp)
{
    UBYTE c = s[0];
    ULONG n, i, v;

    *cp = 0xFFFD;

    if (c < 0x80)
    {
        *cp = c;
        return 1;
    }
    if (c >= 0xC2 && c <= 0xDF)
        n = 2, v = c & 0x1F;
    else if (c >= 0xE0 && c <= 0xEF)
        n = 3, v = c & 0x0F;
    else if (c >= 0xF0 && c <= 0xF4)
        n = 4, v = c & 0x07;
    else
        return 1;

    if (n > len)
        return 1;
    for (i = 1; i < n; i++)
    {
        if ((s[i] & 0xC0) != 0x80)
            return 1;
        v = (v << 6) | (s[i] & 0x3F);
    }
    if ((n == 3 && v < 0x800) || (n == 4 && (v < 0x10000 || v > 0x10FFFF)) ||
        (v >= 0xD800 && v <= 0xDFFF))
        return 1;

    *cp = v;
    return n;
}

/****************************************************************************************/

ULONG font_glyph_utf8(struct TextFont *tf, struct TextFontExtension_intern *utf8,
                      CONST_STRPTR s, ULONG len, struct fontglyph *g,
                      struct GfxBase *GfxBase)
{
    struct TextGlyph *tg = NULL;
    ULONG cp, n;

    n = utf8_decode(s, len, &cp);

    if (cp > 255 && utf8->glyphhook)
        tg = (struct TextGlyph *)CallHookPkt(utf8->glyphhook, tf, &cp);

    if (tg)
    {
        g->data   = tg->tg_CharData;
        g->aadata = tg->tg_AAData;
        g->pos    = 0;
        g->modulo = tg->tg_Modulo;
        g->width  = tg->tg_Width;
        g->kern   = tg->tg_Kern;
        g->space  = tg->tg_Space;
    }
    else
        font_glyph8(tf, cp, g);

    return n;
}

/****************************************************************************************/

UBYTE *colorfontbm_to_chunkybuffer(struct TextFont *font, struct GfxBase *GfxBase)
{
    ULONG  width, height;
    UBYTE *chunky;

    width  = font->tf_Modulo * 8;
    height = font->tf_YSize;

    chunky = AllocVec(width * height, MEMF_CLEAR);
    if(chunky) {
        struct BitMap bm;
        struct RastPort rp;
        UBYTE d, shift = 1, plane = 0;

        InitBitMap(&bm, CTF(font)->ctf_Depth, width, height);

        for(d = 0; d < CTF(font)->ctf_Depth; d++, shift <<= 1) {
            if(CTF(font)->ctf_PlanePick & shift) {
                bm.Planes[d] = (PLANEPTR)(CTF(font)->ctf_CharData[plane++]);
            } else {
                bm.Planes[d] = (CTF(font)->ctf_PlaneOnOff & shift) ? (PLANEPTR) - 1 : NULL;
            }
        }

        InitRastPort(&rp);
        rp.BitMap = &bm;

        ReadPixelArray8(&rp, 0, 0, width - 1, height - 1, chunky, NULL);
    }

    return chunky;
}

/****************************************************************************************/



