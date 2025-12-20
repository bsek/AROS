# SDF Mathematics Deep Dive

## Overview

This document provides a comprehensive mathematical foundation for the Signed Distance Field (SDF) antialiasing system. It covers the theory, derivations, and implementation details for developers who need to understand or extend the mathematical components.

## Signed Distance Fields: Mathematical Foundation

### Definition

A Signed Distance Field (SDF) is a function `f(x, y) → ℝ` that maps every point in 2D space to the shortest distance to the boundary of a shape:

```
f(x, y) = {
  > 0  if (x, y) is outside the shape
  = 0  if (x, y) is on the boundary
  < 0  if (x, y) is inside the shape
}
```

### Properties

1. **Lipschitz Continuity**: `|f(x₁, y₁) - f(x₂, y₂)| ≤ √((x₁-x₂)² + (y₁-y₂)²)`
2. **Gradient Magnitude**: `|∇f| = 1` at all points where f is differentiable
3. **Iso-contours**: Level sets `f(x, y) = c` represent parallel curves to the boundary

## Rectangle SDF Derivation

### Axis-Aligned Rectangle

For a rectangle centered at origin with half-dimensions `(a, b)`:

```
f(x, y) = max(|x| - a, |y| - b)
```

**Derivation:**
- Inside rectangle: both `|x| < a` and `|y| < b`, so `max(|x| - a, |y| - b) < 0`
- Outside rectangle: at least one of `|x| ≥ a` or `|y| ≥ b`, so `max(|x| - a, |y| - b) ≥ 0`

### Rounded Rectangle SDF

For a rounded rectangle with corner radius `r`:

```
f(x, y) = length(max(q, 0)) + min(max(qₓ, qᵧ), 0) - r
```

where `q = (qₓ, qᵧ) = (|x| - (w/2 - r), |y| - (h/2 - r))`

**Step-by-step derivation:**

1. **Transform to first quadrant**: Use symmetry by taking `|x|` and `|y|`

2. **Shift coordinate system**: Subtract `(w/2 - r, h/2 - r)` to place corner centers at origin

3. **Distance calculation**:
   - If `qₓ ≤ 0` and `qᵧ ≤ 0`: Inside corner region, distance = `max(qₓ, qᵧ) - r`
   - If `qₓ > 0` and `qᵧ ≤ 0`: Beside corner, distance = `qₓ - r`
   - If `qₓ ≤ 0` and `qᵧ > 0`: Above corner, distance = `qᵧ - r`
   - If `qₓ > 0` and `qᵧ > 0`: Outside corner, distance = `√(qₓ² + qᵧ²) - r`

4. **Unified formula**:
   ```
   length(max(q, 0)) = {
     0                    if qₓ ≤ 0 and qᵧ ≤ 0
     qₓ                   if qₓ > 0 and qᵧ ≤ 0
     qᵧ                   if qₓ ≤ 0 and qᵧ > 0
     √(qₓ² + qᵧ²)        if qₓ > 0 and qᵧ > 0
   }

   min(max(qₓ, qᵧ), 0) = {
     max(qₓ, qᵧ)         if qₓ ≤ 0 and qᵧ ≤ 0
     0                   otherwise
   }
   ```

## Border SDF Mathematics

### Conceptual Model

A border is the region between two nested shapes:
- **Outer boundary**: Original shape
- **Inner boundary**: Shape shrunk inward by border width

### Mathematical Formulation

Given:
- `f_outer(x, y)`: SDF of outer boundary
- `f_inner(x, y)`: SDF of inner boundary (shrunk shape)
- `w`: border width

The border SDF is:

```
f_border(x, y) = {
  f_outer(x, y)           if f_outer(x, y) > 0 (outside shape)
  min(|f_outer|, f_inner) if f_outer ≤ 0 and f_inner > 0 (border region)
  f_inner(x, y)           if f_inner ≤ 0 (inside inner boundary)
}
```

### Border Width Transformation

For rounded rectangles, shrinking by width `w`:
- **Linear dimensions**: `w_inner = w_outer - 2w`, `h_inner = h_outer - 2w`
- **Corner radius**: `r_inner = max(0, r_outer - w)`

**Geometric constraint**: If `w > min(w_outer/2, h_outer/2)`, the inner region becomes degenerate.

## Antialiasing: Distance to Alpha Conversion

### Linear Mapping

Simplest approach:
```
α(d) = clamp(0.5 - d, 0, 1)
```

**Problems:**
- Sharp transition at `d = ±0.5`
- Doesn't match human visual perception
- Poor quality for small features

### Smoothstep Function

Smooth S-curve transition:
```
smoothstep(a, b, x) = {
  0                    if x ≤ a
  3t² - 2t³           if a < x < b, where t = (x-a)/(b-a)
  1                    if x ≥ b
}
```

**Derivative**: `smoothstep'(a, b, x) = 6t(1-t)/(b-a)`

**Properties:**
- C¹ continuity (smooth first derivative)
- Maximum slope at `t = 0.5`
- Zero slope at endpoints

### Adaptive Smoothness

Our implementation uses distance-dependent smoothness:

```
s_adaptive(d) = s_base × max(0.3, min(1.0, |d| + 0.3))
```

**Rationale:**
- Near edges (`|d| ≈ 0`): Use minimum smoothness for sharp transitions
- Far from edges (`|d| > 0.7`): Use full smoothness to avoid artifacts
- Transition zone: Gradually interpolate smoothness

### Gamma Correction

Linear alpha blending doesn't match human perception. Gamma correction:

```
α_corrected = α_linear^(1/γ)
```

where `γ = 2.2` (standard sRGB gamma).

**Effect**: Makes antialiased edges appear more natural by accounting for display gamma and human visual response.

## Supersampling Mathematics

### Multi-Sample Antialiasing (MSAA)

For quality level `n`, take `n` samples per pixel and average:

```
α_final = (1/n) × Σᵢ₌₁ⁿ α(dᵢ)
```

### Sample Pattern Design

#### 4x Sampling (Ordered Grid)
```
Offsets: [(-0.2, -0.2), (0.2, -0.2), (-0.2, 0.2), (0.2, 0.2)]
```

**Analysis:**
- Even distribution across pixel
- Low-discrepancy pattern
- Reduced sampling artifacts

#### 9x Sampling (Regular Grid)
```
For sx ∈ {-1, 0, 1}, sy ∈ {-1, 0, 1}:
Offsets: [(sx×0.25, sy×0.25)]
```

**Trade-off**: Higher quality vs 9× computational cost

### Sample Spacing Optimization

Optimal spacing balances:
- **Too close**: Redundant samples, wasted computation
- **Too far**: Aliasing artifacts, poor coverage

Our choice of `0.2` and `0.25` pixel spacing is based on:
1. Nyquist sampling theory
2. Empirical quality testing
3. Performance benchmarks

## Performance Analysis

### Computational Complexity

For an `M×N` pixel region:

| Component | Complexity | Quality 0 | Quality 2 | Quality 3 |
|-----------|------------|-----------|-----------|-----------|
| SDF Evaluation | O(1) per sample | M×N | 4×M×N | 9×M×N |
| Alpha Conversion | O(1) | M×N | 4×M×N | 9×M×N |
| Blending | O(1) per pixel | M×N | M×N | M×N |

**Total**: O(k×M×N) where k = samples per pixel

### Cache Efficiency

**Pixel-order traversal** provides good cache locality:
```c
for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
        // Process pixel (x, y)
    }
}
```

**Memory access pattern**: Sequential access to framebuffer improves cache hit rates.

### Bounding Box Optimization

Only process pixels within distance `d_max` of shape:

**Savings**: For shape with area `A` and bounding box area `B`:
- **Without culling**: Process `B` pixels
- **With culling**: Process approximately `A + 2π×d_max×perimeter` pixels

For thin borders, savings can be 90%+.

## Numerical Considerations

### Floating-Point Precision

**Single precision (32-bit)** provides adequate accuracy:
- **Mantissa**: 23 bits ≈ 7 decimal digits
- **Pixel precision**: Sub-pixel accuracy to ~1/128 pixel
- **Distance range**: ±10⁶ pixels (more than sufficient)

### Fixed-Point Interface

Input parameters use 8.8 fixed-point (Q8.8):
```
float_value = fixed_point_value / 256.0f
```

**Range**: [0, 255.996] with 1/256 precision
**Precision**: ~0.004 pixels (excellent for UI elements)

### Numerical Stability

**Distance calculations** are numerically stable:
- No division by small numbers
- Monotonic functions
- Well-conditioned operations

**Alpha blending** potential issues:
- Overflow: Prevented by clamping to [0,1]
- Underflow: Handled by minimum threshold

## Error Analysis

### Discretization Error

**Sampling error**: Maximum error = `0.5 × pixel_size`
- **Quality 0**: Up to 0.5 pixel error
- **Quality 2**: Up to 0.125 pixel error (4x sampling)
- **Quality 3**: Up to 0.083 pixel error (9x sampling)

### SDF Approximation Error

**Rounded rectangles**: Exact for `r > 0`
**Sharp corners**: Exact (`r = 0`)
**Borders**: Exact except for degenerate cases

### Gamma Correction Error

**Approximation**: `x^(1/2.2)` using simplified power function
**Maximum error**: <2% for typical alpha values
**Visual impact**: Negligible for UI rendering

## Advanced Topics

### Adaptive Sampling

Future optimization: Use edge detection to vary sample count:
```c
float edge_strength = |∇f|; // Gradient magnitude
int samples = (edge_strength > threshold) ? 9 : 1;
```

### Temporal Antialiasing

For animation, accumulate samples over time:
```c
α_final = lerp(α_previous, α_current, temporal_factor);
```

### Multi-Distance Fields

Combine multiple SDFs for complex shapes:
```c
// Union: min(f₁, f₂)
// Intersection: max(f₁, f₂)
// Subtraction: max(f₁, -f₂)
```

### Analytical Derivatives

For advanced filtering, compute SDF gradients:
```c
float2 gradient_rectangle_sdf(float2 p, float2 size, float radius) {
    float2 q = abs(p) - (size - radius);
    float2 grad_q = sign(p) * sign(max(q, 0));

    if (max(q.x, q.y) < 0) {
        // Inside corner region
        return (q.x > q.y) ? float2(grad_q.x, 0) : float2(0, grad_q.y);
    } else {
        // Outside or at corner
        return normalize(max(q, 0)) * grad_q;
    }
}
```

## Implementation Verification

### Unit Tests

```c
// Test SDF properties
assert(rectangle_sdf(0, 0, w, h, 0) < 0);  // Center inside
assert(rectangle_sdf(w/2, h/2, w, h, 0) == 0);  // On edge
assert(rectangle_sdf(w, h, w, h, 0) > 0);  // Outside

// Test continuity
float d1 = rectangle_sdf(x, y, w, h, r);
float d2 = rectangle_sdf(x + ε, y, w, h, r);
assert(abs(d1 - d2) <= ε + tolerance);
```

### Visual Validation

```c
// Generate distance field visualization
for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        float d = rectangle_sdf(x - cx, y - cy, w, h, r);
        uint8_t intensity = (uint8_t)(128 + d * 50);
        set_pixel(x, y, rgb(intensity, intensity, intensity));
    }
}
```

### Performance Benchmarks

```c
clock_t start = clock();
for (int i = 0; i < 1000000; i++) {
    float d = rectangle_sdf(random_x(), random_y(), w, h, r);
    volatile float result = sdf_to_alpha(d);  // Prevent optimization
}
clock_t end = clock();
printf("Time per SDF evaluation: %.2f nanoseconds\n",
       1e9 * (end - start) / CLOCKS_PER_SEC / 1000000);
```

## References

1. **"Improved Alpha-Tested Magnification for Vector Textures and Special Effects"** - Valve Corporation, 2007
2. **"Resolution Independent Curve Rendering using Programmable Graphics Hardware"** - Loop & Blinn, 2005
3. **"Distance Functions"** - Iñigo Quílez, https://iquilezles.org/articles/distfunctions2d/
4. **"GPU Gems 3: Chapter 25. Rendering Vector Art on the GPU with Distance Fields"** - NVIDIA, 2007
5. **"Real-Time Rendering, 4th Edition"** - Akenine-Möller, Haines, Hoffman, 2018

---

**Author**: AROS Development Team
**Last Updated**: December 2024
**Version**: 1.0
