#ifndef CYBERGFX_ANTIALIASING_H
#define CYBERGFX_ANTIALIASING_H

#include "../../zunegfx_intern.h"
#include "cybergfx_backend.h"
#include "cybergfx_pixel_format.h"
#include <exec/types.h>
#include <math.h>
#define DEBUG 0
#include <aros/debug.h>

#define CYBERGFX_AA_SMOOTHNESS ((float)ZUNE_AA_HALF_BAND)
#define CYBERGFX_AA_MIN_ALPHA_THRESHOLD 0.005f
extern float cybergfx_aa_smoothness;

/* Fast inverse square root (Quake III algorithm) */
static inline float fast_invsqrt(float x) {
    float xhalf = 0.5f * x;
    LONG i = *(LONG *)&x;
    i = 0x5f375a86 - (i >> 1);
    x = *(float *)&i;
    x = x * (1.5f - xhalf * x * x); /* One Newton-Raphson iteration */
    return x;
}

/* Fast square root using inverse sqrt */
static inline float fast_sqrtf(float x) {
    if (x <= 0.0f)
        return 0.0f;
    return x * fast_invsqrt(x);
}

/* Legacy aliases - use pack_argb32/unpack_argb32 from cybergfx_pixel_format.h */
#define extract_pixel_value(pixel, a, r, g, b) unpack_argb32(pixel, a, r, g, b)
#define make_pixel_value(a, r, g, b) pack_argb32(a, r, g, b)

static inline float rfpart(float input) { return 1.0f - (input - floorf(input)); }

static inline float fpart(float input) { return input - floorf(input); }

static inline LONG cybergfx_fast_ftoi(float f) { return (LONG)(f + 0.5f); }

static inline void blend_over(UBYTE *r, UBYTE *g, UBYTE *b, UBYTE cr, UBYTE cg, UBYTE cb, float a) {
    /* Fixed-point blending: alpha scaled to 0-256 for efficient integer math */
    UWORD alpha = (UWORD)(a * 256.0f + 0.5f);
    UWORD inv_alpha = 256 - alpha;
    *r = (UBYTE)((inv_alpha * (*r) + alpha * cr) >> 8);
    *g = (UBYTE)((inv_alpha * (*g) + alpha * cg) >> 8);
    *b = (UBYTE)((inv_alpha * (*b) + alpha * cb) >> 8);
}

static inline float clamp(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

static inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* Structure to hold AA rectangle computation parameters */
typedef struct {
    float halfLine;
    float max_radius;
    float center_x;
    float center_y;
    float half_w;
    float half_h;
    WORD min_x;
    WORD min_y;
    WORD max_x;
    WORD max_y;
    float aa_edge_neg;
    float aa_edge_pos;
} cybergfx_aa_rect_params;

/* Compute common parameters for AA rectangle rendering */
static inline void cybergfx_compute_aa_rect_params(UWORD x, UWORD y, UWORD width, UWORD height, UBYTE radius, float lineWidth, UWORD board_width,
                                                   UWORD board_height, cybergfx_aa_rect_params *params) {

    params->halfLine = lineWidth * 0.5f;
    params->max_radius = fminf(radius, fminf(width * 0.5f, height * 0.5f));

    /* Calculate bounding box with padding for antialiasing */
    float influence_radius = lineWidth + cybergfx_aa_smoothness + params->max_radius;
    params->min_x = fmaxf(0, cybergfx_fast_ftoi(x - influence_radius));
    params->min_y = fmaxf(0, cybergfx_fast_ftoi(y - influence_radius));
    params->max_x = fminf(board_width - 1, cybergfx_fast_ftoi(x + width + influence_radius));
    params->max_y = fminf(board_height - 1, cybergfx_fast_ftoi(y + height + influence_radius));

    params->center_x = x + width * 0.5f;
    params->center_y = y + height * 0.5f;
    params->half_w = width * 0.5f;
    params->half_h = height * 0.5f;

    params->aa_edge_neg = -cybergfx_aa_smoothness;
    params->aa_edge_pos = cybergfx_aa_smoothness;
}

/*
 * The fill runs under the border. Composited first, with the border blended
 * over it, a plain fill alpha would leak fill colour into the antialiased
 * outer edge. Rescale it so the pair gives exact coverage. line[] is the ring
 * coverage, border_a the border colour's alpha (0..1).
 */
static inline void cybergfx_fill_under_border(float *fill, const float *line, WORD n, float border_a) {
    for (WORD i = 0; i < n; i++) {
        float ring = line[i] * border_a;
        float rest = 1.0f - ring;
        fill[i] = rest > 1e-4f ? clamp((fill[i] - ring) / rest, 0.0f, 1.0f) : 0.0f;
    }
}

#endif
