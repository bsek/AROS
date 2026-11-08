#ifndef FONTSUPPORT_H
#define FONTSUPPORT_H
/*
    Copyright © 1995-2026, The AROS Development Team. All rights reserved.
    $Id$

    Desc: Misc definitions internal to fonts.
    Lang: english
*/

#ifndef GRAHPICS_TEXT_H
#   include <graphics/text.h>
#endif

#define NUMCHARS(tf) 	((tf->tf_HiChar - tf->tf_LoChar) + 2)
#define CTF(x)      	((struct ColorTextFont *)x)

#define TFE_MATCHWORD   0xDFE7 /* randomly invented */

struct tfe_hashnode {
    struct tfe_hashnode 	*next;
    struct TextFont		*back;
    struct TextFontExtension	*ext;

    /* Color font data in chunky format */
    UBYTE   	    	    	*chunky_colorfont;
};

struct TextFontExtension_intern {
    struct TextFontExtension  tfe;
    struct tfe_hashnode      *hash;
    struct Hook              *glyphhook;    /* TA_GlyphHook */
    BOOL                      utf8;         /* TA_CharSet is UTF-8 */
};

/* One glyph of a string, from the font's tables or its glyph hook */
struct fontglyph {
    UBYTE *data;        /* template, tf_YSize rows of modulo bytes */
    UBYTE *aadata;      /* alpha, rows of modulo * 8 bytes, or NULL */
    ULONG  pos;         /* first column of the glyph */
    UWORD  modulo;
    UWORD  width;
    WORD   kern;
    WORD   space;
};

/* The extension of a UTF-8 font, NULL for an 8-bit one */
static inline struct TextFontExtension_intern *utf8_font(struct TextFont *tf)
{
    struct TextFontExtension_intern *tfe = (struct TextFontExtension_intern *)tf->tf_Extension;

    if ((tf->tf_Style & FSF_TAGGED) && tfe && tfe->tfe.tfe_MatchWord == TFE_MATCHWORD &&
        tfe->tfe.tfe_BackPtr == tf && tfe->utf8)
        return tfe;

    return NULL;
}

static inline void font_glyph8(struct TextFont *tf, ULONG c, struct fontglyph *g)
{
    ULONG idx = (c < tf->tf_LoChar || c > tf->tf_HiChar) ? NUMCHARS(tf) - 1 : c - tf->tf_LoChar;
    ULONG charloc = ((ULONG *)tf->tf_CharLoc)[idx];

    g->data   = tf->tf_CharData;
    g->aadata = ((tf->tf_Style & FSF_COLORFONT) &&
                 (CTF(tf)->ctf_Flags & CT_COLORMASK) == CT_ANTIALIAS) ? CTF(tf)->ctf_CharData[0] : NULL;
    g->pos    = charloc >> 16;
    g->modulo = tf->tf_Modulo;
    g->width  = charloc & 0xFFFF;
    g->kern   = tf->tf_CharKern ? ((WORD *)tf->tf_CharKern)[idx] : 0;
    g->space  = tf->tf_CharSpace ? ((WORD *)tf->tf_CharSpace)[idx] : tf->tf_XSize;
}

/* Glyph of the character at s, returns the bytes it takes */
static inline ULONG font_glyph(struct TextFont *tf, struct TextFontExtension_intern *utf8,
                               CONST_STRPTR s, ULONG len, struct fontglyph *g,
                               struct GfxBase *GfxBase)
{
    extern ULONG font_glyph_utf8(struct TextFont *, struct TextFontExtension_intern *,
                                 CONST_STRPTR, ULONG, struct fontglyph *, struct GfxBase *);

    if (utf8)
        return font_glyph_utf8(tf, utf8, s, len, g, GfxBase);

    font_glyph8(tf, (UBYTE)*s, g);
    return 1;
}

ULONG utf8_decode(CONST_STRPTR s, ULONG len, ULONG *cp);

#define TFE_INTERN(tfe) (*(struct TextFontExtension_intern **)&tfe)

extern struct tfe_hashnode *tfe_hashlookup(struct TextFont *tf, struct GfxBase *GfxBase);

extern void tfe_hashadd(struct tfe_hashnode *hn, struct TextFont *tf,
                        struct TextFontExtension *etf, struct GfxBase *GfxBase);


extern void tfe_hashdelete(struct TextFont *tf, struct GfxBase *GfxBase);
struct tfe_hashnode *tfe_hashnode_create(struct GfxBase *GfxBase);

UBYTE *colorfontbm_to_chunkybuffer(struct TextFont *font, struct GfxBase *GfxBase);


#endif /* FONTSUPPORT_H */
