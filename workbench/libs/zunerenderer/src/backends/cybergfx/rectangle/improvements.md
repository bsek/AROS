You can carve the rectangle into three zones along each row:

1. **Skip zone:** Pixels entirely outside the outer AA band (you already have this via the `rel_y_abs` early-out).
2. **Edge zone:** Pixels whose signed distance to either the fill boundary or stroke boundary is within the AA soft range.
3. **Interior zone:** Pixels deeper than the AA band, i.e. `dist_inner <= params.aa_edge_neg` for fill or `dist_outer <= params.aa_edge_neg` for stroke.

To implement that split per row:

- Use the SDF API you already have to find the first/last *edge* column. For a fixed `rel_y`, invert the round-rect SDF to get the x-coordinates where `dist_inner == params.aa_edge_neg` and `dist_inner == params.aa_edge_pos`. That gives you `[edge_fill_left, edge_fill_right]`. Do the same with `dist_outer` for the stroke. Clamp to your `[min_x, max_x]`.

- The pixels between `edge_fill_left + 1` and `edge_fill_right - 1` are fully covered by fill; they can skip the AA math and use a constant coverage of `1.0`. Likewise, if you need the border interior, shrink/grow the SDF so you get the band that corresponds to lineWidth.

- Process each row as:
  1. **Edge pass:** From `min_x` to `edge_fill_left` (and mirrored on the right) keep your existing SIMD path because you still need partial coverage and per-pixel brush sampling.
  2. **Interior fill pass:** For the center span, skip `cybergfx_sdf_roundrect_batch*` and `cybergfx_compute_alphas_batch*`. Instead, sample the brush once per pixel (still SIMD batches) but multiply the brush alpha by `1.0` (or by whatever stroke coverage is constant). If the brush itself is constant (solid color), you can even precompute the premultiplied color and perform a straight `blend_over` without extracting background alpha for every pixel.
  3. **Stroke-only span (if lineWidth > 0):** Between fill interior and stroke exterior lies the stroke interior. Here fill coverage is zero but stroke coverage is 1, so you can blend only the border color with a constant alpha derived from `o_alpha_scale`.

- For very wide rectangles, the interior span dominates, so removing the SDF/AA computation there saves most cycles.

Implementation hints:

- Add helpers like `cybergfx_sdf_roundrect_bounds_y(rel_y, params, edge_neg, edge_pos, float *x0, float *x1)` that solve for x using the round-rect shape you already evaluate. That avoids scanning per pixel to find edges.

- The SIMD loops stay but get three variants: `process_edge_batch`, `process_fill_batch`, `process_stroke_batch`. Each takes flags telling whether fill/stroke apply. Inside those, skip work you know is constant.

- Keep existing code as fallback when the computed interior span is empty (e.g., very thin shapes) to avoid branching overhead in narrow cases.

With this structure you spend the expensive SDF math only near the AA boundaries; the interior bulk becomes cheaper brush sampling + blending.
