/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - SIMD Implementation

    This file implements SIMD-accelerated operations for anti-aliased rendering.
*/
#define DEBUG 0
#include <aros/debug.h>

#include "cybergfx_antialiasing.h"
#include "cybergfx_simd.h"
#include <math.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
#define CYBERGFX_TARGET_AVX2 __attribute__((target("avx2")))
#define CYBERGFX_CAN_BUILD_AVX2 1
#elif defined(__AVX2__)
#define CYBERGFX_TARGET_AVX2
#define CYBERGFX_CAN_BUILD_AVX2 1
#else
#define CYBERGFX_TARGET_AVX2
#define CYBERGFX_CAN_BUILD_AVX2 0
#endif
#else
#define CYBERGFX_TARGET_AVX2
#define CYBERGFX_CAN_BUILD_AVX2 0
#endif

#define CYBERGFX_AA_MIN_ALPHA_THRESHOLD 0.005f

/* SIMD capability detection */
cybergfx_simd_level cybergfx_get_simd_level(void) {
  static int initialized = 0;
  static cybergfx_simd_level cached = CYBERGFX_SIMD_NONE;

  if (initialized) {
    return cached;
  }

#if defined(__x86_64__) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
  if (__builtin_cpu_supports("avx2")) {
    cached = CYBERGFX_SIMD_AVX2;
  } else if (__builtin_cpu_supports("sse2")) {
    cached = CYBERGFX_SIMD_SSE2;
  }
#endif
#if defined(__AVX2__)
  if (cached == CYBERGFX_SIMD_NONE) {
    cached = CYBERGFX_SIMD_AVX2;
  }
#endif
#if defined(__SSE2__)
  if (cached == CYBERGFX_SIMD_NONE) {
    cached = CYBERGFX_SIMD_SSE2;
  }
#endif
#elif defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
  cached = CYBERGFX_SIMD_NEON;
#else
  cached = CYBERGFX_SIMD_NONE;
#endif

  initialized = 1;
  D(bug("Using SIMD level: %d\n", cached));
  return cached;
}

/*****************************************************************************/
/* Scalar implementations                                                     */
/*****************************************************************************/

static inline float sdf_roundrect_scalar(float x, float y, float half_w,
                                         float half_h, float r) {
  float px = fabsf(x) - (half_w - r);
  float py = fabsf(y) - (half_h - r);

  float dx = fmaxf(px, 0.0f);
  float dy = fmaxf(py, 0.0f);

  float outside;
  if (dx > 0.0f && dy > 0.0f) {
    /* In corner region - use cached distance lookup if possible */
    int idx = (int)dx;
    int idy = (int)dy;
    if (idx < CYBERGFX_CORNER_CACHE_SIZE && idy < CYBERGFX_CORNER_CACHE_SIZE && cybergfx_corner_cache.valid) {
      outside = cybergfx_corner_cache.dist[idy][idx];
    } else {
      D(bug("sdf_roundrect_scalar: cache miss idx=%d idy=%d (max=%d)\n", idx, idy, CYBERGFX_CORNER_CACHE_SIZE));
      outside = fast_sqrtf(dx * dx + dy * dy);
    }
  } else {
    /* On straight edge - no sqrt needed */
    outside = fmaxf(dx, dy);
  }

  float inside = fminf(fmaxf(px, py), 0.0f);

  return outside + inside - r;
}

static void cybergfx_sdf_roundrect_batch4_scalar(const float rel_x[4],
                                                 float rel_y, float half_w,
                                                 float half_h, float radius,
                                                 float dist_out[4]) {
  for (int i = 0; i < 4; i++) {
    dist_out[i] = sdf_roundrect_scalar(rel_x[i], rel_y, half_w, half_h, radius);
  }
}

static void cybergfx_sdf_roundrect_batch8_fallback(const float rel_x[8],
                                                   float rel_y, float half_w,
                                                   float half_h, float radius,
                                                   float dist_out[8]) {
  cybergfx_sdf_roundrect_batch4(&rel_x[0], rel_y, half_w, half_h, radius,
                                &dist_out[0]);
  cybergfx_sdf_roundrect_batch4(&rel_x[4], rel_y, half_w, half_h, radius,
                                &dist_out[4]);
}

static inline float clamp_scalar(float x, float a, float b) {
  return x < a ? a : (x > b ? b : x);
}

static inline float smoothstep_scalar(float edge0, float edge1, float x) {
  float t = clamp_scalar((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

static inline float smoothstep_half_scalar(float x) {
  return smoothstep_scalar(-0.5f, 0.5f, x);
}

/* Circle SDF and alpha helpers (scalar fallbacks) */
static inline float sdf_circle_scalar(float px, float py, float cx, float cy,
                                      float radius) {
  float dx = px + 0.5f - cx;
  float dy = py + 0.5f - cy;
  return fast_sqrtf(dx * dx + dy * dy) - radius;
}

static void cybergfx_sdf_circle_batch4_scalar(const float rel_x[4], float rel_y,
                                              float radius, float dist_out[4]) {
  for (int i = 0; i < 4; i++) {
    float dx = rel_x[i];
    float dy = rel_y;
    dist_out[i] = fast_sqrtf(dx * dx + dy * dy) - radius;
  }
}

static void cybergfx_sdf_circle_batch8_scalar(const float rel_x[8], float rel_y,
                                              float radius, float dist_out[8]) {
  for (int i = 0; i < 8; i++) {
    float dx = rel_x[i];
    float dy = rel_y;
    dist_out[i] = fast_sqrtf(dx * dx + dy * dy) - radius;
  }
}

static void cybergfx_circle_alphas_batch4_scalar(const float dist[4],
                                                 float border_width,
                                                 int hasFill, int hasBorder,
                                                 float fill_a[4],
                                                 float border_a[4]) {
  for (int i = 0; i < 4; i++) {
    float d = dist[i];
    float outer_alpha = 1.0f - smoothstep_half_scalar(d);
    float fa = 0.0f;
    float ba = 0.0f;

    if (hasFill) {
      fa = outer_alpha;
    }
    if (hasBorder && border_width > 0.0f) {
      float half_border = border_width * 0.5f;
      ba = 1.0f -
           smoothstep_scalar(-half_border - 0.5f, half_border + 0.5f, fabsf(d));
    }
    fill_a[i] = fa;
    border_a[i] = ba;
  }
}

static void cybergfx_circle_alphas_batch8_scalar(const float dist[8],
                                                 float border_width,
                                                 int hasFill, int hasBorder,
                                                 float fill_a[8],
                                                 float border_a[8]) {
  for (int i = 0; i < 8; i++) {
    float d = dist[i];
    float outer_alpha = 1.0f - smoothstep_half_scalar(d);
    float fa = 0.0f;
    float ba = 0.0f;

    if (hasFill) {
      fa = outer_alpha;
    }
    if (hasBorder && border_width > 0.0f) {
      float half_border = border_width * 0.5f;
      ba = 1.0f -
           smoothstep_scalar(-half_border - 0.5f, half_border + 0.5f, fabsf(d));
    }
    fill_a[i] = fa;
    border_a[i] = ba;
  }
}

#if defined(__SSE2__)
static inline __m128i cybergfx_tint_argb32_sse2(__m128i pixels,
                                                __m128i tint16) {
  const __m128i zero = _mm_setzero_si128();
  __m128i lo = _mm_unpacklo_epi8(pixels, zero);
  __m128i hi = _mm_unpackhi_epi8(pixels, zero);
  lo = _mm_mullo_epi16(lo, tint16);
  hi = _mm_mullo_epi16(hi, tint16);
  lo = _mm_srli_epi16(lo, 8);
  hi = _mm_srli_epi16(hi, 8);
  __m128i packed = _mm_packus_epi16(lo, hi);
#if defined(__SSSE3__)
  const __m128i shuffle =
      _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
  packed = _mm_shuffle_epi8(packed, shuffle);
#else
  __m128i b = _mm_and_si128(packed, _mm_set1_epi32(0x000000FF));
  __m128i g =
      _mm_and_si128(_mm_srli_epi32(packed, 8), _mm_set1_epi32(0x000000FF));
  __m128i r =
      _mm_and_si128(_mm_srli_epi32(packed, 16), _mm_set1_epi32(0x000000FF));
  __m128i a =
      _mm_and_si128(_mm_srli_epi32(packed, 24), _mm_set1_epi32(0x000000FF));
  packed = _mm_or_si128(_mm_slli_epi32(b, 24),
                        _mm_or_si128(_mm_slli_epi32(g, 16),
                                     _mm_or_si128(_mm_slli_epi32(r, 8), a)));
#endif
  return packed;
}
#endif

static void cybergfx_compute_alphas_batch4_scalar(
    const float dist_inner[4], const float dist_outer[4], float aa_edge_neg,
    float aa_edge_pos, int hasFill, int hasBorder, float alphaFill_out[4],
    float alphaLine_out[4]) {
  for (int i = 0; i < 4; i++) {
    float alphaOuter = 0.0f;
    float alphaInner = 0.0f;

    if (hasFill || hasBorder) {
      alphaOuter =
          1.0f - smoothstep_scalar(aa_edge_neg, aa_edge_pos, dist_outer[i]);
    }
    if (hasBorder) {
      alphaInner =
          1.0f - smoothstep_scalar(aa_edge_neg, aa_edge_pos, dist_inner[i]);
      alphaLine_out[i] = clamp_scalar(alphaOuter - alphaInner, 0.0f, 1.0f);
    } else {
      alphaLine_out[i] = 0.0f;
    }
    if (hasFill) {
      if (hasBorder) {
        float interior_cover =
            1.0f - smoothstep_scalar(0.0f, aa_edge_pos, dist_inner[i]);
        alphaFill_out[i] = clamp_scalar(interior_cover, 0.0f, 1.0f);
      } else {
        alphaFill_out[i] = alphaOuter;
      }
    } else {
      alphaFill_out[i] = 0.0f;
    }
  }
}

static void cybergfx_compute_alphas_batch8_fallback(
    const float dist_inner[8], const float dist_outer[8], float aa_edge_neg,
    float aa_edge_pos, int hasFill, int hasBorder, float alphaFill_out[8],
    float alphaLine_out[8]) {
  cybergfx_compute_alphas_batch4(&dist_inner[0], &dist_outer[0], aa_edge_neg,
                                 aa_edge_pos, hasFill, hasBorder,
                                 &alphaFill_out[0], &alphaLine_out[0]);
  cybergfx_compute_alphas_batch4(&dist_inner[4], &dist_outer[4], aa_edge_neg,
                                 aa_edge_pos, hasFill, hasBorder,
                                 &alphaFill_out[4], &alphaLine_out[4]);
}

/*****************************************************************************/
/* SSE2 implementations                                                      */
/*****************************************************************************/

#if defined(__SSE2__)
static inline __m128 cybergfx_clamp_ps(__m128 v, __m128 lo, __m128 hi) {
  return _mm_min_ps(_mm_max_ps(v, lo), hi);
}

static inline __m128 cybergfx_abs_ps(__m128 v) {
  const __m128i mask = _mm_set1_epi32(0x7FFFFFFF);
  return _mm_castsi128_ps(_mm_and_si128(_mm_castps_si128(v), mask));
}

static inline __m128 cybergfx_sdf_roundrect_sse2(__m128 x, __m128 y,
                                                 float half_w, float half_h,
                                                 float r) {
  __m128 abs_x = cybergfx_abs_ps(x);
  __m128 abs_y = cybergfx_abs_ps(y);
  __m128 w_vec = _mm_set1_ps(half_w - r);
  __m128 h_vec = _mm_set1_ps(half_h - r);

  __m128 px = _mm_sub_ps(abs_x, w_vec);
  __m128 py = _mm_sub_ps(abs_y, h_vec);
  __m128 zero = _mm_set1_ps(0.0f);

  __m128 dx = _mm_max_ps(px, zero);
  __m128 dy = _mm_max_ps(py, zero);
  __m128 outside =
      _mm_sqrt_ps(_mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy)));

  __m128 inside = _mm_min_ps(_mm_max_ps(px, py), zero);
  return _mm_sub_ps(_mm_add_ps(outside, inside), _mm_set1_ps(r));
}

static void cybergfx_sdf_roundrect_batch4_sse2(const float rel_x[4],
                                               float rel_y, float half_w,
                                               float half_h, float radius,
                                               float dist_out[4]) {
  __m128 x_vec = _mm_loadu_ps(rel_x);
  __m128 y_vec = _mm_set1_ps(rel_y);
  __m128 result =
      cybergfx_sdf_roundrect_sse2(x_vec, y_vec, half_w, half_h, radius);
  _mm_storeu_ps(dist_out, result);
}

static inline __m128 cybergfx_smoothstep_ps(__m128 edge0, __m128 edge1,
                                            __m128 x) {
  __m128 t = _mm_div_ps(_mm_sub_ps(x, edge0), _mm_sub_ps(edge1, edge0));
  t = cybergfx_clamp_ps(t, _mm_set1_ps(0.0f), _mm_set1_ps(1.0f));
  __m128 t2 = _mm_mul_ps(t, t);
  return _mm_mul_ps(
      t2, _mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_set1_ps(2.0f), t)));
}

static inline __m128 cybergfx_smoothstep_ps_half(__m128 x) {
  const __m128 edge0 = _mm_set1_ps(-0.5f);
  const __m128 edge1 = _mm_set1_ps(0.5f);
  __m128 t = _mm_div_ps(_mm_sub_ps(x, edge0), _mm_sub_ps(edge1, edge0));
  t = cybergfx_clamp_ps(t, _mm_set1_ps(0.0f), _mm_set1_ps(1.0f));
  __m128 t2 = _mm_mul_ps(t, t);
  return _mm_mul_ps(
      t2, _mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(_mm_set1_ps(2.0f), t)));
}

static inline __m128 cybergfx_sdf_circle_sse2(__m128 x, __m128 y,
                                              float radius) {
  __m128 dx2 = _mm_mul_ps(x, x);
  __m128 dy2 = _mm_mul_ps(y, y);
  __m128 sum = _mm_add_ps(dx2, dy2);
#if defined(__SSE3__)
  __m128 dist = _mm_sqrt_ps(sum);
#else
  __m128 dist = _mm_sqrt_ps(sum);
#endif
  return _mm_sub_ps(dist, _mm_set1_ps(radius));
}

static void cybergfx_sdf_circle_batch4_sse2(const float rel_x[4], float rel_y,
                                            float radius, float dist_out[4]) {
  __m128 x = _mm_loadu_ps(rel_x);
  __m128 y = _mm_set1_ps(rel_y);
  __m128 d = cybergfx_sdf_circle_sse2(x, y, radius);
  _mm_storeu_ps(dist_out, d);
}

static void cybergfx_circle_alphas_batch4_sse2(const float dist[4],
                                               float border_width, int hasFill,
                                               int hasBorder, float fill_a[4],
                                               float border_a[4]) {
  __m128 d_vec = _mm_loadu_ps(dist);
  __m128 outer_alpha =
      _mm_sub_ps(_mm_set1_ps(1.0f), cybergfx_smoothstep_ps_half(d_vec));

  __m128 fa = _mm_set1_ps(0.0f);
  __m128 ba = _mm_set1_ps(0.0f);

  if (hasFill) {
    fa = outer_alpha;
  }

  if (hasBorder && border_width > 0.0f) {
    __m128 half_border = _mm_set1_ps(border_width * 0.5f);
    __m128 border_neg = _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(-1.0f), half_border),
                                   _mm_set1_ps(0.5f));
    __m128 border_pos = _mm_add_ps(half_border, _mm_set1_ps(0.5f));
    __m128 abs_d = cybergfx_abs_ps(d_vec);
    ba = _mm_sub_ps(_mm_set1_ps(1.0f),
                    cybergfx_smoothstep_ps(border_neg, border_pos, abs_d));
  }

  _mm_storeu_ps(fill_a, fa);
  _mm_storeu_ps(border_a, ba);
}

static void cybergfx_compute_alphas_batch4_sse2(
    const float dist_inner[4], const float dist_outer[4], float aa_edge_neg,
    float aa_edge_pos, int hasFill, int hasBorder, float alphaFill_out[4],
    float alphaLine_out[4]) {
  __m128 d_inner = _mm_loadu_ps(dist_inner);
  __m128 d_outer = _mm_loadu_ps(dist_outer);
  __m128 edge_neg = _mm_set1_ps(aa_edge_neg);
  __m128 edge_pos = _mm_set1_ps(aa_edge_pos);
  __m128 one = _mm_set1_ps(1.0f);
  __m128 zero = _mm_set1_ps(0.0f);

  __m128 alphaOuter = zero;
  __m128 alphaInner = zero;

  if (hasFill || hasBorder) {
    alphaOuter =
        _mm_sub_ps(one, cybergfx_smoothstep_ps(edge_neg, edge_pos, d_outer));
  }

  if (hasBorder) {
    alphaInner =
        _mm_sub_ps(one, cybergfx_smoothstep_ps(edge_neg, edge_pos, d_inner));
    __m128 alphaLine =
        cybergfx_clamp_ps(_mm_sub_ps(alphaOuter, alphaInner), zero, one);
    _mm_storeu_ps(alphaLine_out, alphaLine);
  } else {
    _mm_storeu_ps(alphaLine_out, zero);
  }

  __m128 alphaFill = alphaOuter;
  if (hasBorder) {
    __m128 interior_cover =
        _mm_sub_ps(one, cybergfx_smoothstep_ps(zero, edge_pos, d_inner));
    alphaFill = cybergfx_clamp_ps(interior_cover, zero, one);
  }

  if (hasFill) {
    _mm_storeu_ps(alphaFill_out, alphaFill);
  } else {
    _mm_storeu_ps(alphaFill_out, zero);
  }
}
#endif /* __SSE2__ */

/*****************************************************************************/
/* AVX2 implementations (256-bit, 8-wide)                                   */
/*****************************************************************************/

#if CYBERGFX_CAN_BUILD_AVX2
static inline __m256i CYBERGFX_TARGET_AVX2
cybergfx_tint_argb32_avx2(__m256i pixels, __m256i tint16) {
  const __m256i zero = _mm256_setzero_si256();
  __m256i lo = _mm256_unpacklo_epi8(pixels, zero);
  __m256i hi = _mm256_unpackhi_epi8(pixels, zero);
  lo = _mm256_mullo_epi16(lo, tint16);
  hi = _mm256_mullo_epi16(hi, tint16);
  lo = _mm256_srli_epi16(lo, 8);
  hi = _mm256_srli_epi16(hi, 8);
  __m256i packed = _mm256_packus_epi16(lo, hi);
  const __m256i shuffle = _mm256_setr_epi8(
      3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23,
      22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28);
  return _mm256_shuffle_epi8(packed, shuffle);
}

static inline __m256 CYBERGFX_TARGET_AVX2 cybergfx_clamp_ps256(__m256 v,
                                                               __m256 lo,
                                                               __m256 hi) {
  return _mm256_min_ps(_mm256_max_ps(v, lo), hi);
}

static inline __m256 CYBERGFX_TARGET_AVX2 cybergfx_abs_ps256(__m256 v) {
  const __m256i mask = _mm256_set1_epi32(0x7FFFFFFF);
  return _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(v), mask));
}

static inline __m256 CYBERGFX_TARGET_AVX2 cybergfx_sdf_roundrect_avx2(
    __m256 x, __m256 y, float half_w, float half_h, float r) {
  __m256 abs_x = cybergfx_abs_ps256(x);
  __m256 abs_y = cybergfx_abs_ps256(y);
  __m256 w_vec = _mm256_set1_ps(half_w - r);
  __m256 h_vec = _mm256_set1_ps(half_h - r);

  __m256 px = _mm256_sub_ps(abs_x, w_vec);
  __m256 py = _mm256_sub_ps(abs_y, h_vec);
  __m256 zero = _mm256_set1_ps(0.0f);

  __m256 dx = _mm256_max_ps(px, zero);
  __m256 dy = _mm256_max_ps(py, zero);
  __m256 outside = _mm256_sqrt_ps(
      _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_mul_ps(dy, dy)));

  __m256 inside = _mm256_min_ps(_mm256_max_ps(px, py), zero);
  return _mm256_sub_ps(_mm256_add_ps(outside, inside), _mm256_set1_ps(r));
}

static void CYBERGFX_TARGET_AVX2 cybergfx_sdf_roundrect_batch8_avx2(
    const float rel_x[8], float rel_y, float half_w, float half_h, float radius,
    float dist_out[8]) {
  __m256 x_vec = _mm256_loadu_ps(rel_x);
  __m256 y_vec = _mm256_set1_ps(rel_y);
  __m256 result =
      cybergfx_sdf_roundrect_avx2(x_vec, y_vec, half_w, half_h, radius);
  _mm256_storeu_ps(dist_out, result);
}

static inline __m256 CYBERGFX_TARGET_AVX2
cybergfx_smoothstep_ps256(__m256 edge0, __m256 edge1, __m256 x) {
  __m256 t =
      _mm256_div_ps(_mm256_sub_ps(x, edge0), _mm256_sub_ps(edge1, edge0));
  t = cybergfx_clamp_ps256(t, _mm256_set1_ps(0.0f), _mm256_set1_ps(1.0f));
  __m256 t2 = _mm256_mul_ps(t, t);
  return _mm256_mul_ps(t2,
                       _mm256_sub_ps(_mm256_set1_ps(3.0f),
                                     _mm256_mul_ps(_mm256_set1_ps(2.0f), t)));
}

static void CYBERGFX_TARGET_AVX2 cybergfx_compute_alphas_batch8_avx2(
    const float dist_inner[8], const float dist_outer[8], float aa_edge_neg,
    float aa_edge_pos, int hasFill, int hasBorder, float alphaFill_out[8],
    float alphaLine_out[8]) {
  __m256 d_inner = _mm256_loadu_ps(dist_inner);
  __m256 d_outer = _mm256_loadu_ps(dist_outer);
  __m256 edge_neg = _mm256_set1_ps(aa_edge_neg);
  __m256 edge_pos = _mm256_set1_ps(aa_edge_pos);
  __m256 one = _mm256_set1_ps(1.0f);
  __m256 zero = _mm256_set1_ps(0.0f);

  __m256 alphaOuter = zero;
  __m256 alphaInner = zero;

  if (hasFill || hasBorder) {
    alphaOuter = _mm256_sub_ps(
        one, cybergfx_smoothstep_ps256(edge_neg, edge_pos, d_outer));
  }

  if (hasBorder) {
    alphaInner = _mm256_sub_ps(
        one, cybergfx_smoothstep_ps256(edge_neg, edge_pos, d_inner));
    __m256 alphaLine =
        cybergfx_clamp_ps256(_mm256_sub_ps(alphaOuter, alphaInner), zero, one);
    _mm256_storeu_ps(alphaLine_out, alphaLine);
  } else {
    _mm256_storeu_ps(alphaLine_out, zero);
  }

  __m256 alphaFill = alphaOuter;
  if (hasBorder) {
    __m256 interior_cover = _mm256_sub_ps(
        one, cybergfx_smoothstep_ps256(zero, edge_pos, d_inner));
    alphaFill = cybergfx_clamp_ps256(interior_cover, zero, one);
  }

  if (hasFill) {
    _mm256_storeu_ps(alphaFill_out, alphaFill);
  } else {
    _mm256_storeu_ps(alphaFill_out, zero);
  }
}
#endif /* CYBERGFX_CAN_BUILD_AVX2 */

/*****************************************************************************/
/* NEON implementations                                                      */
/*****************************************************************************/

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
static inline float32x4_t cybergfx_abs_f32(float32x4_t v) {
  return vreinterpretq_f32_u32(
      vandq_u32(vreinterpretq_u32_f32(v), vdupq_n_u32(0x7FFFFFFF)));
}

static inline float32x4_t cybergfx_sdf_roundrect_neon(float32x4_t x,
                                                      float32x4_t y,
                                                      float half_w,
                                                      float half_h, float r) {
  float32x4_t abs_x = cybergfx_abs_f32(x);
  float32x4_t abs_y = cybergfx_abs_f32(y);
  float32x4_t w_vec = vdupq_n_f32(half_w - r);
  float32x4_t h_vec = vdupq_n_f32(half_h - r);

  float32x4_t px = vsubq_f32(abs_x, w_vec);
  float32x4_t py = vsubq_f32(abs_y, h_vec);
  float32x4_t zero = vdupq_n_f32(0.0f);

  float32x4_t dx = vmaxq_f32(px, zero);
  float32x4_t dy = vmaxq_f32(py, zero);
  float32x4_t outside =
      vsqrtq_f32(vaddq_f32(vmulq_f32(dx, dx), vmulq_f32(dy, dy)));

  float32x4_t inside = vminq_f32(vmaxq_f32(px, py), zero);
  return vsubq_f32(vaddq_f32(outside, inside), vdupq_n_f32(r));
}

static void cybergfx_sdf_roundrect_batch4_neon(const float rel_x[4],
                                               float rel_y, float half_w,
                                               float half_h, float radius,
                                               float dist_out[4]) {
  float32x4_t x_vec = vld1q_f32(rel_x);
  float32x4_t y_vec = vdupq_n_f32(rel_y);
  float32x4_t result =
      cybergfx_sdf_roundrect_neon(x_vec, y_vec, half_w, half_h, radius);
  vst1q_f32(dist_out, result);
}

static inline float32x4_t
cybergfx_clamp_f32_alpha(float32x4_t v, float32x4_t lo, float32x4_t hi) {
  return vminq_f32(vmaxq_f32(v, lo), hi);
}

static inline float32x4_t
cybergfx_smoothstep_f32(float32x4_t edge0, float32x4_t edge1, float32x4_t x) {
  float32x4_t t = vdivq_f32(vsubq_f32(x, edge0), vsubq_f32(edge1, edge0));
  t = cybergfx_clamp_f32_alpha(t, vdupq_n_f32(0.0f), vdupq_n_f32(1.0f));
  float32x4_t t2 = vmulq_f32(t, t);
  return vmulq_f32(
      t2, vsubq_f32(vdupq_n_f32(3.0f), vmulq_f32(vdupq_n_f32(2.0f), t)));
}

static void cybergfx_compute_alphas_batch4_neon(
    const float dist_inner[4], const float dist_outer[4], float aa_edge_neg,
    float aa_edge_pos, int hasFill, int hasBorder, float alphaFill_out[4],
    float alphaLine_out[4]) {
  float32x4_t d_inner = vld1q_f32(dist_inner);
  float32x4_t d_outer = vld1q_f32(dist_outer);
  float32x4_t edge_neg = vdupq_n_f32(aa_edge_neg);
  float32x4_t edge_pos = vdupq_n_f32(aa_edge_pos);
  float32x4_t one = vdupq_n_f32(1.0f);
  float32x4_t zero = vdupq_n_f32(0.0f);

  float32x4_t alphaOuter = zero;
  float32x4_t alphaInner = zero;

  if (hasFill || hasBorder) {
    alphaOuter =
        vsubq_f32(one, cybergfx_smoothstep_f32(edge_neg, edge_pos, d_outer));
  }

  if (hasBorder) {
    alphaInner =
        vsubq_f32(one, cybergfx_smoothstep_f32(edge_neg, edge_pos, d_inner));
    float32x4_t alphaLine =
        cybergfx_clamp_f32_alpha(vsubq_f32(alphaOuter, alphaInner), zero, one);
    vst1q_f32(alphaLine_out, alphaLine);
  } else {
    vst1q_f32(alphaLine_out, zero);
  }

  float32x4_t alphaFill = alphaOuter;
  if (hasBorder) {
    float32x4_t interior_cover =
        vsubq_f32(one, cybergfx_smoothstep_f32(zero, edge_pos, d_inner));
    alphaFill = cybergfx_clamp_f32_alpha(interior_cover, zero, one);
  }

  if (hasFill) {
    vst1q_f32(alphaFill_out, alphaFill);
  } else {
    vst1q_f32(alphaFill_out, zero);
  }
}

static void cybergfx_blend_aa_pixels_batch4_neon(
    const ULONG bg_pixels[4], const float alphaFill[4],
    const float alphaLine[4], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[4],
    unsigned int *changed_mask) {
  *changed_mask = 0;

  /* Process each pixel (scalar for now) */
  for (int i = 0; i < 4; i++) {
    UBYTE bg_a, bg_r, bg_g, bg_b;
    extract_pixel_value(bg_pixels[i], &bg_a, &bg_r, &bg_g, &bg_b);

    float r = (float)bg_r;
    float g = (float)bg_g;
    float b = (float)bg_b;

    int pixel_changed = 0;

    if (hasFill && alphaFill[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
      float final_alpha = fc_alpha_scale * alphaFill[i];
      if (final_alpha > 0.0f) {
        float inv_alpha = 1.0f - final_alpha;
        r = inv_alpha * r + final_alpha * fc_r;
        g = inv_alpha * g + final_alpha * fc_g;
        b = inv_alpha * b + final_alpha * fc_b;
        pixel_changed = 1;
      }
    }

    if (hasBorder && alphaLine[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
      float final_alpha = o_alpha_scale * alphaLine[i];
      if (final_alpha > 0.0f) {
        float inv_alpha = 1.0f - final_alpha;
        r = inv_alpha * r + final_alpha * o_r;
        g = inv_alpha * g + final_alpha * o_g;
        b = inv_alpha * b + final_alpha * o_b;
        pixel_changed = 1;
      }
    }

    if (pixel_changed) {
      result_pixels[i] = make_pixel_value(bg_a, (UBYTE)r, (UBYTE)g, (UBYTE)b);
      *changed_mask |= (1 << i);
    } else {
      result_pixels[i] = bg_pixels[i];
    }
  }
}
#endif /* __ARM_NEON */

/*****************************************************************************/
/* Blending helpers                                                          */
/*****************************************************************************/

#if defined(__SSE2__)
static void cybergfx_blend_aa_pixels_batch4_sse2(
    const ULONG bg_pixels[4], const float alphaFill[4],
    const float alphaLine[4], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[4],
    unsigned int *changed_mask) {
  *changed_mask = 0;

  /* Process each pixel (scalar for now - full SIMD unpack/pack is complex) */
  for (int i = 0; i < 4; i++) {
    UBYTE bg_a, bg_r, bg_g, bg_b;
    extract_pixel_value(bg_pixels[i], &bg_a, &bg_r, &bg_g, &bg_b);

    float r = (float)bg_r;
    float g = (float)bg_g;
    float b = (float)bg_b;

    int pixel_changed = 0;

    if (hasFill && alphaFill[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
      float final_alpha = fc_alpha_scale * alphaFill[i];
      if (final_alpha > 0.0f) {
        float inv_alpha = 1.0f - final_alpha;
        r = inv_alpha * r + final_alpha * fc_r;
        g = inv_alpha * g + final_alpha * fc_g;
        b = inv_alpha * b + final_alpha * fc_b;
        pixel_changed = 1;
      }
    }

    if (hasBorder && alphaLine[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
      float final_alpha = o_alpha_scale * alphaLine[i];
      if (final_alpha > 0.0f) {
        float inv_alpha = 1.0f - final_alpha;
        r = inv_alpha * r + final_alpha * o_r;
        g = inv_alpha * g + final_alpha * o_g;
        b = inv_alpha * b + final_alpha * o_b;
        pixel_changed = 1;
      }
    }

    if (pixel_changed) {
      result_pixels[i] = make_pixel_value(bg_a, (UBYTE)r, (UBYTE)g, (UBYTE)b);
      *changed_mask |= (1 << i);
    } else {
      result_pixels[i] = bg_pixels[i];
    }
  }
}
#endif /* __SSE2__ */

static void cybergfx_blend_aa_pixels_batch4_scalar(
    const ULONG bg_pixels[4], const float alphaFill[4],
    const float alphaLine[4], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[4],
    unsigned int *changed_mask) {
  *changed_mask = 0;

  for (int i = 0; i < 4; i++) {
    UBYTE bg_a, bg_r, bg_g, bg_b;
    extract_pixel_value(bg_pixels[i], &bg_a, &bg_r, &bg_g, &bg_b);

    float r = (float)bg_r;
    float g = (float)bg_g;
    float b = (float)bg_b;

    int pixel_changed = 0;

    if (hasFill && alphaFill[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
      float final_alpha = fc_alpha_scale * alphaFill[i];
      if (final_alpha > 0.0f) {
        float inv_alpha = 1.0f - final_alpha;
        r = inv_alpha * r + final_alpha * fc_r;
        g = inv_alpha * g + final_alpha * fc_g;
        b = inv_alpha * b + final_alpha * fc_b;
        pixel_changed = 1;
      }
    }

    if (hasBorder && alphaLine[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
      float final_alpha = o_alpha_scale * alphaLine[i];
      if (final_alpha > 0.0f) {
        float inv_alpha = 1.0f - final_alpha;
        r = inv_alpha * r + final_alpha * o_r;
        g = inv_alpha * g + final_alpha * o_g;
        b = inv_alpha * b + final_alpha * o_b;
        pixel_changed = 1;
      }
    }

    if (pixel_changed) {
      result_pixels[i] = make_pixel_value(bg_a, (UBYTE)r, (UBYTE)g, (UBYTE)b);
      *changed_mask |= (1 << i);
    } else {
      result_pixels[i] = bg_pixels[i];
    }
  }
}

static void cybergfx_blend_aa_pixels_batch8_fallback(
    const ULONG bg_pixels[8], const float alphaFill[8],
    const float alphaLine[8], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[8],
    unsigned int *changed_mask) {
  unsigned int mask1 = 0, mask2 = 0;

  cybergfx_blend_aa_pixels_batch4(&bg_pixels[0], &alphaFill[0], &alphaLine[0],
                                  fc_r, fc_g, fc_b, fc_alpha_scale, o_r, o_g,
                                  o_b, o_alpha_scale, hasFill, hasBorder,
                                  &result_pixels[0], &mask1);

  cybergfx_blend_aa_pixels_batch4(&bg_pixels[4], &alphaFill[4], &alphaLine[4],
                                  fc_r, fc_g, fc_b, fc_alpha_scale, o_r, o_g,
                                  o_b, o_alpha_scale, hasFill, hasBorder,
                                  &result_pixels[4], &mask2);

  *changed_mask = mask1 | (mask2 << 4);
}

/*****************************************************************************/
/* Dispatch helpers                                                          */
/*****************************************************************************/

void cybergfx_sdf_roundrect_batch4(const float rel_x[4], float rel_y,
                                   float half_w, float half_h, float radius,
                                   float dist_out[4]) {
  static void (*impl)(const float *, float, float, float, float, float *) =
      NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if defined(__SSE2__)
    if (level == CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_sdf_roundrect_batch4_sse2;
    } else
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        if (level == CYBERGFX_SIMD_NEON) {
      impl = cybergfx_sdf_roundrect_batch4_neon;
    } else
#endif
    {
      impl = cybergfx_sdf_roundrect_batch4_scalar;
    }
  }

  impl(rel_x, rel_y, half_w, half_h, radius, dist_out);
}

void cybergfx_sdf_roundrect_batch8(const float rel_x[8], float rel_y,
                                   float half_w, float half_h, float radius,
                                   float dist_out[8]) {
  static void (*impl)(const float *, float, float, float, float, float *) =
      NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if CYBERGFX_CAN_BUILD_AVX2
    if (level == CYBERGFX_SIMD_AVX2) {
      impl = cybergfx_sdf_roundrect_batch8_avx2;
    } else
#endif
    {
      impl = cybergfx_sdf_roundrect_batch8_fallback;
    }
  }

  impl(rel_x, rel_y, half_w, half_h, radius, dist_out);
}

void cybergfx_compute_alphas_batch4(const float dist_inner[4],
                                    const float dist_outer[4],
                                    float aa_edge_neg, float aa_edge_pos,
                                    int hasFill, int hasBorder,
                                    float alphaFill_out[4],
                                    float alphaLine_out[4]) {
  static void (*impl)(const float *, const float *, float, float, int, int,
                      float *, float *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if defined(__SSE2__)
    if (level == CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_compute_alphas_batch4_sse2;
    } else
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        if (level == CYBERGFX_SIMD_NEON) {
      impl = cybergfx_compute_alphas_batch4_neon;
    } else
#endif
    {
      impl = cybergfx_compute_alphas_batch4_scalar;
    }
  }

  impl(dist_inner, dist_outer, aa_edge_neg, aa_edge_pos, hasFill, hasBorder,
       alphaFill_out, alphaLine_out);
}

void cybergfx_compute_alphas_batch8(const float dist_inner[8],
                                    const float dist_outer[8],
                                    float aa_edge_neg, float aa_edge_pos,
                                    int hasFill, int hasBorder,
                                    float alphaFill_out[8],
                                    float alphaLine_out[8]) {
  static void (*impl)(const float *, const float *, float, float, int, int,
                      float *, float *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if CYBERGFX_CAN_BUILD_AVX2
    if (level == CYBERGFX_SIMD_AVX2) {
      impl = cybergfx_compute_alphas_batch8_avx2;
    } else
#endif
    {
      impl = cybergfx_compute_alphas_batch8_fallback;
    }
  }

  impl(dist_inner, dist_outer, aa_edge_neg, aa_edge_pos, hasFill, hasBorder,
       alphaFill_out, alphaLine_out);
}

void cybergfx_blend_aa_pixels_batch4(
    const ULONG bg_pixels[4], const float alphaFill[4],
    const float alphaLine[4], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[4],
    unsigned int *changed_mask) {
  static void (*impl)(const ULONG *, const float *, const float *, UBYTE, UBYTE,
                      UBYTE, float, UBYTE, UBYTE, UBYTE, float, int, int,
                      ULONG *, unsigned int *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if defined(__SSE2__)
    if (level == CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_blend_aa_pixels_batch4_sse2;
    } else
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        if (level == CYBERGFX_SIMD_NEON) {
      impl = cybergfx_blend_aa_pixels_batch4_neon;
    } else
#endif
    {
      impl = cybergfx_blend_aa_pixels_batch4_scalar;
    }
  }

  impl(bg_pixels, alphaFill, alphaLine, fc_r, fc_g, fc_b, fc_alpha_scale, o_r,
       o_g, o_b, o_alpha_scale, hasFill, hasBorder, result_pixels,
       changed_mask);
}

void cybergfx_blend_aa_pixels_batch8(
    const ULONG bg_pixels[8], const float alphaFill[8],
    const float alphaLine[8], UBYTE fc_r, UBYTE fc_g, UBYTE fc_b,
    float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale,
    int hasFill, int hasBorder, ULONG result_pixels[8],
    unsigned int *changed_mask) {
  static void (*impl)(const ULONG *, const float *, const float *, UBYTE, UBYTE,
                      UBYTE, float, UBYTE, UBYTE, UBYTE, float, int, int,
                      ULONG *, unsigned int *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if defined(__AVX2__)
    if (level == CYBERGFX_SIMD_AVX2) {
      /* For now, AVX2 uses fallback too (full SIMD pixel packing is complex) */
      impl = cybergfx_blend_aa_pixels_batch8_fallback;
    } else
#endif
    {
      impl = cybergfx_blend_aa_pixels_batch8_fallback;
    }
  }

  impl(bg_pixels, alphaFill, alphaLine, fc_r, fc_g, fc_b, fc_alpha_scale, o_r,
       o_g, o_b, o_alpha_scale, hasFill, hasBorder, result_pixels,
       changed_mask);
}

void cybergfx_sdf_circle_batch4(const float rel_x[4], float rel_y, float radius,
                                float dist_out[4]) {
  static void (*impl)(const float *, float, float, float *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();
#if defined(__SSE2__)
    if (level == CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_sdf_circle_batch4_sse2;
    } else
#endif
    {
      impl = cybergfx_sdf_circle_batch4_scalar;
    }
  }

  impl(rel_x, rel_y, radius, dist_out);
}

void cybergfx_sdf_circle_batch8(const float rel_x[8], float rel_y, float radius,
                                float dist_out[8]) {
  static void (*impl)(const float *, float, float, float *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();
#if CYBERGFX_CAN_BUILD_AVX2
    if (level == CYBERGFX_SIMD_AVX2) {
      /* Use two batch4 SSE2/scalar calls if AVX2 not available for circle SDF
       */
      impl = NULL;
    } else
#endif
    {
      impl = NULL;
    }
  }

  if (impl) {
    impl(rel_x, rel_y, radius, dist_out);
  } else {
    cybergfx_sdf_circle_batch4(rel_x, rel_y, radius, dist_out);
    cybergfx_sdf_circle_batch4(&rel_x[4], rel_y, radius, &dist_out[4]);
  }
}

void cybergfx_circle_alphas_batch4(const float dist[4], float border_width,
                                   int hasFill, int hasBorder, float fill_a[4],
                                   float border_a[4]) {
  static void (*impl)(const float *, float, int, int, float *, float *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();
#if defined(__SSE2__)
    if (level == CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_circle_alphas_batch4_sse2;
    } else
#endif
    {
      impl = cybergfx_circle_alphas_batch4_scalar;
    }
  }

  impl(dist, border_width, hasFill, hasBorder, fill_a, border_a);
}

void cybergfx_circle_alphas_batch8(const float dist[8], float border_width,
                                   int hasFill, int hasBorder, float fill_a[8],
                                   float border_a[8]) {
  static void (*impl)(const float *, float, int, int, float *, float *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();
#if CYBERGFX_CAN_BUILD_AVX2
    if (level == CYBERGFX_SIMD_AVX2) {
      impl = NULL;
    } else
#endif
    {
      impl = NULL;
    }
  }

  if (impl) {
    impl(dist, border_width, hasFill, hasBorder, fill_a, border_a);
  } else {
    cybergfx_circle_alphas_batch4(dist, border_width, hasFill, hasBorder,
                                  fill_a, border_a);
    cybergfx_circle_alphas_batch4(&dist[4], border_width, hasFill, hasBorder,
                                  &fill_a[4], &border_a[4]);
  }
}

/*****************************************************************************/
/* Texture SIMD helpers                                                      */
/*****************************************************************************/

static void
cybergfx_blit_argb32_unity_scalar(const UBYTE *src, ULONG *dst, int width,
                                  const struct InternalColor *tint) {
  if (width <= 0) {
    return;
  }

  if (!tint) {
    memcpy(dst, src, (size_t)width * sizeof(ULONG));
    return;
  }

  UBYTE tr = tint->r, tg = tint->g, tb = tint->b, ta = tint->a;

  for (int i = 0; i < width; i++) {
    ULONG pixel = *(const ULONG *)(src + i * 4);
    UBYTE a = (pixel >> 24) & 0xFF;
    UBYTE r = (pixel >> 16) & 0xFF;
    UBYTE g = (pixel >> 8) & 0xFF;
    UBYTE b = pixel & 0xFF;

    a = (a * ta) >> 8;
    r = (r * tr) >> 8;
    g = (g * tg) >> 8;
    b = (b * tb) >> 8;

#if AROS_BIG_ENDIAN
    dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
#else
    dst[i] = (b << 24) | (g << 16) | (r << 8) | a;
#endif
  }
}

#if defined(__SSE2__)
static void cybergfx_blit_argb32_unity_sse2(const UBYTE *src, ULONG *dst,
                                            int width,
                                            const struct InternalColor *tint) {
  if (width <= 0) {
    return;
  }

  if (!tint) {
    memcpy(dst, src, (size_t)width * sizeof(ULONG));
    return;
  }

  __m128i tint16_sse = _mm_setr_epi16(tint->a, tint->r, tint->g, tint->b,
                                      tint->a, tint->r, tint->g, tint->b);

  int i = 0;
  for (; i + 3 < width; i += 4) {
    __m128i pixels = _mm_loadu_si128((const __m128i *)(src + i * 4));
    __m128i out = cybergfx_tint_argb32_sse2(pixels, tint16_sse);
    _mm_storeu_si128((__m128i *)(dst + i), out);
  }

  for (; i < width; i++) {
    cybergfx_blit_argb32_unity_scalar(src + i * 4, dst + i, 1, tint);
  }
}
#endif

#if CYBERGFX_CAN_BUILD_AVX2
static void CYBERGFX_TARGET_AVX2 cybergfx_blit_argb32_unity_avx2(
    const UBYTE *src, ULONG *dst, int width, const struct InternalColor *tint) {
  if (width <= 0) {
    return;
  }

  if (!tint) {
    memcpy(dst, src, (size_t)width * sizeof(ULONG));
    return;
  }

  __m256i tint16_avx = _mm256_setr_epi16(
      tint->a, tint->r, tint->g, tint->b, tint->a, tint->r, tint->g, tint->b,
      tint->a, tint->r, tint->g, tint->b, tint->a, tint->r, tint->g, tint->b);

  int i = 0;
  for (; i + 7 < width; i += 8) {
    __m256i pixels = _mm256_loadu_si256((const __m256i *)(src + i * 4));
    __m256i out = cybergfx_tint_argb32_avx2(pixels, tint16_avx);
    _mm256_storeu_si256((__m256i *)(dst + i), out);
  }

  for (; i < width; i++) {
    cybergfx_blit_argb32_unity_scalar(src + i * 4, dst + i, 1, tint);
  }
}
#endif

void cybergfx_blit_argb32_unity(const UBYTE *src, ULONG *dst, int width,
                                const struct InternalColor *tint) {
  static void (*impl)(const UBYTE *, ULONG *, int,
                      const struct InternalColor *) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();
#if CYBERGFX_CAN_BUILD_AVX2
    if (level == CYBERGFX_SIMD_AVX2) {
      impl = cybergfx_blit_argb32_unity_avx2;
    } else
#endif
#if defined(__SSE2__)
        if (level == CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_blit_argb32_unity_sse2;
    } else
#endif
    {
      impl = cybergfx_blit_argb32_unity_scalar;
    }
  }

  impl(src, dst, width, tint);
}

/*****************************************************************************/
/* ARGB32 Packing                                                            */
/*****************************************************************************/

/* Scalar implementation */
static void cybergfx_make_argb32_batch4_scalar(const UBYTE a[4],
                                               const UBYTE r[4],
                                               const UBYTE g[4],
                                               const UBYTE b[4],
                                               ULONG pixels_out[4]) {
  for (int i = 0; i < 4; i++) {
    pixels_out[i] = (((ULONG)a[i]) << 24) | (((ULONG)r[i]) << 16) |
                    (((ULONG)g[i]) << 8) | ((ULONG)b[i]);
  }
}

#if defined(__SSE2__)
/* SSE2 implementation - pack 4 ARGB32 pixels using integer SIMD */
static void cybergfx_make_argb32_batch4_sse2(const UBYTE a[4], const UBYTE r[4],
                                             const UBYTE g[4], const UBYTE b[4],
                                             ULONG pixels_out[4]) {
  /* Load bytes into 128-bit registers (only lower 32 bits used) */
  __m128i va =
      _mm_cvtsi32_si128(*(const int *)a); /* a[0-3] in lowest 4 bytes */
  __m128i vr = _mm_cvtsi32_si128(*(const int *)r);
  __m128i vg = _mm_cvtsi32_si128(*(const int *)g);
  __m128i vb = _mm_cvtsi32_si128(*(const int *)b);

  /* Unpack bytes to 16-bit integers (zero extend) */
  va = _mm_unpacklo_epi8(
      va, _mm_setzero_si128()); /* a[0] a[1] a[2] a[3] as 16-bit */
  vr = _mm_unpacklo_epi8(vr, _mm_setzero_si128());
  vg = _mm_unpacklo_epi8(vg, _mm_setzero_si128());
  vb = _mm_unpacklo_epi8(vb, _mm_setzero_si128());

  /* Unpack to 32-bit integers */
  va = _mm_unpacklo_epi16(
      va, _mm_setzero_si128()); /* a[0] a[1] a[2] a[3] as 32-bit */
  vr = _mm_unpacklo_epi16(vr, _mm_setzero_si128());
  vg = _mm_unpacklo_epi16(vg, _mm_setzero_si128());
  vb = _mm_unpacklo_epi16(vb, _mm_setzero_si128());

  /* Shift to correct positions: A<<24 | R<<16 | G<<8 | B */
  va = _mm_slli_epi32(va, 24);
  vr = _mm_slli_epi32(vr, 16);
  vg = _mm_slli_epi32(vg, 8);
  /* b stays at position 0 */

  /* Combine all channels with OR */
  __m128i result = _mm_or_si128(va, vr);
  result = _mm_or_si128(result, vg);
  result = _mm_or_si128(result, vb);

  /* Store result */
  _mm_storeu_si128((__m128i *)pixels_out, result);
}
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
/* NEON implementation - pack 4 ARGB32 pixels using integer SIMD */
static void cybergfx_make_argb32_batch4_neon(const UBYTE a[4], const UBYTE r[4],
                                             const UBYTE g[4], const UBYTE b[4],
                                             ULONG pixels_out[4]) {
  /* Load bytes into 8x8 vectors (only lower 4 bytes used) */
  uint8x8_t va = vld1_u8(a); /* Load a[0-7], but only use first 4 */
  uint8x8_t vr = vld1_u8(r);
  uint8x8_t vg = vld1_u8(g);
  uint8x8_t vb = vld1_u8(b);

  /* Widen to 16-bit */
  uint16x4_t va16 = vget_low_u16(vmovl_u8(va));
  uint16x4_t vr16 = vget_low_u16(vmovl_u8(vr));
  uint16x4_t vg16 = vget_low_u16(vmovl_u8(vg));
  uint16x4_t vb16 = vget_low_u16(vmovl_u8(vb));

  /* Widen to 32-bit */
  uint32x4_t va32 = vmovl_u16(va16);
  uint32x4_t vr32 = vmovl_u16(vr16);
  uint32x4_t vg32 = vmovl_u16(vg16);
  uint32x4_t vb32 = vmovl_u16(vb16);

  /* Shift to correct positions */
  va32 = vshlq_n_u32(va32, 24);
  vr32 = vshlq_n_u32(vr32, 16);
  vg32 = vshlq_n_u32(vg32, 8);

  /* Combine all channels with OR */
  uint32x4_t result = vorrq_u32(va32, vr32);
  result = vorrq_u32(result, vg32);
  result = vorrq_u32(result, vb32);

  /* Store result */
  vst1q_u32(pixels_out, result);
}
#endif

/* Dispatcher function */
void cybergfx_make_argb32_batch4(const UBYTE a[4], const UBYTE r[4],
                                 const UBYTE g[4], const UBYTE b[4],
                                 ULONG pixels_out[4]) {
  static void (*impl)(const UBYTE[4], const UBYTE[4], const UBYTE[4],
                      const UBYTE[4], ULONG[4]) = NULL;

  if (!impl) {
    cybergfx_simd_level level = cybergfx_get_simd_level();

#if defined(__SSE2__)
    if (level >= CYBERGFX_SIMD_SSE2) {
      impl = cybergfx_make_argb32_batch4_sse2;
    } else
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        if (level == CYBERGFX_SIMD_NEON) {
      impl = cybergfx_make_argb32_batch4_neon;
    } else
#endif
    {
      impl = cybergfx_make_argb32_batch4_scalar;
    }
  }

  impl(a, r, g, b, pixels_out);
}
