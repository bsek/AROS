#include "../cybergfx_brush_sampler.h"
#include "../cybergfx_pixel_format.h"
#include "clib/intuition_protos.h"
#include "cybergfx_rectangle_internal.h"
#include "cybergraphx/cybergraphics.h"

#include <stdlib.h>
#include <string.h>

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

    if ((brush->type == ZUNE_BRUSH_TYPE_SOLID || brush->type == ZUNE_BRUSH_TYPE_PEN) && brush->internal.valid) {
        ULONG pixel = brush->internal.color.original_pixel;
        state->solid = TRUE;
        state->solid_r = ZUNE_GET_RED(pixel);
        state->solid_g = ZUNE_GET_GREEN(pixel);
        state->solid_b = ZUNE_GET_BLUE(pixel);
    }
}

static void cybergfx_sample_fill_block(const cybergfx_fill_state *state, WORD rect_x, WORD rect_y, UWORD rect_w, UWORD rect_h, WORD px_start, WORD py,
                                       UBYTE out_r[4], UBYTE out_g[4], UBYTE out_b[4]) {
    if (!state->enabled) {
        for (WORD i = 0; i < 4; ++i) {
            out_r[i] = out_g[i] = out_b[i] = 0;
        }
        return;
    }

    if (state->solid) {
        for (WORD i = 0; i < 4; ++i) {
            out_r[i] = state->solid_r;
            out_g[i] = state->solid_g;
            out_b[i] = state->solid_b;
        }
        return;
    }

    WORD px_coords[4];
    for (WORD i = 0; i < 4; ++i) {
        px_coords[i] = px_start + i;
    }

    UBYTE a_tmp[4];
    SampleBrushBatch4(state->brush, rect_x, rect_y, rect_w, rect_h, px_coords, py, out_r, out_g, out_b, a_tmp);
}

/*****************************************************************************/
/* Anti-aliased Rectangle Rendering to RastPort                              */
/*****************************************************************************/

void CybergfxAARectangleRasterPort(struct RastPort *rp, UWORD x, UWORD y, UWORD width, UWORD height, UBYTE radius, float line_width,
                                   struct ZuneBrush *fill_brush, struct InternalColor *outline_color, struct InternalColor *outline_tint,
                                   BOOL drawFill, BOOL draw_border) {

    ENTER_FUNCTION("CybergfxAARectangleRasterPort");

    if (!rp || !rp->BitMap) {
        D(bug("CybergfxAARectangleRasterPort: Invalid RastPort or bitmap\n"));
        return;
    }

    cybergfx_fill_state fill_state;
    cybergfx_init_fill_state(fill_brush, drawFill, &fill_state);
    BOOL hasFill = fill_state.enabled;
    BOOL hasBorder = draw_border && line_width > 0.0f && outline_color && outline_tint;
    if (!hasFill && !hasBorder) {
        return;
    }

    ULONG bitmap_width = GetBitMapAttr(rp->BitMap, BMA_WIDTH);
    ULONG bitmap_height = GetBitMapAttr(rp->BitMap, BMA_HEIGHT);
    if (bitmap_width == 0 || bitmap_height == 0) {
        EXIT_FUNCTION("CybergfxAARectangleRasterPort");
        return;
    }

    cybergfx_aa_rect_params params;
    cybergfx_compute_aa_rect_params(x, y, width, height, radius, line_width, (UWORD)bitmap_width, (UWORD)bitmap_height, &params);
    if (params.max_x < params.min_x || params.max_y < params.min_y) {
        EXIT_FUNCTION("CybergfxAARectangleRasterPort");
        return;
    }

    WORD core_min_x = 0, core_max_x = -1, core_min_y = 0, core_max_y = -1;
    BOOL core_prefilled = FALSE;
    if (fill_state.solid) {
        float rect_left = params.center_x - params.half_w;
        float rect_right = params.center_x + params.half_w;
        float rect_top = params.center_y - params.half_h;
        float rect_bottom = params.center_y + params.half_h;

        float margin = cybergfx_aa_smoothness;
        if (hasBorder) {
            margin += params.halfLine;
        }
        float inset = fmaxf(params.max_radius, margin);

        core_min_x = (WORD)ceilf(rect_left + inset);
        core_max_x = (WORD)floorf(rect_right - inset);
        core_min_y = (WORD)ceilf(rect_top + inset);
        core_max_y = (WORD)floorf(rect_bottom - inset);

        if (core_min_x <= core_max_x && core_min_y <= core_max_y) {
            WORD core_width = core_max_x - core_min_x + 1;
            WORD core_height = core_max_y - core_min_y + 1;
            if (core_width > 0 && core_height > 0 && fill_state.brush && fill_state.brush->internal.valid) {
                ULONG solid_pixel = fill_state.brush->internal.color.original_pixel;
                FillPixelArray(rp, (UWORD)core_min_x, (UWORD)core_min_y, (UWORD)core_width, (UWORD)core_height, solid_pixel);
                core_prefilled = TRUE;
            }
        }
    }

    WORD row_width = params.max_x - params.min_x + 1;
    if (row_width <= 0) {
        EXIT_FUNCTION("CybergfxAARectangleRasterPort");
        return;
    }

    /* Use multi-scanline batching to reduce ReadPixelArray/WritePixelArray syscall overhead */
    #define SCANLINE_BATCH 32
    WORD total_height = params.max_y - params.min_y + 1;
    
    ULONG *batch_buffer = malloc((ULONG)row_width * SCANLINE_BATCH * sizeof(ULONG));
    if (!batch_buffer) {
        D(bug("CybergfxAARectangleRasterPort: Failed to allocate batch buffer\n"));
        return;
    }
    
    /* Track which rows in the batch are dirty */
    BOOL batch_dirty[SCANLINE_BATCH];

    UBYTE o_a = 0, o_r = 0, o_g = 0, o_b = 0;
    float o_alpha_scale = 0.0f;
    if (hasBorder) {
        o_r = outline_color->r;
        o_g = outline_color->g;
        o_b = outline_color->b;
        o_a = outline_color->a;
        o_alpha_scale = o_a / 255.0f;
    }

    float base_rel_x = 0.5f - params.center_x;

    /* Pre-compute outer dimensions for border */
    float outer_half_w = params.half_w + params.halfLine;
    float outer_half_h = params.half_h + params.halfLine;
    float outer_radius = params.max_radius + params.halfLine;

    /* Process rows in batches */
    for (WORD batch_start_y = params.min_y; batch_start_y <= params.max_y; batch_start_y += SCANLINE_BATCH) {
        WORD batch_height = (batch_start_y + SCANLINE_BATCH <= params.max_y + 1) 
                              ? SCANLINE_BATCH 
                              : (params.max_y - batch_start_y + 1);
        
        /* Read entire batch from raster port */
        ReadPixelArray(batch_buffer, 0, 0, (ULONG)row_width * 4, rp, 
                       params.min_x, batch_start_y, row_width, batch_height, CYBERGFX_PIXELFORMAT_ARGB32);
        
        /* Reset dirty flags */
        for (WORD i = 0; i < batch_height; ++i) {
            batch_dirty[i] = FALSE;
        }
        
        BOOL any_dirty = FALSE;

        for (WORD row_in_batch = 0; row_in_batch < batch_height; ++row_in_batch) {
            WORD py = batch_start_y + row_in_batch;
            ULONG *row_buffer = batch_buffer + (row_in_batch * row_width);
            
            float rel_y = (py + 0.5f) - params.center_y;
            float rel_y_abs = fabsf(rel_y);
            if (rel_y_abs > outer_half_h + cybergfx_aa_smoothness) {
                continue;
            }

            BOOL row_in_core = core_prefilled && py >= core_min_y && py <= core_max_y;

            for (WORD px = params.min_x; px <= params.max_x; px += 4) {
                if (row_in_core && px >= core_min_x && px + 3 <= core_max_x) {
                    continue;
                }

                float rel_x[4];
                for (WORD i = 0; i < 4; ++i) {
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

                WORD valid = (px + 3 <= params.max_x) ? 4 : (params.max_x - px + 1);
                WORD buffer_index = px - params.min_x;

                for (WORD i = 0; i < valid; ++i) {
                    float totalAlpha = alphaFill[i] + alphaLine[i];
                    if (totalAlpha < CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
                        continue; /* Skip fully transparent pixels */
                    }

                    /* Fast path for solid fill with no blending needed */
                    if (fill_state.solid && alphaFill[i] > 0.9999f && alphaLine[i] < CYBERGFX_AA_MIN_ALPHA_THRESHOLD) {
                        row_buffer[buffer_index + i] = fill_state.brush->internal.color.original_pixel;
                        batch_dirty[row_in_batch] = TRUE;
                        any_dirty = TRUE;
                        continue;
                    }

                    UBYTE bg_a, bg_r, bg_g, bg_b;
                    /* Use logical ARGB32 for ReadPixelArray/WritePixelArray buffers */
                    unpack_argb32_logical(row_buffer[buffer_index + i], &bg_a, &bg_r, &bg_g, &bg_b);

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
                        /* Use logical ARGB32 for ReadPixelArray/WritePixelArray buffers */
                        row_buffer[buffer_index + i] = pack_argb32_logical(bg_a, bg_r, bg_g, bg_b);
                        batch_dirty[row_in_batch] = TRUE;
                        any_dirty = TRUE;
                    }
                }
            }
        }

        /* Write back dirty rows - find contiguous dirty regions for optimal writes */
        if (any_dirty) {
            WORD write_start = -1;
            for (WORD i = 0; i <= batch_height; ++i) {
                BOOL is_dirty = (i < batch_height) && batch_dirty[i];
                if (is_dirty && write_start < 0) {
                    write_start = i;
                } else if (!is_dirty && write_start >= 0) {
                    /* Write contiguous dirty region */
                    WORD write_height = i - write_start;
                    WritePixelArray(batch_buffer + (write_start * row_width), 0, 0, (ULONG)row_width * 4, 
                                    rp, params.min_x, batch_start_y + write_start, row_width, write_height, 
                                    CYBERGFX_PIXELFORMAT_ARGB32);
                    write_start = -1;
                }
            }
        }
    }

    free(batch_buffer);
    #undef SCANLINE_BATCH

    EXIT_FUNCTION("CybergfxAARectangleRasterPort");
}
