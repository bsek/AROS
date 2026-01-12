/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - SIMD Abstraction Layer

    This header provides a unified interface for SIMD operations across
    different architectures (SSE2, NEON) used in anti-aliased rendering.
*/

#ifndef CYBERGFX_SIMD_H
#define CYBERGFX_SIMD_H

#include "../../zunegfx_intern.h"
#include <exec/types.h>

/* SIMD capability levels */
typedef enum {
  CYBERGFX_SIMD_NONE = 0,
  CYBERGFX_SIMD_SSE2, /* 128-bit, 4-wide float (2001+) */
  CYBERGFX_SIMD_AVX2, /* 256-bit, 8-wide float (2013+) */
  CYBERGFX_SIMD_NEON  /* 128-bit, 4-wide float (ARM) */
} cybergfx_simd_level;

/* Detect available SIMD support */
cybergfx_simd_level cybergfx_get_simd_level(void);

/* Batch SDF computation for rounded rectangles
 *
 * Computes signed distance field values for 4 pixels at once using SIMD.
 * Falls back to scalar implementation if SIMD is not available.
 *
 * Parameters:
 *   rel_x: Array of 4 relative X coordinates (pixel center - rect center)
 *   rel_y: Relative Y coordinate (same for all 4 pixels in row)
 *   half_w: Half-width of rectangle
 *   half_h: Half-height of rectangle
 *   radius: Corner radius
 *   dist_out: Output array for 4 distance values
 */
void cybergfx_sdf_roundrect_batch4(const float rel_x[4], float rel_y,
                                   float half_w, float half_h, float radius,
                                   float dist_out[4]);

/* Batch SDF computation for 8 pixels (AVX2 optimized, falls back to 2x batch4)
 *
 * Same as batch4 but processes 8 pixels at once using AVX2 when available.
 * On non-AVX2 systems, automatically falls back to 2x batch4 calls.
 */
void cybergfx_sdf_roundrect_batch8(const float rel_x[8], float rel_y,
                                   float half_w, float half_h, float radius,
                                   float dist_out[8]);

/* Batch alpha computation from SDF distances
 *
 * Computes alpha values (fill and line) for 4 pixels using SIMD smoothstep.
 *
 * Parameters:
 *   dist_inner: Array of 4 inner SDF distances
 *   dist_outer: Array of 4 outer SDF distances
 *   aa_edge_neg: Negative AA edge (-smoothness)
 *   aa_edge_pos: Positive AA edge (+smoothness)
 *   hasFill: Whether fill is enabled
 *   hasBorder: Whether border is enabled
 *   alphaFill_out: Output array for 4 fill alpha values
 *   alphaLine_out: Output array for 4 line alpha values
 */
void cybergfx_compute_alphas_batch4(const float dist_inner[4],
                                    const float dist_outer[4],
                                    float aa_edge_neg, float aa_edge_pos,
                                    int hasFill, int hasBorder,
                                    float alphaFill_out[4],
                                    float alphaLine_out[4]);

/* Batch alpha computation for 8 pixels (AVX2 optimized) */
void cybergfx_compute_alphas_batch8(const float dist_inner[8],
                                    const float dist_outer[8],
                                    float aa_edge_neg, float aa_edge_pos,
                                    int hasFill, int hasBorder,
                                    float alphaFill_out[8],
                                    float alphaLine_out[8]);

/* Batch pixel blending with source-over compositing
 *
 * Blends fill and border colors onto 4 background pixels using SIMD.
 * Performs efficient ARGB32 unpacking, floating-point blending, and packing.
 *
 * Parameters:
 *   bg_pixels: Array of 4 background pixels (ARGB32 or native format)
 *   alphaFill: Array of 4 fill alpha values
 *   alphaLine: Array of 4 line alpha values
 *   fc_r, fc_g, fc_b: Fill color components (0-255)
 *   fc_alpha_scale: Fill alpha scale factor
 *   o_r, o_g, o_b: Outline color components (0-255)
 *   o_alpha_scale: Outline alpha scale factor
 *   hasFill: Whether fill is enabled
 *   hasBorder: Whether border is enabled
 *   result_pixels: Output array for 4 blended pixels
 *   changed_mask: Output bitmask indicating which pixels changed (bit 0-3)
 */
void cybergfx_blend_aa_pixels_batch4(
    const ULONG bg_pixels[4], const float alphaFill[4],
    const float alphaLine[4], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[4],
    unsigned int *changed_mask);

/* Batch pixel blending for 8 pixels (AVX2 optimized) */
void cybergfx_blend_aa_pixels_batch8(
    const ULONG bg_pixels[8], const float alphaFill[8],
    const float alphaLine[8], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[8],
    unsigned int *changed_mask);

/* Pack RGBA components into ARGB32 format (batch of 4)
 *
 * Vectorized packing of 4 sets of RGBA components into ARGB32 pixels.
 * Uses SIMD when available for optimal performance.
 *
 * Parameters:
 *   a: Array of 4 alpha values (0-255)
 *   r: Array of 4 red values (0-255)
 *   g: Array of 4 green values (0-255)
 *   b: Array of 4 blue values (0-255)
 *   pixels_out: Output array for 4 ARGB32 pixels
 */
void cybergfx_make_argb32_batch4(const UBYTE a[4], const UBYTE r[4],
                                 const UBYTE g[4], const UBYTE b[4],
                                 ULONG pixels_out[4]);

/* Texture helpers */
void cybergfx_blit_argb32_unity(const UBYTE *src, ULONG *dst, int width,
                                const struct InternalColor *tint);

/* Circle helpers */
void cybergfx_sdf_circle_batch4(const float rel_x[4], float rel_y, float radius,
                                float dist_out[4]);
void cybergfx_sdf_circle_batch8(const float rel_x[8], float rel_y, float radius,
                                float dist_out[8]);
void cybergfx_circle_alphas_batch4(const float dist[4], float border_width,
                                   int hasFill, int hasBorder, float fill_a[4],
                                   float border_a[4]);
void cybergfx_circle_alphas_batch8(const float dist[8], float border_width,
                                   int hasFill, int hasBorder, float fill_a[8],
                                   float border_a[8]);

/* Texture alpha blending - batch operations for ARGB32 textures
 *
 * These functions perform Porter-Duff "source over" alpha compositing
 * on batches of 4 or 8 pixels using SIMD instructions when available.
 *
 * The blending formula is:
 *   out = src * src_alpha + dst * (1 - src_alpha)
 *
 * Parameters:
 *   dst: Array of destination pixels (ARGB32 format, modified in-place)
 *   src: Array of source pixels (ARGB32 format)
 *
 * Note: Pixels are expected in native ARGB32 format (0xAARRGGBB)
 */
void cybergfx_blend_argb32_batch4(ULONG dst[4], const ULONG src[4]);
void cybergfx_blend_argb32_batch8(ULONG dst[8], const ULONG src[8]);

/* Texture alpha blending for scanlines
 *
 * Blends an entire row of source pixels over destination pixels.
 * Automatically uses the best available SIMD path (AVX2 > SSE2 > scalar).
 *
 * Parameters:
 *   dst: Destination pixel buffer (ARGB32 format, modified in-place)
 *   src: Source pixel buffer (ARGB32 format)
 *   count: Number of pixels to blend
 */
void cybergfx_blend_argb32_row(ULONG *dst, const ULONG *src, int count);

#endif /* CYBERGFX_SIMD_H */
