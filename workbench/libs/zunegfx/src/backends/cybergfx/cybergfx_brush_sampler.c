/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Brush Sampling Implementation
*/

#include "cybergfx_brush_sampler.h"
#include "clib/exec_protos.h"
#include "cybergfx_antialiasing.h"
#include <math.h>
#include <aros/debug.h>
#include <string.h>
#include <clib/alib_protos.h>

void PrepareBrushForRendering(struct RenderContext *rctx, struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                              UWORD rect_w, UWORD rect_h) {
    /* Point the brush at this context's scratch. The brush is typically a
     * temporary, so the cache must not live inside it. */
    if (!rctx || !brush)
        return;
    brush->internal = &rctx->brush_cache;

  if (!brush)
    return;

  BRUSHCACHE(brush)->valid = TRUE;

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    /* Convert relative coordinates to absolute */
    float start_x = rect_x + brush->data.linear.start.x;
    float start_y = rect_y + brush->data.linear.start.y;
    float end_x = rect_x + brush->data.linear.end.x;
    float end_y = rect_y + brush->data.linear.end.y;

    /* Calculate gradient vector */
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float length_sq = dx * dx + dy * dy;

    if (length_sq > 0.0f) {
      /* Pre-compute how much t changes per pixel in x/y directions */
      BRUSHCACHE(brush)->linear_cache.t_step_x = dx / length_sq;
      BRUSHCACHE(brush)->linear_cache.t_step_y = dy / length_sq;

      /* Calculate t value at rectangle origin */
      float origin_dx = rect_x - start_x;
      float origin_dy = rect_y - start_y;
      BRUSHCACHE(brush)->linear_cache.t_start =
          (origin_dx * dx + origin_dy * dy) / length_sq;
    } else {
      /* Degenerate gradient - zero length */
      BRUSHCACHE(brush)->linear_cache.t_step_x = 0.0f;
      BRUSHCACHE(brush)->linear_cache.t_step_y = 0.0f;
      BRUSHCACHE(brush)->linear_cache.t_start = 0.0f;
    }

    /* Pre-rasterize gradient if dimensions are reasonable (max 512KB) */
    ULONG pixel_count = (ULONG)rect_w * rect_h;
    if (pixel_count > 0 && pixel_count <= 131072 &&
        brush->data.linear.stops && brush->data.linear.stop_count > 0) { /* 131072 pixels = 512KB */
      /* Free old cache - we always regenerate since gradient params may have changed */
      if (BRUSHCACHE(brush)->linear_cache.rasterized_pixels) {
        FreeVec(BRUSHCACHE(brush)->linear_cache.rasterized_pixels);
        BRUSHCACHE(brush)->linear_cache.rasterized_pixels = NULL;
      }

      /* Allocate new cache */
      ULONG *pixels = AllocVec(pixel_count * sizeof(ULONG), MEMF_PUBLIC);
      if (pixels) {
        ULONG *dst = pixels;
        float row_t = BRUSHCACHE(brush)->linear_cache.t_start;

        for (UWORD y = 0; y < rect_h; y++) {
          float t = row_t;
          for (UWORD x = 0; x < rect_w; x++) {
            UBYTE r, g, b, a;
            InterpolateGradientStops(brush->data.linear.stops,
                                     brush->data.linear.stop_count, t,
                                     &r, &g, &b, &a);
            *dst++ = ZUNE_COLOR_ARGB32(a, r, g, b);
            t += BRUSHCACHE(brush)->linear_cache.t_step_x;
          }
          row_t += BRUSHCACHE(brush)->linear_cache.t_step_y;
        }

        BRUSHCACHE(brush)->linear_cache.rasterized_pixels = pixels;
        BRUSHCACHE(brush)->linear_cache.rasterized_width = rect_w;
        BRUSHCACHE(brush)->linear_cache.rasterized_height = rect_h;
      }
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_RADIAL_GRADIENT: {
    float center_x = rect_x + brush->data.radial.center.x;
    float center_y = rect_y + brush->data.radial.center.y;
    float radius = (float)brush->data.radial.radius;

    BRUSHCACHE(brush)->radial_cache.center_x = center_x;
    BRUSHCACHE(brush)->radial_cache.center_y = center_y;
    BRUSHCACHE(brush)->radial_cache.inv_radius = (radius > 0.001f) ? (1.0f / radius) : 0.0f;

    /* Pre-rasterize if dimensions are reasonable (max 512KB) */
    ULONG pixel_count = (ULONG)rect_w * rect_h;
    if (pixel_count > 0 && pixel_count <= 131072 &&
        brush->data.radial.stops && brush->data.radial.stop_count > 0 &&
        radius > 0.001f) {

      if (BRUSHCACHE(brush)->radial_cache.rasterized_pixels) {
        FreeVec(BRUSHCACHE(brush)->radial_cache.rasterized_pixels);
        BRUSHCACHE(brush)->radial_cache.rasterized_pixels = NULL;
      }

      ULONG *pixels = AllocVec(pixel_count * sizeof(ULONG), MEMF_PUBLIC);
      if (pixels) {
        ULONG *dst = pixels;
        float inv_r = BRUSHCACHE(brush)->radial_cache.inv_radius;

        for (UWORD y = 0; y < rect_h; y++) {
          float dy = (rect_y + y) - center_y;
          float dy2 = dy * dy;
          for (UWORD x = 0; x < rect_w; x++) {
            float dx = (rect_x + x) - center_x;
            float dist = sqrtf(dx * dx + dy2);
            float t = dist * inv_r;

            UBYTE r, g, b, a;
            InterpolateGradientStops(brush->data.radial.stops,
                                     brush->data.radial.stop_count, t,
                                     &r, &g, &b, &a);
            *dst++ = ZUNE_COLOR_ARGB32(a, r, g, b);
          }
        }

        BRUSHCACHE(brush)->radial_cache.rasterized_pixels = pixels;
        BRUSHCACHE(brush)->radial_cache.rasterized_width = rect_w;
        BRUSHCACHE(brush)->radial_cache.rasterized_height = rect_h;
      }
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_TEXTURE:
  case ZUNE_BRUSH_TYPE_DATATYPE: {
    struct ZuneTexture *tex = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                                  ? brush->data.texture.texture
                                  : brush->data.datatype.texture;
    if (!tex || !tex->pixel_data || !tex->valid) {
      BRUSHCACHE(brush)->valid = FALSE;
      break;
    }

    /* Cache direct pointer to pixel data */
    BRUSHCACHE(brush)->texture_cache.pixels = (ULONG *)tex->pixel_data;
    BRUSHCACHE(brush)->texture_cache.pitch_pixels = tex->pitch / 4;

    /* Cache source rectangle */
    struct ZuneRect src = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                              ? brush->data.texture.source
                              : brush->data.datatype.source;
    BRUSHCACHE(brush)->texture_cache.src_x = src.x;
    BRUSHCACHE(brush)->texture_cache.src_y = src.y;
    BRUSHCACHE(brush)->texture_cache.src_w = src.width ? src.width : tex->width;
    BRUSHCACHE(brush)->texture_cache.src_h = src.height ? src.height : tex->height;
    break;
  }

  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN:
    D(bug("PrepareBrushForRendering: Processing brush type %s\n",
          brush->type == ZUNE_BRUSH_TYPE_SOLID ? "SOLID" : "PEN"));

    /* Use inline color struct - no allocation needed */
    D(bug("PrepareBrushForRendering: Calling ZuneBrushToInternalColor with rctx=%p, brush=%p, color=%p\n", rctx, brush, &BRUSHCACHE(brush)->color));

    if (!ZuneBrushToInternalColor(rctx, brush, &BRUSHCACHE(brush)->color)) {
      BRUSHCACHE(brush)->valid = FALSE;
    }
    break;

  case ZUNE_BRUSH_TYPE_PATTERN: {
    /* Convert pens to ARGB32 colors */
    if (!brush->data.pattern.colormap) {
      D(bug("PrepareBrushForRendering: PATTERN - no colormap!\n"));
      BRUSHCACHE(brush)->valid = FALSE;
      break;
    }

    ULONG rgb[3];

    /* Get foreground color */
    GetRGB32(brush->data.pattern.colormap, brush->data.pattern.fg_pen, 1, rgb);
    BRUSHCACHE(brush)->pattern_cache.fg_color = ZUNE_COLOR_ARGB32(0xFF,
                                                                 rgb[0] >> 24,
                                                                 rgb[1] >> 24,
                                                                 rgb[2] >> 24);
    D(bug("PrepareBrushForRendering: PATTERN fg_pen=%ld -> RGB32=(%08lx,%08lx,%08lx) -> ARGB=%08lx\n",
          brush->data.pattern.fg_pen, rgb[0], rgb[1], rgb[2], BRUSHCACHE(brush)->pattern_cache.fg_color));

    /* Get background color */
    GetRGB32(brush->data.pattern.colormap, brush->data.pattern.bg_pen, 1, rgb);
    BRUSHCACHE(brush)->pattern_cache.bg_color = ZUNE_COLOR_ARGB32(0xFF,
                                                                 rgb[0] >> 24,
                                                                 rgb[1] >> 24,
                                                                 rgb[2] >> 24);
    D(bug("PrepareBrushForRendering: PATTERN bg_pen=%ld -> RGB32=(%08lx,%08lx,%08lx) -> ARGB=%08lx\n",
          brush->data.pattern.bg_pen, rgb[0], rgb[1], rgb[2], BRUSHCACHE(brush)->pattern_cache.bg_color));
    break;
  }

  default:
    BRUSHCACHE(brush)->valid = FALSE;
    break;
  }
}

/*****************************************************************************/

void InterpolateGradientStops(const struct ZuneGradientStop *stops,
                              UWORD stop_count, float t, UBYTE *r, UBYTE *g,
                              UBYTE *b, UBYTE *a) {

  if (stop_count == 0 || !stops) {
    *r = *g = *b = *a = 0;
    return;
  }

  /* Clamp t to valid range */
  t = clamp(t, 0.0f, 1.0f);

  /* Before first stop */
  if (t <= stops[0].position) {
    ULONG color = stops[0].color;
    *a = ZUNE_GET_ALPHA(color);
    *r = ZUNE_GET_RED(color);
    *g = ZUNE_GET_GREEN(color);
    *b = ZUNE_GET_BLUE(color);
    return;
  }

  /* After last stop */
  if (t >= stops[stop_count - 1].position) {
    ULONG color = stops[stop_count - 1].color;
    *a = ZUNE_GET_ALPHA(color);
    *r = ZUNE_GET_RED(color);
    *g = ZUNE_GET_GREEN(color);
    *b = ZUNE_GET_BLUE(color);
    return;
  }

  /* Find surrounding stops */
  WORD i;
  for (i = 0; i < stop_count - 1; i++) {
    if (t < stops[i + 1].position)
      break;
  }

  /* Interpolate between stops[i] and stops[i+1] */
  float pos0 = stops[i].position;
  float pos1 = stops[i + 1].position;
  float range = pos1 - pos0;

  if (range <= 0.0f) {
    /* Degenerate case - stops at same position */
    ULONG color = stops[i].color;
    *a = ZUNE_GET_ALPHA(color);
    *r = ZUNE_GET_RED(color);
    *g = ZUNE_GET_GREEN(color);
    *b = ZUNE_GET_BLUE(color);
    return;
  }

  float local_t = (t - pos0) / range;
  local_t = clamp(local_t, 0.0f, 1.0f);

  ULONG color0 = stops[i].color;
  ULONG color1 = stops[i + 1].color;

  UBYTE a0 = ZUNE_GET_ALPHA(color0), a1 = ZUNE_GET_ALPHA(color1);
  UBYTE r0 = ZUNE_GET_RED(color0), r1 = ZUNE_GET_RED(color1);
  UBYTE g0 = ZUNE_GET_GREEN(color0), g1 = ZUNE_GET_GREEN(color1);
  UBYTE b0 = ZUNE_GET_BLUE(color0), b1 = ZUNE_GET_BLUE(color1);

  *a = (UBYTE)((1.0f - local_t) * a0 + local_t * a1);
  *r = (UBYTE)((1.0f - local_t) * r0 + local_t * r1);
  *g = (UBYTE)((1.0f - local_t) * g0 + local_t * g1);
  *b = (UBYTE)((1.0f - local_t) * b0 + local_t * b1);
}

/*****************************************************************************/

/* Helper: Apply texture wrapping to coordinate */
static inline WORD ApplyTextureWrap(WORD coord, WORD size,
                                    enum ZuneBrushWrapMode wrap) {
  switch (wrap) {
  case ZUNE_BRUSH_WRAP_REPEAT: {
    coord = coord % size;
    if (coord < 0)
      coord += size;
    return coord;
  }

  case ZUNE_BRUSH_WRAP_CLAMP:
    return clamp(coord, 0, size - 1);

  case ZUNE_BRUSH_WRAP_MIRROR: {
    WORD doubled = size * 2;
    coord = coord % doubled;
    if (coord < 0)
      coord += doubled;
    if (coord >= size)
      coord = (doubled - 1) - coord;
    return coord;
  }

  default:
    return clamp(coord, 0, size - 1);
  }
}

/*****************************************************************************/

void SampleBrush(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                 UWORD rect_w, UWORD rect_h, WORD px, WORD py, UBYTE *r, UBYTE *g,
                 UBYTE *b, UBYTE *a) {

  if (!brush) {
    *r = *g = *b = *a = 0;
    return;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN: {
    /* Use stored ARGB components directly - they're already in correct format */
    *a = BRUSHCACHE(brush)->color.a;
    *r = BRUSHCACHE(brush)->color.r;
    *g = BRUSHCACHE(brush)->color.g;
    *b = BRUSHCACHE(brush)->color.b;
    break;
  }

  case ZUNE_BRUSH_TYPE_TEXTURE:
  case ZUNE_BRUSH_TYPE_DATATYPE: {
    if (!BRUSHCACHE(brush)->valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Calculate relative position in rectangle */
    WORD rel_x = px - rect_x;
    WORD rel_y = py - rect_y;

    /* Apply wrapping/clamping */
    WORD tex_x = ApplyTextureWrap(rel_x, BRUSHCACHE(brush)->texture_cache.src_w,
                                  brush->data.texture.wrap_u);
    WORD tex_y = ApplyTextureWrap(rel_y, BRUSHCACHE(brush)->texture_cache.src_h,
                                  brush->data.texture.wrap_v);

    /* Add source rectangle offset */
    tex_x += BRUSHCACHE(brush)->texture_cache.src_x;
    tex_y += BRUSHCACHE(brush)->texture_cache.src_y;

    /* Sample pixel from texture */
    ULONG pixel =
        BRUSHCACHE(brush)->texture_cache
            .pixels[tex_y * BRUSHCACHE(brush)->texture_cache.pitch_pixels + tex_x];

    *a = ZUNE_GET_ALPHA(pixel);
    *r = ZUNE_GET_RED(pixel);
    *g = ZUNE_GET_GREEN(pixel);
    *b = ZUNE_GET_BLUE(pixel);
    break;
  }

  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    if (!BRUSHCACHE(brush)->valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Use pre-rasterized cache if available and valid */
    if (BRUSHCACHE(brush)->linear_cache.rasterized_pixels &&
        BRUSHCACHE(brush)->linear_cache.rasterized_width > 0 &&
        BRUSHCACHE(brush)->linear_cache.rasterized_height > 0) {
      WORD rel_x = px - rect_x;
      WORD rel_y = py - rect_y;

      /* Clamp to valid range */
      if (rel_x < 0) rel_x = 0;
      if (rel_y < 0) rel_y = 0;
      if (rel_x >= BRUSHCACHE(brush)->linear_cache.rasterized_width)
        rel_x = BRUSHCACHE(brush)->linear_cache.rasterized_width - 1;
      if (rel_y >= BRUSHCACHE(brush)->linear_cache.rasterized_height)
        rel_y = BRUSHCACHE(brush)->linear_cache.rasterized_height - 1;

      ULONG pixel = BRUSHCACHE(brush)->linear_cache.rasterized_pixels[
          rel_y * BRUSHCACHE(brush)->linear_cache.rasterized_width + rel_x];
      *a = ZUNE_GET_ALPHA(pixel);
      *r = ZUNE_GET_RED(pixel);
      *g = ZUNE_GET_GREEN(pixel);
      *b = ZUNE_GET_BLUE(pixel);
      break;
    }

    /* Calculate gradient t using cached increments */
    float rel_x = px - rect_x;
    float rel_y = py - rect_y;
    float t = BRUSHCACHE(brush)->linear_cache.t_start +
              rel_x * BRUSHCACHE(brush)->linear_cache.t_step_x +
              rel_y * BRUSHCACHE(brush)->linear_cache.t_step_y;

    /* Interpolate between gradient stops */
    InterpolateGradientStops(brush->data.linear.stops,
                             brush->data.linear.stop_count, t, r, g, b, a);
    break;
  }

  case ZUNE_BRUSH_TYPE_RADIAL_GRADIENT: {
    if (!BRUSHCACHE(brush)->valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Use pre-rasterized cache if available */
    if (BRUSHCACHE(brush)->radial_cache.rasterized_pixels &&
        BRUSHCACHE(brush)->radial_cache.rasterized_width > 0 &&
        BRUSHCACHE(brush)->radial_cache.rasterized_height > 0) {
      WORD rel_x = px - rect_x;
      WORD rel_y = py - rect_y;

      if (rel_x < 0) rel_x = 0;
      if (rel_y < 0) rel_y = 0;
      if (rel_x >= BRUSHCACHE(brush)->radial_cache.rasterized_width)
        rel_x = BRUSHCACHE(brush)->radial_cache.rasterized_width - 1;
      if (rel_y >= BRUSHCACHE(brush)->radial_cache.rasterized_height)
        rel_y = BRUSHCACHE(brush)->radial_cache.rasterized_height - 1;

      ULONG pixel = BRUSHCACHE(brush)->radial_cache.rasterized_pixels[
          rel_y * BRUSHCACHE(brush)->radial_cache.rasterized_width + rel_x];
      *a = ZUNE_GET_ALPHA(pixel);
      *r = ZUNE_GET_RED(pixel);
      *g = ZUNE_GET_GREEN(pixel);
      *b = ZUNE_GET_BLUE(pixel);
      break;
    }

    /* Fallback: compute per-pixel */
    float dx = px - BRUSHCACHE(brush)->radial_cache.center_x;
    float dy = py - BRUSHCACHE(brush)->radial_cache.center_y;
    float dist = sqrtf(dx * dx + dy * dy);
    float t = dist * BRUSHCACHE(brush)->radial_cache.inv_radius;

    InterpolateGradientStops(brush->data.radial.stops,
                             brush->data.radial.stop_count, t, r, g, b, a);
    break;
  }

  case ZUNE_BRUSH_TYPE_PATTERN: {
    if (!BRUSHCACHE(brush)->valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Calculate relative position in rectangle */
    WORD rel_x = px - rect_x;
    WORD rel_y = py - rect_y;

    /* Pattern is 16x2, repeating */
    WORD pat_x = rel_x % 16;
    WORD pat_y = rel_y % 2;
    if (pat_x < 0) pat_x += 16;
    if (pat_y < 0) pat_y += 2;

    /* Get pattern bit (bit 15 is leftmost) */
    UWORD row_bits = brush->data.pattern.pattern[pat_y];
    BOOL is_fg = (row_bits & (1U << (15 - pat_x))) != 0;

    /* Select color */
    ULONG color = is_fg ? BRUSHCACHE(brush)->pattern_cache.fg_color
                        : BRUSHCACHE(brush)->pattern_cache.bg_color;

    *a = ZUNE_GET_ALPHA(color);
    *r = ZUNE_GET_RED(color);
    *g = ZUNE_GET_GREEN(color);
    *b = ZUNE_GET_BLUE(color);
    break;
  }

  default:
    *r = *g = *b = *a = 0;
    break;
  }
}

/*****************************************************************************/

void SampleBrushBatch4(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                       UWORD rect_w, UWORD rect_h, const WORD px[4], WORD py,
                       UBYTE r[4], UBYTE g[4], UBYTE b[4], UBYTE a[4]) {

  if (!brush) {
    for (WORD i = 0; i < 4; i++) {
      r[i] = g[i] = b[i] = a[i] = 0;
    }
    return;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN: {
    /* All pixels get same color */
    ULONG color = BRUSHCACHE(brush)->color.original_pixel;
    UBYTE sa = ZUNE_GET_ALPHA(color);
    UBYTE sr = ZUNE_GET_RED(color);
    UBYTE sg = ZUNE_GET_GREEN(color);
    UBYTE sb = ZUNE_GET_BLUE(color);

    for (WORD i = 0; i < 4; i++) {
      a[i] = sa;
      r[i] = sr;
      g[i] = sg;
      b[i] = sb;
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    if (!BRUSHCACHE(brush)->valid) {
      for (WORD i = 0; i < 4; i++) {
        r[i] = g[i] = b[i] = a[i] = 0;
      }
      break;
    }

    /* Use pre-rasterized cache if available and valid */
    if (BRUSHCACHE(brush)->linear_cache.rasterized_pixels &&
        BRUSHCACHE(brush)->linear_cache.rasterized_width > 0 &&
        BRUSHCACHE(brush)->linear_cache.rasterized_height > 0) {
      WORD rel_y = py - rect_y;
      if (rel_y < 0) rel_y = 0;
      if (rel_y >= BRUSHCACHE(brush)->linear_cache.rasterized_height)
        rel_y = BRUSHCACHE(brush)->linear_cache.rasterized_height - 1;

      ULONG *row = BRUSHCACHE(brush)->linear_cache.rasterized_pixels +
                   rel_y * BRUSHCACHE(brush)->linear_cache.rasterized_width;
      UWORD cache_w = BRUSHCACHE(brush)->linear_cache.rasterized_width;

      for (WORD i = 0; i < 4; i++) {
        WORD rel_x = px[i] - rect_x;
        if (rel_x < 0) rel_x = 0;
        if (rel_x >= cache_w) rel_x = cache_w - 1;

        ULONG pixel = row[rel_x];
        a[i] = ZUNE_GET_ALPHA(pixel);
        r[i] = ZUNE_GET_RED(pixel);
        g[i] = ZUNE_GET_GREEN(pixel);
        b[i] = ZUNE_GET_BLUE(pixel);
      }
      break;
    }

    /* Calculate base t for this row */
    float rel_y = py - rect_y;
    float base_t = BRUSHCACHE(brush)->linear_cache.t_start +
                   rel_y * BRUSHCACHE(brush)->linear_cache.t_step_y;

    /* Sample each pixel with incremental t */
    for (WORD i = 0; i < 4; i++) {
      float rel_x = px[i] - rect_x;
      float t = base_t + rel_x * BRUSHCACHE(brush)->linear_cache.t_step_x;

      InterpolateGradientStops(brush->data.linear.stops,
                               brush->data.linear.stop_count, t, &r[i], &g[i],
                               &b[i], &a[i]);
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_RADIAL_GRADIENT: {
    if (!BRUSHCACHE(brush)->valid) {
      for (WORD i = 0; i < 4; i++) {
        r[i] = g[i] = b[i] = a[i] = 0;
      }
      break;
    }

    /* Use pre-rasterized cache if available */
    if (BRUSHCACHE(brush)->radial_cache.rasterized_pixels &&
        BRUSHCACHE(brush)->radial_cache.rasterized_width > 0 &&
        BRUSHCACHE(brush)->radial_cache.rasterized_height > 0) {
      WORD rel_y = py - rect_y;
      if (rel_y < 0) rel_y = 0;
      if (rel_y >= BRUSHCACHE(brush)->radial_cache.rasterized_height)
        rel_y = BRUSHCACHE(brush)->radial_cache.rasterized_height - 1;

      ULONG *row = BRUSHCACHE(brush)->radial_cache.rasterized_pixels +
                   rel_y * BRUSHCACHE(brush)->radial_cache.rasterized_width;
      UWORD cache_w = BRUSHCACHE(brush)->radial_cache.rasterized_width;

      for (WORD i = 0; i < 4; i++) {
        WORD rel_x = px[i] - rect_x;
        if (rel_x < 0) rel_x = 0;
        if (rel_x >= cache_w) rel_x = cache_w - 1;

        ULONG pixel = row[rel_x];
        a[i] = ZUNE_GET_ALPHA(pixel);
        r[i] = ZUNE_GET_RED(pixel);
        g[i] = ZUNE_GET_GREEN(pixel);
        b[i] = ZUNE_GET_BLUE(pixel);
      }
      break;
    }

    /* Fallback to scalar */
    for (WORD i = 0; i < 4; i++) {
      SampleBrush(brush, rect_x, rect_y, rect_w, rect_h, px[i], py, &r[i],
                  &g[i], &b[i], &a[i]);
    }
    break;
  }

  default:
    /* Fall back to scalar sampling */
    for (WORD i = 0; i < 4; i++) {
      SampleBrush(brush, rect_x, rect_y, rect_w, rect_h, px[i], py, &r[i],
                  &g[i], &b[i], &a[i]);
    }
    break;
  }
}

/*****************************************************************************/

void CleanupBrushInternalState(struct ZuneBrush *brush) {
  if (!brush || !brush->internal) {
    return;
  }

  /* internal.color is now inline, no need to free */

  /* Free pre-rasterized gradient caches */
  if (BRUSHCACHE(brush)->linear_cache.rasterized_pixels) {
    FreeVec(BRUSHCACHE(brush)->linear_cache.rasterized_pixels);
    BRUSHCACHE(brush)->linear_cache.rasterized_pixels = NULL;
    BRUSHCACHE(brush)->linear_cache.rasterized_width = 0;
    BRUSHCACHE(brush)->linear_cache.rasterized_height = 0;
  }

  if (BRUSHCACHE(brush)->radial_cache.rasterized_pixels) {
    FreeVec(BRUSHCACHE(brush)->radial_cache.rasterized_pixels);
    BRUSHCACHE(brush)->radial_cache.rasterized_pixels = NULL;
    BRUSHCACHE(brush)->radial_cache.rasterized_width = 0;
    BRUSHCACHE(brush)->radial_cache.rasterized_height = 0;
  }

  /* Mark internal cache as invalid */
  BRUSHCACHE(brush)->valid = FALSE;
}

/*****************************************************************************/

void SampleBrushBatch8(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                       UWORD rect_w, UWORD rect_h, const WORD px[8], WORD py,
                       UBYTE r[8], UBYTE g[8], UBYTE b[8], UBYTE a[8]) {

  if (!brush) {
    for (WORD i = 0; i < 8; i++) {
      r[i] = g[i] = b[i] = a[i] = 0;
    }
    return;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN: {
    /* All pixels get same color */
    ULONG color = BRUSHCACHE(brush)->color.original_pixel;
    UBYTE sa = ZUNE_GET_ALPHA(color);
    UBYTE sr = ZUNE_GET_RED(color);
    UBYTE sg = ZUNE_GET_GREEN(color);
    UBYTE sb = ZUNE_GET_BLUE(color);

    for (WORD i = 0; i < 8; i++) {
      a[i] = sa;
      r[i] = sr;
      g[i] = sg;
      b[i] = sb;
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    if (!BRUSHCACHE(brush)->valid) {
      for (WORD i = 0; i < 8; i++) {
        r[i] = g[i] = b[i] = a[i] = 0;
      }
      break;
    }

    /* Use pre-rasterized cache if available and valid */
    if (BRUSHCACHE(brush)->linear_cache.rasterized_pixels &&
        BRUSHCACHE(brush)->linear_cache.rasterized_width > 0 &&
        BRUSHCACHE(brush)->linear_cache.rasterized_height > 0) {
      WORD rel_y = py - rect_y;
      if (rel_y < 0) rel_y = 0;
      if (rel_y >= BRUSHCACHE(brush)->linear_cache.rasterized_height)
        rel_y = BRUSHCACHE(brush)->linear_cache.rasterized_height - 1;

      ULONG *row = BRUSHCACHE(brush)->linear_cache.rasterized_pixels +
                   rel_y * BRUSHCACHE(brush)->linear_cache.rasterized_width;
      UWORD cache_w = BRUSHCACHE(brush)->linear_cache.rasterized_width;

      for (WORD i = 0; i < 8; i++) {
        WORD rel_x = px[i] - rect_x;
        if (rel_x < 0) rel_x = 0;
        if (rel_x >= cache_w) rel_x = cache_w - 1;

        ULONG pixel = row[rel_x];
        a[i] = ZUNE_GET_ALPHA(pixel);
        r[i] = ZUNE_GET_RED(pixel);
        g[i] = ZUNE_GET_GREEN(pixel);
        b[i] = ZUNE_GET_BLUE(pixel);
      }
      break;
    }

    /* Calculate base t for this row */
    float rel_y = py - rect_y;
    float base_t = BRUSHCACHE(brush)->linear_cache.t_start +
                   rel_y * BRUSHCACHE(brush)->linear_cache.t_step_y;

    /* Sample each pixel with incremental t */
    for (WORD i = 0; i < 8; i++) {
      float rel_x = px[i] - rect_x;
      float t = base_t + rel_x * BRUSHCACHE(brush)->linear_cache.t_step_x;

      InterpolateGradientStops(brush->data.linear.stops,
                               brush->data.linear.stop_count, t, &r[i], &g[i],
                               &b[i], &a[i]);
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_RADIAL_GRADIENT: {
    if (!BRUSHCACHE(brush)->valid) {
      for (WORD i = 0; i < 8; i++) {
        r[i] = g[i] = b[i] = a[i] = 0;
      }
      break;
    }

    /* Use pre-rasterized cache if available */
    if (BRUSHCACHE(brush)->radial_cache.rasterized_pixels &&
        BRUSHCACHE(brush)->radial_cache.rasterized_width > 0 &&
        BRUSHCACHE(brush)->radial_cache.rasterized_height > 0) {
      WORD rel_y = py - rect_y;
      if (rel_y < 0) rel_y = 0;
      if (rel_y >= BRUSHCACHE(brush)->radial_cache.rasterized_height)
        rel_y = BRUSHCACHE(brush)->radial_cache.rasterized_height - 1;

      ULONG *row = BRUSHCACHE(brush)->radial_cache.rasterized_pixels +
                   rel_y * BRUSHCACHE(brush)->radial_cache.rasterized_width;
      UWORD cache_w = BRUSHCACHE(brush)->radial_cache.rasterized_width;

      for (WORD i = 0; i < 8; i++) {
        WORD rel_x = px[i] - rect_x;
        if (rel_x < 0) rel_x = 0;
        if (rel_x >= cache_w) rel_x = cache_w - 1;

        ULONG pixel = row[rel_x];
        a[i] = ZUNE_GET_ALPHA(pixel);
        r[i] = ZUNE_GET_RED(pixel);
        g[i] = ZUNE_GET_GREEN(pixel);
        b[i] = ZUNE_GET_BLUE(pixel);
      }
      break;
    }

    /* Fallback to scalar */
    for (WORD i = 0; i < 8; i++) {
      SampleBrush(brush, rect_x, rect_y, rect_w, rect_h, px[i], py, &r[i],
                  &g[i], &b[i], &a[i]);
    }
    break;
  }

  default:
    /* Fall back to scalar sampling */
    for (WORD i = 0; i < 8; i++) {
      SampleBrush(brush, rect_x, rect_y, rect_w, rect_h, px[i], py, &r[i],
                  &g[i], &b[i], &a[i]);
    }
    break;
  }
}

/*
 * Release whatever the brush scratch still holds. The cache belongs to the
 * RenderContext, so this runs when the context goes away.
 */
void CleanupBrushCache(struct RenderContext *rctx) {
  if (!rctx)
    return;

  if (rctx->brush_cache.linear_cache.rasterized_pixels) {
    FreeVec(rctx->brush_cache.linear_cache.rasterized_pixels);
    rctx->brush_cache.linear_cache.rasterized_pixels = NULL;
  }

  if (rctx->brush_cache.radial_cache.rasterized_pixels) {
    FreeVec(rctx->brush_cache.radial_cache.rasterized_pixels);
    rctx->brush_cache.radial_cache.rasterized_pixels = NULL;
  }

  rctx->brush_cache.valid = FALSE;
}
