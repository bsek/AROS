/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Brush Sampling Implementation
*/

#include "cybergfx_brush_sampler.h"
#include "clib/exec_protos.h"
#include "cybergfx_antialiasing.h"
#include <math.h>
#include <aros/debug.h>
#include <string.h>
#include <clib/alib_protos.h>

void PrepareBrushForRendering(struct RenderPort *rp, struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                              UWORD rect_w, UWORD rect_h) {
  if (!brush)
    return;

  brush->internal.valid = TRUE;

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
      brush->internal.linear_cache.t_step_x = dx / length_sq;
      brush->internal.linear_cache.t_step_y = dy / length_sq;

      /* Calculate t value at rectangle origin */
      float origin_dx = rect_x - start_x;
      float origin_dy = rect_y - start_y;
      brush->internal.linear_cache.t_start =
          (origin_dx * dx + origin_dy * dy) / length_sq;
    } else {
      /* Degenerate gradient - zero length */
      brush->internal.linear_cache.t_step_x = 0.0f;
      brush->internal.linear_cache.t_step_y = 0.0f;
      brush->internal.linear_cache.t_start = 0.0f;
    }

    /* Pre-rasterize gradient if dimensions are reasonable (max 512KB) */
    ULONG pixel_count = (ULONG)rect_w * rect_h;
    if (pixel_count > 0 && pixel_count <= 131072 &&
        brush->data.linear.stops && brush->data.linear.stop_count > 0) { /* 131072 pixels = 512KB */
      /* Free old cache - we always regenerate since gradient params may have changed */
      if (brush->internal.linear_cache.rasterized_pixels) {
        FreeVec(brush->internal.linear_cache.rasterized_pixels);
        brush->internal.linear_cache.rasterized_pixels = NULL;
      }

      /* Allocate new cache */
      ULONG *pixels = AllocVec(pixel_count * sizeof(ULONG), MEMF_PUBLIC);
      if (pixels) {
        ULONG *dst = pixels;
        float row_t = brush->internal.linear_cache.t_start;

        for (UWORD y = 0; y < rect_h; y++) {
          float t = row_t;
          for (UWORD x = 0; x < rect_w; x++) {
            UBYTE r, g, b, a;
            InterpolateGradientStops(brush->data.linear.stops,
                                     brush->data.linear.stop_count, t,
                                     &r, &g, &b, &a);
            *dst++ = ZUNE_COLOR_ARGB32(a, r, g, b);
            t += brush->internal.linear_cache.t_step_x;
          }
          row_t += brush->internal.linear_cache.t_step_y;
        }

        brush->internal.linear_cache.rasterized_pixels = pixels;
        brush->internal.linear_cache.rasterized_width = rect_w;
        brush->internal.linear_cache.rasterized_height = rect_h;
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
      brush->internal.valid = FALSE;
      break;
    }

    /* Cache direct pointer to pixel data */
    brush->internal.texture_cache.pixels = (ULONG *)tex->pixel_data;
    brush->internal.texture_cache.pitch_pixels = tex->pitch / 4;

    /* Cache source rectangle */
    struct ZuneRect src = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                              ? brush->data.texture.source
                              : brush->data.datatype.source;
    brush->internal.texture_cache.src_x = src.x;
    brush->internal.texture_cache.src_y = src.y;
    brush->internal.texture_cache.src_w = src.width ? src.width : tex->width;
    brush->internal.texture_cache.src_h = src.height ? src.height : tex->height;
    break;
  }

  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN:
    D(bug("PrepareBrushForRendering: Processing brush type %s\n",
          brush->type == ZUNE_BRUSH_TYPE_SOLID ? "SOLID" : "PEN"));

    /* Allocate memory for internal color if not already allocated */
    if (!brush->internal.color) {
      brush->internal.color = AllocMem(sizeof(struct InternalColor), MEMF_PUBLIC | MEMF_CLEAR);
      if (!brush->internal.color) {
        brush->internal.valid = FALSE;
        break;
      }
      D(bug("PrepareBrushForRendering: Memory allocated at %p\n", brush->internal.color));
    }

    D(bug("PrepareBrushForRendering: Calling ZuneBrushToInternalColor with rp=%p, brush=%p, color=%p\n", rp, brush, brush->internal.color));

    if (!ZuneBrushToInternalColor(rp, brush, brush->internal.color)) {
      brush->internal.valid = FALSE;
    }
    break;

  case ZUNE_BRUSH_TYPE_PATTERN: {
    /* Convert pens to ARGB32 colors */
    if (!brush->data.pattern.colormap) {
      D(bug("PrepareBrushForRendering: PATTERN - no colormap!\n"));
      brush->internal.valid = FALSE;
      break;
    }

    ULONG rgb[3];

    /* Get foreground color */
    GetRGB32(brush->data.pattern.colormap, brush->data.pattern.fg_pen, 1, rgb);
    brush->internal.pattern_cache.fg_color = ZUNE_COLOR_ARGB32(0xFF,
                                                                 rgb[0] >> 24,
                                                                 rgb[1] >> 24,
                                                                 rgb[2] >> 24);
    D(bug("PrepareBrushForRendering: PATTERN fg_pen=%ld -> RGB32=(%08lx,%08lx,%08lx) -> ARGB=%08lx\n",
          brush->data.pattern.fg_pen, rgb[0], rgb[1], rgb[2], brush->internal.pattern_cache.fg_color));

    /* Get background color */
    GetRGB32(brush->data.pattern.colormap, brush->data.pattern.bg_pen, 1, rgb);
    brush->internal.pattern_cache.bg_color = ZUNE_COLOR_ARGB32(0xFF,
                                                                 rgb[0] >> 24,
                                                                 rgb[1] >> 24,
                                                                 rgb[2] >> 24);
    D(bug("PrepareBrushForRendering: PATTERN bg_pen=%ld -> RGB32=(%08lx,%08lx,%08lx) -> ARGB=%08lx\n",
          brush->data.pattern.bg_pen, rgb[0], rgb[1], rgb[2], brush->internal.pattern_cache.bg_color));
    break;
  }

  default:
    brush->internal.valid = FALSE;
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
  int i;
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
static inline int ApplyTextureWrap(int coord, int size,
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
    int doubled = size * 2;
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
                 UWORD rect_w, UWORD rect_h, int px, int py, UBYTE *r, UBYTE *g,
                 UBYTE *b, UBYTE *a) {

  if (!brush) {
    *r = *g = *b = *a = 0;
    return;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN: {
    /* Use stored ARGB components directly - they're already in correct format */
    *a = brush->internal.color->a;
    *r = brush->internal.color->r;
    *g = brush->internal.color->g;
    *b = brush->internal.color->b;
    break;
  }

  case ZUNE_BRUSH_TYPE_TEXTURE:
  case ZUNE_BRUSH_TYPE_DATATYPE: {
    if (!brush->internal.valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Calculate relative position in rectangle */
    int rel_x = px - rect_x;
    int rel_y = py - rect_y;

    /* Apply wrapping/clamping */
    int tex_x = ApplyTextureWrap(rel_x, brush->internal.texture_cache.src_w,
                                 brush->data.texture.wrap_u);
    int tex_y = ApplyTextureWrap(rel_y, brush->internal.texture_cache.src_h,
                                 brush->data.texture.wrap_v);

    /* Add source rectangle offset */
    tex_x += brush->internal.texture_cache.src_x;
    tex_y += brush->internal.texture_cache.src_y;

    /* Sample pixel from texture */
    ULONG pixel =
        brush->internal.texture_cache
            .pixels[tex_y * brush->internal.texture_cache.pitch_pixels + tex_x];

    *a = ZUNE_GET_ALPHA(pixel);
    *r = ZUNE_GET_RED(pixel);
    *g = ZUNE_GET_GREEN(pixel);
    *b = ZUNE_GET_BLUE(pixel);
    break;
  }

  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    if (!brush->internal.valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Use pre-rasterized cache if available and valid */
    if (brush->internal.linear_cache.rasterized_pixels &&
        brush->internal.linear_cache.rasterized_width > 0 &&
        brush->internal.linear_cache.rasterized_height > 0) {
      int rel_x = px - rect_x;
      int rel_y = py - rect_y;

      /* Clamp to valid range */
      if (rel_x < 0) rel_x = 0;
      if (rel_y < 0) rel_y = 0;
      if (rel_x >= brush->internal.linear_cache.rasterized_width)
        rel_x = brush->internal.linear_cache.rasterized_width - 1;
      if (rel_y >= brush->internal.linear_cache.rasterized_height)
        rel_y = brush->internal.linear_cache.rasterized_height - 1;

      ULONG pixel = brush->internal.linear_cache.rasterized_pixels[
          rel_y * brush->internal.linear_cache.rasterized_width + rel_x];
      *a = ZUNE_GET_ALPHA(pixel);
      *r = ZUNE_GET_RED(pixel);
      *g = ZUNE_GET_GREEN(pixel);
      *b = ZUNE_GET_BLUE(pixel);
      break;
    }

    /* Calculate gradient t using cached increments */
    float rel_x = px - rect_x;
    float rel_y = py - rect_y;
    float t = brush->internal.linear_cache.t_start +
              rel_x * brush->internal.linear_cache.t_step_x +
              rel_y * brush->internal.linear_cache.t_step_y;

    /* Interpolate between gradient stops */
    InterpolateGradientStops(brush->data.linear.stops,
                             brush->data.linear.stop_count, t, r, g, b, a);
    break;
  }

  case ZUNE_BRUSH_TYPE_PATTERN: {
    if (!brush->internal.valid) {
      *r = *g = *b = *a = 0;
      break;
    }

    /* Calculate relative position in rectangle */
    int rel_x = px - rect_x;
    int rel_y = py - rect_y;

    /* Pattern is 16x2, repeating */
    int pat_x = rel_x % 16;
    int pat_y = rel_y % 2;
    if (pat_x < 0) pat_x += 16;
    if (pat_y < 0) pat_y += 2;

    /* Get pattern bit (bit 15 is leftmost) */
    UWORD row_bits = brush->data.pattern.pattern[pat_y];
    BOOL is_fg = (row_bits & (1U << (15 - pat_x))) != 0;

    /* Select color */
    ULONG color = is_fg ? brush->internal.pattern_cache.fg_color
                        : brush->internal.pattern_cache.bg_color;

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
                       UWORD rect_w, UWORD rect_h, const int px[4], int py,
                       UBYTE r[4], UBYTE g[4], UBYTE b[4], UBYTE a[4]) {

  if (!brush) {
    for (int i = 0; i < 4; i++) {
      r[i] = g[i] = b[i] = a[i] = 0;
    }
    return;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN: {
    /* All pixels get same color */
    ULONG color = brush->internal.color->original_pixel;
    UBYTE sa = ZUNE_GET_ALPHA(color);
    UBYTE sr = ZUNE_GET_RED(color);
    UBYTE sg = ZUNE_GET_GREEN(color);
    UBYTE sb = ZUNE_GET_BLUE(color);

    for (int i = 0; i < 4; i++) {
      a[i] = sa;
      r[i] = sr;
      g[i] = sg;
      b[i] = sb;
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    if (!brush->internal.valid) {
      for (int i = 0; i < 4; i++) {
        r[i] = g[i] = b[i] = a[i] = 0;
      }
      break;
    }

    /* Use pre-rasterized cache if available and valid */
    if (brush->internal.linear_cache.rasterized_pixels &&
        brush->internal.linear_cache.rasterized_width > 0 &&
        brush->internal.linear_cache.rasterized_height > 0) {
      int rel_y = py - rect_y;
      if (rel_y < 0) rel_y = 0;
      if (rel_y >= brush->internal.linear_cache.rasterized_height)
        rel_y = brush->internal.linear_cache.rasterized_height - 1;

      ULONG *row = brush->internal.linear_cache.rasterized_pixels +
                   rel_y * brush->internal.linear_cache.rasterized_width;
      UWORD cache_w = brush->internal.linear_cache.rasterized_width;

      for (int i = 0; i < 4; i++) {
        int rel_x = px[i] - rect_x;
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
    float base_t = brush->internal.linear_cache.t_start +
                   rel_y * brush->internal.linear_cache.t_step_y;

    /* Sample each pixel with incremental t */
    for (int i = 0; i < 4; i++) {
      float rel_x = px[i] - rect_x;
      float t = base_t + rel_x * brush->internal.linear_cache.t_step_x;

      InterpolateGradientStops(brush->data.linear.stops,
                               brush->data.linear.stop_count, t, &r[i], &g[i],
                               &b[i], &a[i]);
    }
    break;
  }

  default:
    /* Fall back to scalar sampling */
    for (int i = 0; i < 4; i++) {
      SampleBrush(brush, rect_x, rect_y, rect_w, rect_h, px[i], py, &r[i],
                  &g[i], &b[i], &a[i]);
    }
    break;
  }
}

/*****************************************************************************/

void CleanupBrushInternalState(struct ZuneBrush *brush) {
  if (!brush) {
    return;
  }

  /* Free allocated internal color memory */
  if (brush->internal.color) {
    FreeMem(brush->internal.color, sizeof(struct InternalColor));
    brush->internal.color = NULL;
  }

  /* Free pre-rasterized gradient cache */
  if (brush->internal.linear_cache.rasterized_pixels) {
    FreeVec(brush->internal.linear_cache.rasterized_pixels);
    brush->internal.linear_cache.rasterized_pixels = NULL;
    brush->internal.linear_cache.rasterized_width = 0;
    brush->internal.linear_cache.rasterized_height = 0;
  }

  /* Mark internal cache as invalid */
  brush->internal.valid = FALSE;
}

/*****************************************************************************/

void SampleBrushBatch8(struct ZuneBrush *brush, WORD rect_x, WORD rect_y,
                       UWORD rect_w, UWORD rect_h, const int px[8], int py,
                       UBYTE r[8], UBYTE g[8], UBYTE b[8], UBYTE a[8]) {

  if (!brush) {
    for (int i = 0; i < 8; i++) {
      r[i] = g[i] = b[i] = a[i] = 0;
    }
    return;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
  case ZUNE_BRUSH_TYPE_PEN: {
    /* All pixels get same color */
    ULONG color = brush->internal.color->original_pixel;
    UBYTE sa = ZUNE_GET_ALPHA(color);
    UBYTE sr = ZUNE_GET_RED(color);
    UBYTE sg = ZUNE_GET_GREEN(color);
    UBYTE sb = ZUNE_GET_BLUE(color);

    for (int i = 0; i < 8; i++) {
      a[i] = sa;
      r[i] = sr;
      g[i] = sg;
      b[i] = sb;
    }
    break;
  }

  case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
    if (!brush->internal.valid) {
      for (int i = 0; i < 8; i++) {
        r[i] = g[i] = b[i] = a[i] = 0;
      }
      break;
    }

    /* Use pre-rasterized cache if available and valid */
    if (brush->internal.linear_cache.rasterized_pixels &&
        brush->internal.linear_cache.rasterized_width > 0 &&
        brush->internal.linear_cache.rasterized_height > 0) {
      int rel_y = py - rect_y;
      if (rel_y < 0) rel_y = 0;
      if (rel_y >= brush->internal.linear_cache.rasterized_height)
        rel_y = brush->internal.linear_cache.rasterized_height - 1;

      ULONG *row = brush->internal.linear_cache.rasterized_pixels +
                   rel_y * brush->internal.linear_cache.rasterized_width;
      UWORD cache_w = brush->internal.linear_cache.rasterized_width;

      for (int i = 0; i < 8; i++) {
        int rel_x = px[i] - rect_x;
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
    float base_t = brush->internal.linear_cache.t_start +
                   rel_y * brush->internal.linear_cache.t_step_y;

    /* Sample each pixel with incremental t */
    for (int i = 0; i < 8; i++) {
      float rel_x = px[i] - rect_x;
      float t = base_t + rel_x * brush->internal.linear_cache.t_step_x;

      InterpolateGradientStops(brush->data.linear.stops,
                               brush->data.linear.stop_count, t, &r[i], &g[i],
                               &b[i], &a[i]);
    }
    break;
  }

  default:
    /* Fall back to scalar sampling */
    for (int i = 0; i < 8; i++) {
      SampleBrush(brush, rect_x, rect_y, rect_w, rect_h, px[i], py, &r[i],
                  &g[i], &b[i], &a[i]);
    }
    break;
  }
}
