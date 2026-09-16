/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Pixel Format Conversion Utilities

    This header provides inline functions for packing and unpacking
    pixels to/from ARGB32 format. All internal pixel operations use
    ARGB32 as the canonical format.
*/

#ifndef CYBERGFX_PIXEL_FORMAT_H
#define CYBERGFX_PIXEL_FORMAT_H

#include <aros/cpu.h>
#include <exec/types.h>

/**
 * pack_argb32
 *
 * Packs ARGB color components into a ULONG pixel value in native
 * byte order for ARGB32 format.
 *
 * Parameters:
 *   a, r, g, b: Color components (0-255)
 *
 * Returns:
 *   ULONG pixel value in native ARGB32 format
 */
static inline ULONG pack_argb32(UBYTE a, UBYTE r, UBYTE g, UBYTE b) {
#if AROS_BIG_ENDIAN
    /* Big endian: ULONG 0xAARRGGBB stored as bytes AA RR GG BB — matches PIXFMT_ARGB32 */
    return (((ULONG)a) << 24) | (((ULONG)r) << 16) | (((ULONG)g) << 8) | ((ULONG)b);
#else
    /* Little endian: PIXFMT_ARGB32 means memory bytes AA RR GG BB.
     * A ULONG stored little-endian puts low byte first, so we pack
     * with B in high bits so memory order becomes AA RR GG BB. */
    return (((ULONG)b) << 24) | (((ULONG)g) << 16) | (((ULONG)r) << 8) | ((ULONG)a);
#endif
}

/**
 * unpack_argb32
 *
 * Unpacks a ULONG pixel value from native ARGB32 format
 * into ARGB color components.
 *
 * Parameters:
 *   pixel: The pixel value to unpack
 *   a, r, g, b: Output color components (0-255)
 */
static inline void unpack_argb32(ULONG pixel, UBYTE *a, UBYTE *r, UBYTE *g, UBYTE *b) {
#if AROS_BIG_ENDIAN
    *a = (pixel >> 24) & 0xFF;
    *r = (pixel >> 16) & 0xFF;
    *g = (pixel >> 8) & 0xFF;
    *b = pixel & 0xFF;
#else
    /* Little endian: memory bytes are AA RR GG BB, ULONG is BBGGRRAA */
    *a = pixel & 0xFF;
    *r = (pixel >> 8) & 0xFF;
    *g = (pixel >> 16) & 0xFF;
    *b = (pixel >> 24) & 0xFF;
#endif
}

/**
 * pack_argb32_logical
 *
 * Packs ARGB color components into a ULONG pixel value in logical
 * ARGB32 format (A in bits 24-31, R in 16-23, G in 8-15, B in 0-7).
 * This format is used by CyberGraphX Read/WritePixelArray with
 * CYBERGFX_PIXELFORMAT_ARGB32.
 *
 * Parameters:
 *   a, r, g, b: Color components (0-255)
 *
 * Returns:
 *   ULONG pixel value in logical ARGB32 format
 */
static inline ULONG pack_argb32_logical(UBYTE a, UBYTE r, UBYTE g, UBYTE b) {
    return (((ULONG)a) << 24) | (((ULONG)r) << 16) | (((ULONG)g) << 8) | ((ULONG)b);
}

/**
 * unpack_argb32_logical
 *
 * Unpacks a ULONG pixel value from logical ARGB32 format
 * into ARGB color components. This format is used by CyberGraphX
 * Read/WritePixelArray with CYBERGFX_PIXELFORMAT_ARGB32.
 *
 * Parameters:
 *   pixel: The pixel value to unpack
 *   a, r, g, b: Output color components (0-255)
 */
static inline void unpack_argb32_logical(ULONG pixel, UBYTE *a, UBYTE *r, UBYTE *g, UBYTE *b) {
    *a = (pixel >> 24) & 0xFF;
    *r = (pixel >> 16) & 0xFF;
    *g = (pixel >> 8) & 0xFF;
    *b = pixel & 0xFF;
}

/**
 * blend_argb32
 *
 * Alpha blends a source pixel over a destination pixel using integer math.
 * Uses the standard Porter-Duff "source over" compositing operation:
 *   out = src * src_alpha + dst * (1 - src_alpha)
 *
 * Uses fixed-point arithmetic (scale to 0-256) for efficient integer blending.
 *
 * Parameters:
 *   dst_pixel: Destination pixel in native ARGB32 format
 *   src_pixel: Source pixel in native ARGB32 format
 *
 * Returns:
 *   Blended pixel in native ARGB32 format
 */
static inline ULONG blend_argb32(ULONG dst_pixel, ULONG src_pixel) {
    UBYTE src_a, src_r, src_g, src_b;
    UBYTE dst_a, dst_r, dst_g, dst_b;

    unpack_argb32(src_pixel, &src_a, &src_r, &src_g, &src_b);

    /* Fast path: fully transparent source - return destination unchanged */
    if (src_a == 0)
        return dst_pixel;

    /* Fast path: fully opaque source - return source */
    if (src_a == 255)
        return src_pixel;

    unpack_argb32(dst_pixel, &dst_a, &dst_r, &dst_g, &dst_b);

    /* Fixed-point blending: scale alpha to 0-256 for efficient integer math */
    UWORD alpha = src_a + 1;  /* 1-256 range avoids divide by 255 */
    UWORD inv_alpha = 257 - alpha;  /* Complementary for 256 total */

    UBYTE out_r = (UBYTE)((alpha * src_r + inv_alpha * dst_r) >> 8);
    UBYTE out_g = (UBYTE)((alpha * src_g + inv_alpha * dst_g) >> 8);
    UBYTE out_b = (UBYTE)((alpha * src_b + inv_alpha * dst_b) >> 8);
    UBYTE out_a = 255;  /* Result is fully opaque after blending */

    return pack_argb32(out_a, out_r, out_g, out_b);
}

/**
 * blend_argb32_alpha
 *
 * Alpha blends source color components over a destination pixel.
 * Useful when source components are already unpacked.
 *
 * Parameters:
 *   dst_pixel: Destination pixel in native ARGB32 format
 *   src_a, src_r, src_g, src_b: Source color components (0-255)
 *
 * Returns:
 *   Blended pixel in native ARGB32 format
 */
static inline ULONG blend_argb32_alpha(ULONG dst_pixel, UBYTE src_a, UBYTE src_r, UBYTE src_g, UBYTE src_b) {
    /* Fast path: fully transparent source - return destination unchanged */
    if (src_a == 0)
        return dst_pixel;

    /* Fast path: fully opaque source - return source */
    if (src_a == 255)
        return pack_argb32(255, src_r, src_g, src_b);

    UBYTE dst_a, dst_r, dst_g, dst_b;
    unpack_argb32(dst_pixel, &dst_a, &dst_r, &dst_g, &dst_b);

    /* Fixed-point blending: scale alpha to 0-256 for efficient integer math */
    UWORD alpha = src_a + 1;
    UWORD inv_alpha = 257 - alpha;

    UBYTE out_r = (UBYTE)((alpha * src_r + inv_alpha * dst_r) >> 8);
    UBYTE out_g = (UBYTE)((alpha * src_g + inv_alpha * dst_g) >> 8);
    UBYTE out_b = (UBYTE)((alpha * src_b + inv_alpha * dst_b) >> 8);

    return pack_argb32(255, out_r, out_g, out_b);
}

/**
 * argb32_logical_to_native / argb32_native_to_logical
 *
 * Convert between logical ARGB32 (as used by CyberGraphX ReadPixelArray/
 * WritePixelArray with CYBERGFX_PIXELFORMAT_ARGB32) and native ARGB32
 * (as used by pack_argb32/unpack_argb32 for direct pixel buffer access).
 *
 * On big-endian systems these are identity operations.
 * On little-endian systems they perform a byte swap.
 */
static inline ULONG argb32_logical_to_native(ULONG pixel) {
#if AROS_BIG_ENDIAN
    return pixel;
#else
    return __builtin_bswap32(pixel);
#endif
}

static inline ULONG argb32_native_to_logical(ULONG pixel) {
#if AROS_BIG_ENDIAN
    return pixel;
#else
    return __builtin_bswap32(pixel);
#endif
}


/**
 * Byte offset -> ULONG shift. The CGX PIXFMT constants name components in
 * memory order (see the LockBitMapTagList() autodoc), so byte 0 is the
 * low-order byte of a little-endian ULONG and the high-order one on BE.
 */
#if AROS_BIG_ENDIAN
#define CGX_BYTE_SHIFT(off) ((UBYTE)((3 - (off)) * 8))
#else
#define CGX_BYTE_SHIFT(off) ((UBYTE)((off) * 8))
#endif

/**
 * zunegfx_derive_pixel_layout
 *
 * Fills in a ZunePixelLayout for a CGX pixel format. Returns FALSE for
 * anything that must not be written as 32-bit words, in which case the
 * caller has to use the RastPort path instead.
 */
static inline BOOL zunegfx_derive_pixel_layout(ULONG cgxfmt,
                                               ULONG bytes_per_pixel,
                                               struct ZunePixelLayout *out)
{
    UBYTE a, r, g, b;
    BOOL alpha = TRUE;

    out->valid = FALSE;
    out->bytes_per_pixel = (UBYTE)bytes_per_pixel;

    if (bytes_per_pixel != 4)
        return FALSE;

    switch (cgxfmt)
    {
    case PIXFMT_ARGB32: a = 0; r = 1; g = 2; b = 3; break;
    case PIXFMT_BGRA32: b = 0; g = 1; r = 2; a = 3; break;
    case PIXFMT_RGBA32: r = 0; g = 1; b = 2; a = 3; break;
    case PIXFMT_ABGR32: a = 0; b = 1; g = 2; r = 3; break;
    /* The 0-variants have an unused byte where alpha would be. Point the
     * alpha shift at it so writes stay in range, but flag it as opaque. */
    case PIXFMT_0RGB32: a = 0; r = 1; g = 2; b = 3; alpha = FALSE; break;
    case PIXFMT_BGR032: b = 0; g = 1; r = 2; a = 3; alpha = FALSE; break;
    case PIXFMT_RGB032: r = 0; g = 1; b = 2; a = 3; alpha = FALSE; break;
    case PIXFMT_0BGR32: a = 0; b = 1; g = 2; r = 3; alpha = FALSE; break;
    default:
        return FALSE;
    }

    out->a_shift = CGX_BYTE_SHIFT(a);
    out->r_shift = CGX_BYTE_SHIFT(r);
    out->g_shift = CGX_BYTE_SHIFT(g);
    out->b_shift = CGX_BYTE_SHIFT(b);
    out->has_alpha = alpha;
    out->valid = TRUE;
    return TRUE;
}

/* Pack/unpack in whatever order the locked bitmap actually uses. */
static inline ULONG pack_pixel(const struct ZunePixelLayout *l,
                               UBYTE a, UBYTE r, UBYTE g, UBYTE b)
{
    return (((ULONG)a) << l->a_shift) | (((ULONG)r) << l->r_shift) |
           (((ULONG)g) << l->g_shift) | (((ULONG)b) << l->b_shift);
}

static inline void unpack_pixel(const struct ZunePixelLayout *l, ULONG pixel,
                                UBYTE *a, UBYTE *r, UBYTE *g, UBYTE *b)
{
    *a = l->has_alpha ? (UBYTE)((pixel >> l->a_shift) & 0xFF) : 0xFF;
    *r = (UBYTE)((pixel >> l->r_shift) & 0xFF);
    *g = (UBYTE)((pixel >> l->g_shift) & 0xFF);
    *b = (UBYTE)((pixel >> l->b_shift) & 0xFF);
}

/* TRUE when the layout is exactly native ARGB32, i.e. the order the SIMD
 * helpers and pack_argb32() assume. */
static inline BOOL zunegfx_layout_is_argb32(const struct ZunePixelLayout *l)
{
    return l->valid && l->has_alpha &&
           l->a_shift == CGX_BYTE_SHIFT(0) && l->r_shift == CGX_BYTE_SHIFT(1) &&
           l->g_shift == CGX_BYTE_SHIFT(2) && l->b_shift == CGX_BYTE_SHIFT(3);
}


/* Convert between the locked bitmap's order and logical 0xAARRGGBB, which is
 * what the AA blend helpers and the public API speak. */
static inline ULONG layout_to_logical(const struct ZunePixelLayout *l, ULONG pixel)
{
    UBYTE a, r, g, b;
    unpack_pixel(l, pixel, &a, &r, &g, &b);
    return pack_argb32_logical(a, r, g, b);
}

static inline ULONG logical_to_layout(const struct ZunePixelLayout *l, ULONG pixel)
{
    UBYTE a, r, g, b;
    unpack_argb32_logical(pixel, &a, &r, &g, &b);
    return pack_pixel(l, a, r, g, b);
}


/* Convert between the locked bitmap's order and native ARGB32, which is the
 * space blend_argb32()/blend_argb32_alpha() and the SIMD helpers work in. */
static inline ULONG layout_to_argb32(const struct ZunePixelLayout *l, ULONG pixel)
{
    UBYTE a, r, g, b;
    unpack_pixel(l, pixel, &a, &r, &g, &b);
    return pack_argb32(a, r, g, b);
}

static inline ULONG argb32_to_layout(const struct ZunePixelLayout *l, ULONG pixel)
{
    UBYTE a, r, g, b;
    unpack_argb32(pixel, &a, &r, &g, &b);
    return pack_pixel(l, a, r, g, b);
}

#endif /* CYBERGFX_PIXEL_FORMAT_H */
