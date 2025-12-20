/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Pixel Format Conversion Utilities

    This header provides inline functions for packing and unpacking
    pixels to/from ARGB32 format. All internal pixel operations use
    ARGB32 as the canonical format.
*/

#ifndef CYBERGFX_PIXEL_FORMAT_H
#define CYBERGFX_PIXEL_FORMAT_H

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
    return (((ULONG)a) << 24) | (((ULONG)r) << 16) | (((ULONG)g) << 8) | ((ULONG)b);
#else
    /* Little endian: pack so memory bytes are A,R,G,B */
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
    /* Little endian: memory bytes are A,R,G,B */
    *b = (pixel >> 24) & 0xFF;
    *g = (pixel >> 16) & 0xFF;
    *r = (pixel >> 8) & 0xFF;
    *a = pixel & 0xFF;
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

#endif /* CYBERGFX_PIXEL_FORMAT_H */
