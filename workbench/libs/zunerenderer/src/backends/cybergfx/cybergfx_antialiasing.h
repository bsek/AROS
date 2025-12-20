#ifndef CYBERGFX_ANTIALIASING_H
#define CYBERGFX_ANTIALIASING_H

#include "../../zunerenderer_intern.h"
#include "cybergfx_backend.h"
#include "cybergfx_pixel_format.h"
#include <exec/types.h>
#include <math.h>
#define DEBUG 0
#include <aros/debug.h>

#define CYBERGFX_AA_SMOOTHNESS 1.0f /* Sharper edges for UI elements */
#define CYBERGFX_AA_MIN_ALPHA_THRESHOLD 0.005f
extern int cybergfx_aa_quality;
extern float cybergfx_aa_smoothness;

/*
 * Corner distance cache for common radii.
 * Pre-computed sqrt(dx^2 + dy^2) for corner pixels.
 * Max radius 15, max offset 15+AA_padding = ~18 pixels from corner center.
 */
#define CYBERGFX_CORNER_CACHE_MAX_RADIUS 15
#define CYBERGFX_CORNER_CACHE_SIZE 40 /* radius + AA padding */

typedef struct {
    float dist[CYBERGFX_CORNER_CACHE_SIZE][CYBERGFX_CORNER_CACHE_SIZE];
    BOOL valid;
} CybergfxCornerCache;

extern CybergfxCornerCache cybergfx_corner_cache;

/* Fast inverse square root (Quake III algorithm) */
static inline float fast_invsqrt(float x) {
    float xhalf = 0.5f * x;
    int i = *(int *)&x;
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

/* Initialize corner distance cache */
static inline void cybergfx_init_corner_cache(void) {
    if (cybergfx_corner_cache.valid)
        return;

    for (int dy = 0; dy < CYBERGFX_CORNER_CACHE_SIZE; dy++) {
        for (int dx = 0; dx < CYBERGFX_CORNER_CACHE_SIZE; dx++) {
            /* Pre-compute distance from origin for each integer offset */
            float fdx = (float)dx + 0.5f; /* pixel center */
            float fdy = (float)dy + 0.5f;
            cybergfx_corner_cache.dist[dy][dx] = sqrtf(fdx * fdx + fdy * fdy);
        }
    }
    cybergfx_corner_cache.valid = TRUE;
}

/* Fast corner distance lookup - returns distance from corner center */
static inline float cybergfx_corner_dist_lookup(int dx, int dy) {
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;

    if (adx < CYBERGFX_CORNER_CACHE_SIZE && ady < CYBERGFX_CORNER_CACHE_SIZE) {
        return cybergfx_corner_cache.dist[ady][adx];
    }
    /* Fallback for large values */
    float fdx = (float)adx + 0.5f;
    float fdy = (float)ady + 0.5f;
    return fast_sqrtf(fdx * fdx + fdy * fdy);
}

/* Legacy aliases - use pack_argb32/unpack_argb32 from cybergfx_pixel_format.h */
#define extract_pixel_value(pixel, a, r, g, b) unpack_argb32(pixel, a, r, g, b)
#define make_pixel_value(a, r, g, b) pack_argb32(a, r, g, b)

static inline float rfpart(float input) { return 1.0f - (input - floorf(input)); }

static inline float fpart(float input) { return input - floorf(input); }

static inline LONG cybergfx_fast_ftoi(float f) { return (LONG)(f + 0.5f); }

static inline float sdf_roundrect(float x, float y, float width, float height, float r) {
    float px = fabsf(x) - (width - r);
    float py = fabsf(y) - (height - r);

    float dx = fmaxf(px, 0.0f);
    float dy = fmaxf(py, 0.0f);

    float outside;
    if (dx > 0.0f && dy > 0.0f) {
        /* In corner region - use cached distance lookup if possible */
        int idx = (int)dx;
        int idy = (int)dy;
        if (idx < CYBERGFX_CORNER_CACHE_SIZE && idy < CYBERGFX_CORNER_CACHE_SIZE && cybergfx_corner_cache.valid) {
            /* Interpolate between cached integer positions for sub-pixel accuracy */
            outside = cybergfx_corner_cache.dist[idy][idx];
        } else {
            D(bug("sdf_roundrect: cache miss idx=%d idy=%d (max=%d)\n", idx, idy, CYBERGFX_CORNER_CACHE_SIZE));
            outside = fast_sqrtf(dx * dx + dy * dy);
        }
    } else {
        /* On straight edge - no sqrt needed, just use the max component */
        outside = fmaxf(dx, dy);
    }

    float inside = fminf(fmaxf(px, py), 0.0f);

    return outside + inside - r;
}

static inline void mix(struct InternalColor a, struct InternalColor b, float t, UBYTE *ca, UBYTE *cr, UBYTE *cg, UBYTE *cb) {
    *cr = (UBYTE)((1.0f - t) * a.r + t * b.r);
    *cg = (UBYTE)((1.0f - t) * a.g + t * b.g);
    *cb = (UBYTE)((1.0f - t) * a.b + t * b.b);
    *ca = (UBYTE)((1.0f - t) * a.a + t * b.a);
}

static inline void blend_over(UBYTE *r, UBYTE *g, UBYTE *b, UBYTE cr, UBYTE cg, UBYTE cb, float a) {
    /* Fixed-point blending: alpha scaled to 0-256 for efficient integer math */
    unsigned int alpha = (unsigned int)(a * 256.0f + 0.5f);
    unsigned int inv_alpha = 256 - alpha;
    *r = (UBYTE)((inv_alpha * (*r) + alpha * cr) >> 8);
    *g = (UBYTE)((inv_alpha * (*g) + alpha * cg) >> 8);
    *b = (UBYTE)((inv_alpha * (*b) + alpha * cb) >> 8);
}

static inline float clamp(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

static inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline void mul(struct InternalColor color1, struct InternalColor color2, UBYTE *r, UBYTE *g, UBYTE *b, UBYTE *a) {
    /* Integer multiply with divide by 255: (a*b + 127) / 255 ≈ (a*b + 128) >> 8 for speed */
    *r = (UBYTE)(((unsigned int)color1.r * color2.r + 128) / 255);
    *g = (UBYTE)(((unsigned int)color1.g * color2.g + 128) / 255);
    *b = (UBYTE)(((unsigned int)color1.b * color2.b + 128) / 255);
    *a = (UBYTE)(((unsigned int)color1.a * color2.a + 128) / 255);
}

/* Structure to hold AA rectangle computation parameters */
typedef struct {
    float halfLine;
    float max_radius;
    float center_x;
    float center_y;
    float half_w;
    float half_h;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    float aa_edge_neg;
    float aa_edge_pos;
} cybergfx_aa_rect_params;

/* Compute common parameters for AA rectangle rendering */
static inline void cybergfx_compute_aa_rect_params(UWORD x, UWORD y, UWORD width, UWORD height, UBYTE radius, float lineWidth, int board_width,
                                                   int board_height, cybergfx_aa_rect_params *params) {

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

/* Compute alpha values for fill and border from SDF distances */
static inline void cybergfx_compute_alphas(float dist_inner, float dist_outer, float aa_edge_neg, float aa_edge_pos, int hasFill, int hasBorder,
                                           float *alphaFill, float *alphaLine) {

    float alphaOuter = 0.0f;
    float alphaInner = 0.0f;

    if (hasFill || hasBorder) {
        alphaOuter = 1.0f - smoothstep(aa_edge_neg, aa_edge_pos, dist_outer);
    }
    if (hasBorder) {
        alphaInner = 1.0f - smoothstep(aa_edge_neg, aa_edge_pos, dist_inner);
        *alphaLine = clamp(alphaOuter - alphaInner, 0.0f, 1.0f);
    } else {
        *alphaLine = 0.0f;
    }
    if (hasFill) {
        if (hasBorder) {
            float interior_cover = 1.0f - smoothstep(0.0f, aa_edge_pos, dist_inner);
            *alphaFill = clamp(interior_cover, 0.0f, 1.0f);
        } else {
            *alphaFill = alphaOuter;
        }
    } else {
        *alphaFill = 0.0f;
    }
}

/* Blend fill and border colors onto background pixel */
static inline BOOL cybergfx_blend_aa_pixel(UBYTE *bg_r, UBYTE *bg_g, UBYTE *bg_b, float alphaFill, float alphaLine, UBYTE fc_r, UBYTE fc_g,
                                           UBYTE fc_b, float fc_alpha_scale, UBYTE o_r, UBYTE o_g, UBYTE o_b, float o_alpha_scale, int hasFill,
                                           int hasBorder) {

    BOOL pixel_changed = 0;

    if (hasFill && alphaFill > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
        float final_alpha = fc_alpha_scale * alphaFill;
        if (final_alpha > 0.0f) {
            blend_over(bg_r, bg_g, bg_b, fc_r, fc_g, fc_b, final_alpha);
            pixel_changed = 1;
        }
    }

    if (hasBorder && alphaLine > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
        float final_alpha = o_alpha_scale * alphaLine;
        if (final_alpha > 0.0f) {
            blend_over(bg_r, bg_g, bg_b, o_r, o_g, o_b, final_alpha);
            pixel_changed = 1;
        }
    }

    return pixel_changed;
}

#endif
