#include "../cybergfx_antialiasing.h"
#include "../cybergfx_brush_sampler.h"
#include "../cybergfx_pixel_format.h"
#include "cybergfx_rectangle_internal.h"
#include <string.h>
#include <stdlib.h>

/*****************************************************************************/
/* Anti-aliased Rectangle Rendering to DrawingBoard */
/*****************************************************************************/

typedef struct {
    BOOL enabled;
    BOOL solid;
    UBYTE solid_r;
    UBYTE solid_g;
    UBYTE solid_b;
    struct ZuneBrush *brush;
} cybergfx_fill_state;

static void cybergfx_init_fill_state(struct ZuneBrush *brush, BOOL draw_fill, cybergfx_fill_state *state) {
    state->enabled = FALSE;
    state->solid = FALSE;
    state->solid_r = state->solid_g = state->solid_b = 0;
    state->brush = NULL;

    if (!draw_fill || !brush) {
        return;
    }

    if (!brush->internal.valid) {
        return;
    }

    state->enabled = TRUE;
    state->brush = brush;

    if (brush->type == ZUNE_BRUSH_TYPE_SOLID || brush->type == ZUNE_BRUSH_TYPE_PEN) {
        state->solid = TRUE;
        state->solid_r = brush->internal.color.r;
        state->solid_g = brush->internal.color.g;
        state->solid_b = brush->internal.color.b;
    }
}

static void cybergfx_sample_fill_block(const cybergfx_fill_state *state, WORD rect_x, WORD rect_y, UWORD rect_w, UWORD rect_h, int px_start, int py,
                                       UBYTE out_r[4], UBYTE out_g[4], UBYTE out_b[4]) {
    if (!state->enabled) {
        for (int i = 0; i < 4; ++i) {
            out_r[i] = out_g[i] = out_b[i] = 0;
        }
        return;
    }

    if (state->solid) {
        for (int i = 0; i < 4; ++i) {
            out_r[i] = state->solid_r;
            out_g[i] = state->solid_g;
            out_b[i] = state->solid_b;
        }
        return;
    }

    int px_coords[4];
    for (int i = 0; i < 4; ++i) {
        px_coords[i] = px_start + i;
    }

    UBYTE a_tmp[4];
    SampleBrushBatch4(state->brush, rect_x, rect_y, rect_w, rect_h, px_coords, py, out_r, out_g, out_b, a_tmp);
}

static BOOL cybergfx_prefill_solid_core_board(struct DrawingBoard *board, const cybergfx_fill_state *state, BOOL hasBorder,
                                              const cybergfx_aa_rect_params *params, int *out_min_x, int *out_max_x, int *out_min_y, int *out_max_y) {
    if (!state->solid || !state->brush || !state->brush->internal.valid) {
        return FALSE;
    }

    float rect_left = params->center_x - params->half_w;
    float rect_right = params->center_x + params->half_w;
    float rect_top = params->center_y - params->half_h;
    float rect_bottom = params->center_y + params->half_h;

    float margin = cybergfx_aa_smoothness;
    if (hasBorder) {
        margin += params->halfLine;
    }
    float inset = fmaxf(params->max_radius, margin);

    int min_x = (int)ceilf(rect_left + inset);
    int max_x = (int)floorf(rect_right - inset);
    int min_y = (int)ceilf(rect_top + inset);
    int max_y = (int)floorf(rect_bottom - inset);

    if (min_x > max_x || min_y > max_y) {
        return FALSE;
    }

    /* Debug the solid fill color values */
    D(bug("cybergfx_prefill_solid_core: solid rgb = (%d, %d, %d)\n", (int)state->solid_r, (int)state->solid_g, (int)state->solid_b));

    /* Use pack_argb32 to create pixel in correct format for direct memory access */
    ULONG solid_pixel = pack_argb32(255, state->solid_r, state->solid_g, state->solid_b);
    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;

    for (int py = min_y; py <= max_y; ++py) {
        ULONG *row_ptr = pixels + (size_t)py * pitch_pixels;
        ULONG *dest = row_ptr + min_x;
        for (int px = min_x; px <= max_x; ++px) {
            *dest++ = solid_pixel;
        }
    }

    if (out_min_x)
        *out_min_x = min_x;
    if (out_max_x)
        *out_max_x = max_x;
    if (out_min_y)
        *out_min_y = min_y;
    if (out_max_y)
        *out_max_y = max_y;

    return TRUE;
}

void CybergfxAARectangleDrawingBoard(struct DrawingBoard *board, UWORD x, UWORD y, UWORD width, UWORD height, UBYTE radius, float line_width,
                                     struct ZuneBrush *fill_brush, struct InternalColor *outline_color, struct InternalColor *outline_tint,
                                     BOOL drawFill, BOOL draw_border) {

    ENTER_FUNCTION("CybergfxAARectangleDrawingBoard");

    if (!board || !board->pixels) {
        D(bug("CybergfxAARectangleDrawingBoard: Invalid board or pixels\n"));
        return;
    }

    cybergfx_fill_state fill_state;
    cybergfx_init_fill_state(fill_brush, drawFill, &fill_state);
    BOOL hasFill = fill_state.enabled;
    BOOL hasBorder = draw_border && line_width > 0.0f && outline_color && outline_tint;
    if (!hasFill && !hasBorder) {
        return;
    }

    /* Debug fill brush RGB values if available */
    if (fill_brush && fill_brush->internal.valid) {
        D(bug("Fill brush RGBA: (%d, %d, %d, %d)\n", (int)fill_brush->internal.color.r, (int)fill_brush->internal.color.g,
              (int)fill_brush->internal.color.b, (int)fill_brush->internal.color.a));
    } else if (fill_brush) {
        D(bug("Fill brush present but no valid color information\n"));
    } else {
        D(bug("No fill brush provided\n"));
    }

    cybergfx_aa_rect_params params;
    cybergfx_compute_aa_rect_params(x, y, width, height, radius, line_width, board->width, board->height, &params);
    if (params.max_x < params.min_x || params.max_y < params.min_y) {
        EXIT_FUNCTION("CybergfxAARectangleDrawingBoard");
        return;
    }

    int core_min_x = 0, core_max_x = -1, core_min_y = 0, core_max_y = -1;
    BOOL core_prefilled = FALSE;
    if (fill_state.solid) {
        core_prefilled =
            cybergfx_prefill_solid_core_board(board, &fill_state, hasBorder, &params, &core_min_x, &core_max_x, &core_min_y, &core_max_y);
    }

    /* Debug fill brush RGB values if available */
    if (fill_state.solid) {
        D(bug("Fill state RGBA: (%d, %d, %d)\n", (int)fill_state.solid_r, (int)fill_state.solid_g, (int)fill_state.solid_b));
    }

    UBYTE o_a = 0, o_r = 0, o_g = 0, o_b = 0;
    float o_alpha_scale = 0.0f;
    if (hasBorder) {
        mul(*outline_color, *outline_tint, &o_r, &o_g, &o_b, &o_a);
        o_alpha_scale = o_a / 255.0f;
    }

    ULONG *pixels = (ULONG *)board->pixels;
    ULONG pitch_pixels = board->pitch / 4;
    float base_rel_x = 0.5f - params.center_x;

    /* Pre-compute outer dimensions for border */
    float outer_half_w = params.half_w + params.halfLine;
    float outer_half_h = params.half_h + params.halfLine;
    float outer_radius = params.max_radius + params.halfLine;

    for (int py = params.min_y; py <= params.max_y; ++py) {
        float rel_y = (py + 0.5f) - params.center_y;
        float rel_y_abs = fabsf(rel_y);
        if (rel_y_abs > outer_half_h + cybergfx_aa_smoothness) {
            continue;
        }

        ULONG *row_ptr = pixels + (size_t)py * pitch_pixels;
        BOOL row_in_core = core_prefilled && py >= core_min_y && py <= core_max_y;

        for (int px = params.min_x; px <= params.max_x; px += 4) {
            if (row_in_core && px >= core_min_x && px + 3 <= core_max_x) {
                continue;
            }

            float rel_x[4];
            for (int i = 0; i < 4; ++i) {
                rel_x[i] = px + i + base_rel_x;
            }

            float dist_outer[4];
            cybergfx_sdf_roundrect_batch4(rel_x, rel_y, outer_half_w, outer_half_h, outer_radius, dist_outer);

            /* Early exit: if all 4 pixels are fully outside, skip this batch */
            if (dist_outer[0] > cybergfx_aa_smoothness && dist_outer[1] > cybergfx_aa_smoothness &&
                dist_outer[2] > cybergfx_aa_smoothness && dist_outer[3] > cybergfx_aa_smoothness) {
                continue;
            }

            float dist_inner[4];
            cybergfx_sdf_roundrect_batch4(rel_x, rel_y, params.half_w, params.half_h, params.max_radius, dist_inner);

            float alphaFill[4] = {0}, alphaLine[4] = {0};
            cybergfx_compute_alphas_batch4(dist_inner, dist_outer, params.aa_edge_neg, params.aa_edge_pos, hasFill, hasBorder, alphaFill, alphaLine);

            UBYTE fc_r[4], fc_g[4], fc_b[4];
            if (hasFill) {
                cybergfx_sample_fill_block(&fill_state, x, y, width, height, px, py, fc_r, fc_g, fc_b);
            }

            int valid = (px + 3 <= params.max_x) ? 4 : (params.max_x - px + 1);
            for (int i = 0; i < valid; ++i) {
                float totalAlpha = alphaFill[i] + alphaLine[i];
                if (totalAlpha < CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
                    continue; /* Skip fully transparent pixels */
                }

                ULONG *dst = row_ptr + px + i;

                /* Fast path for solid fill with no blending needed */
                if (fill_state.solid && alphaFill[i] > 0.9999f && alphaLine[i] < CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
                    *dst = pack_argb32(255, fill_state.solid_r, fill_state.solid_g, fill_state.solid_b);
                    continue;
                }

                UBYTE bg_a, bg_r, bg_g, bg_b;
                unpack_argb32(*dst, &bg_a, &bg_r, &bg_g, &bg_b);
                BOOL pixel_changed = FALSE;

                if (hasFill && alphaFill[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
                    float final_alpha = alphaFill[i];
                    if (fill_state.solid && final_alpha > 0.9999f) {
                        bg_r = fill_state.solid_r;
                        bg_g = fill_state.solid_g;
                        bg_b = fill_state.solid_b;
                        pixel_changed = TRUE;
                    } else if (final_alpha > 0.0f) {
                        blend_over(&bg_r, &bg_g, &bg_b, fc_r[i], fc_g[i], fc_b[i], final_alpha);
                        pixel_changed = TRUE;
                    }
                }

                if (hasBorder && alphaLine[i] > CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
                    float final_alpha = o_alpha_scale * alphaLine[i];
                    if (final_alpha > 0.0f) {
                        blend_over(&bg_r, &bg_g, &bg_b, o_r, o_g, o_b, final_alpha);
                        pixel_changed = TRUE;
                    }
                }

                if (pixel_changed) {
                    bg_a = 255;
                    *dst = pack_argb32(bg_a, bg_r, bg_g, bg_b);
                }
            }
        }
    }

    EXIT_FUNCTION("CybergfxAARectangleDrawingBoard");
}
