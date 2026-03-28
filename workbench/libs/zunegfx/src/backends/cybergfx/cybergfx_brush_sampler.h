/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Brush Sampling for Anti-Aliased Rendering

    This module provides optimized brush sampling functions for use in
    anti-aliased rendering. Supports solid colors, textures, and linear
    gradients with optional caching for performance.
*/

#ifndef CYBERGFX_BRUSH_SAMPLER_H
#define CYBERGFX_BRUSH_SAMPLER_H

#include "../../zunegfx_intern.h"
#include <exec/types.h>

/**
 * PrepareBrushForRendering
 *
 * Pre-computes and caches data needed for efficient brush sampling.
 * Call this once before rendering a shape to populate the brush's
 * internal cache.
 *
 * Parameters:
 *   brush: The brush to prepare (modified in-place)
 *   rect_x, rect_y: Top-left corner of rectangle being drawn
 *   rect_w, rect_h: Dimensions of rectangle being drawn
 *
 * Notes:
 *   - For LINEAR_GRADIENT: Pre-computes gradient direction and step sizes
 *   - For TEXTURE: Caches direct pixel pointers and dimensions
 *   - For SOLID: No preparation needed
 *   - Sets brush->internal.valid = TRUE on success
 */
void PrepareBrushForRendering(struct RenderContext *rctx, struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                              UWORD rect_w, UWORD rect_h);

/**
 * SampleBrush
 *
 * Samples a brush at a specific pixel coordinate.
 *
 * Parameters:
 *   brush: The brush to sample from
 *   rect_x, rect_y: Top-left corner of rectangle being drawn
 *   rect_w, rect_h: Dimensions of rectangle being drawn
 *   px, py: Absolute screen coordinates to sample at
 *   r, g, b, a: Output color components (0-255)
 *
 * Notes:
 *   - For best performance, call PrepareBrushForRendering first
 *   - Works correctly even without preparation, but slower
 *   - Handles wrapping/clamping for textures
 *   - Clamps gradient t-values to [0, 1]
 */
void SampleBrush(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                 UWORD rect_w, UWORD rect_h, WORD px, WORD py, UBYTE *r, UBYTE *g,
                 UBYTE *b, UBYTE *a);

/**
 * SampleBrushBatch4
 *
 * Samples a brush at 4 pixel coordinates simultaneously using SIMD.
 *
 * Parameters:
 *   brush: The brush to sample from
 *   rect_x, rect_y: Top-left corner of rectangle being drawn
 *   rect_w, rect_h: Dimensions of rectangle being drawn
 *   px: Array of 4 x-coordinates to sample
 *   py: Y-coordinate (same for all 4 pixels)
 *   r, g, b, a: Output arrays of 4 color components each
 *
 * Notes:
 *   - Optimized for row-by-row rendering
 *   - All 4 pixels must be on the same scanline
 */
void SampleBrushBatch4(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                       UWORD rect_w, UWORD rect_h, const WORD px[4], WORD py,
                       UBYTE r[4], UBYTE g[4], UBYTE b[4], UBYTE a[4]);

/**
 * SampleBrushBatch8
 *
 * Samples a brush at 8 pixel coordinates simultaneously using SIMD.
 *
 * Parameters:
 *   brush: The brush to sample from
 *   rect_x, rect_y: Top-left corner of rectangle being drawn
 *   rect_w, rect_h: Dimensions of rectangle being drawn
 *   px: Array of 8 x-coordinates to sample
 *   py: Y-coordinate (same for all 8 pixels)
 *   r, g, b, a: Output arrays of 8 color components each
 *
 * Notes:
 *   - Optimized for AVX2 systems
 *   - All 8 pixels must be on the same scanline
 */
void SampleBrushBatch8(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                       UWORD rect_w, UWORD rect_h, const WORD px[8], WORD py,
                       UBYTE r[8], UBYTE g[8], UBYTE b[8], UBYTE a[8]);

/**
 * InterpolateGradientStops
 *
 * Helper function to interpolate a color from gradient stops.
 *
 * Parameters:
 *   stops: Array of gradient stops
 *   stop_count: Number of stops in array
 *   t: Position along gradient (0.0 to 1.0)
 *   r, g, b, a: Output color components
 */
void InterpolateGradientStops(const struct ZuneGradientStop *stops,
                              UWORD stop_count, float t, UBYTE *r, UBYTE *g,
                              UBYTE *b, UBYTE *a);

/**
 * CleanupBrushInternalState
 *
 * Cleans up any allocated memory in the brush's internal cache.
 * Call this when a brush is no longer needed to free resources.
 *
 * Parameters:
 *   brush: The brush to clean up
 *
 * Notes:
 *   - Safe to call multiple times on the same brush
 *   - Sets brush->internal.valid = FALSE
 *   - Frees any allocated internal.color memory
 */
void CleanupBrushInternalState(struct ZuneBrush *brush);

#endif /* CYBERGFX_BRUSH_SAMPLER_H */
