/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Tell how many characters will fit into a box.
*/
#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <proto/graphics.h>

        AROS_LH8(ULONG, TextFit,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort   *, rp, A1),
        AROS_LHA(CONST_STRPTR       , string, A0),
        AROS_LHA(UWORD              , strLen, D0),
        AROS_LHA(struct TextExtent *, textExtent, A2),
        AROS_LHA(struct TextExtent *, constrainingExtent, A3),
        AROS_LHA(WORD               , strDirection, D1),
        AROS_LHA(UWORD              , constrainingBitWidth, D2),
        AROS_LHA(UWORD              , constrainingBitHeight, D3),

/*  LOCATION */
        struct GfxBase *, GfxBase, 116, Graphics)

/*  FUNCTION
        Tries to fill the given space with as many characters of the
        font in rp as possible and returns that number.

    INPUTS
        rp - Use the settings in this RastPort (e.g. Font)
        string - Use this string
        strLen - The length of the string
        textExtent - The size actually occupied will be returned here
        constrainingExtent - If non-NULL, the routine will use the
                dimensions of the box described here
        strDirection - In which is the next character. Must be either 1
                or -1. If it is -1, then string must point to the end (the
                first character to check) of the text to fit (this is for
                checking text which runs from right to left).
        constrainingBitWidth - If constrainingExtent is NULL, then this
                is the width of the bounding box.
        constrainingBitHeight - If constrainingExtent is NULL, then this
                is the height of the bounding box.

    RESULT
        The number of characters which fit in the bounding box.
        If any characters fit in the bounding box, then textExtent will
        tell how large the minimal bounding box for the string is.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        TextLength()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct TextFont *tf = rp->Font;
    struct TextFontExtension_intern *utf8 = utf8_font(tf);
    ULONG retval = 0;

    if(strLen && (constrainingBitHeight >= tf->tf_YSize)) {
        BOOL ok = TRUE;

        textExtent->te_Extent.MinX = 0;
        textExtent->te_Extent.MinY = -tf->tf_Baseline;
        textExtent->te_Extent.MaxX = 0;
        textExtent->te_Extent.MaxY = tf->tf_YSize - tf->tf_Baseline - 1;
        textExtent->te_Width       = 0;
        textExtent->te_Height      = tf->tf_YSize;

        if(constrainingExtent) {
            if(constrainingExtent->te_Extent.MinY > textExtent->te_Extent.MinY
                    ||
                    constrainingExtent->te_Extent.MaxY < textExtent->te_Extent.MaxY
                    ||
                    constrainingExtent->te_Height < textExtent->te_Height) {
                ok = FALSE;
            }
        }

        if(ok) {
            while(strLen) {
                struct TextExtent char_extent;
                WORD newwidth, newminx, newmaxx, minx, maxx;
                CONST_STRPTR c = string;
                ULONG n = 1;

                /* UTF-8 fonts fit whole characters and count their bytes */
                if(utf8) {
                    if(strDirection < 0) {
                        while(n < strLen && n < 4 && (*c & 0xC0) == 0x80) {
                            c--;
                            n++;
                        }
                    } else {
                        ULONG cp;

                        n = utf8_decode(string, strLen, &cp);
                    }
                }

                TextExtent(rp, c, n, &char_extent);
                if(!utf8)
                    string += strDirection;
                else
                    string = (strDirection < 0) ? c - 1 : string + n;

                newwidth = textExtent->te_Width + char_extent.te_Width;
                minx = textExtent->te_Width + char_extent.te_Extent.MinX;
                maxx = textExtent->te_Width + char_extent.te_Extent.MaxX;

                newminx = (minx < textExtent->te_Extent.MinX) ?
                          minx : textExtent->te_Extent.MinX;
                newmaxx = (maxx > textExtent->te_Extent.MaxX) ?
                          maxx : textExtent->te_Extent.MaxX;

                if((ULONG)(newmaxx - newminx + 1) > constrainingBitWidth)
                    break;

                if(constrainingExtent) {
                    if(constrainingExtent->te_Extent.MinX > newminx) break;
                    if(constrainingExtent->te_Extent.MaxX < newmaxx) break;
                    if(constrainingExtent->te_Width < newwidth) break;
                }

                textExtent->te_Width = newwidth;
                textExtent->te_Extent.MinX = newminx;
                textExtent->te_Extent.MaxX = newmaxx;

                retval += n;
                strLen -= n;

            } /* while(strLen) */

        } /* if (ok) */

    } /* if (strLen && (constrainingBitHeight >= tf->tf_YSize)) */

    if(retval == 0) {
        textExtent->te_Width = 0;
        textExtent->te_Height = 0;
        textExtent->te_Extent.MinX = 0;
        textExtent->te_Extent.MinY = 0;
        textExtent->te_Extent.MaxX = 0;
        textExtent->te_Extent.MaxY = 0;
    }

    return retval;

    AROS_LIBFUNC_EXIT

} /* TextFit */
