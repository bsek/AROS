# zunegfx.library - Modern 2D Rendering Library for AROS

zunegfx.library provides a clean, unified 2D rendering API with support for
both CyberGraphics (software/CPU) and OpenGL (hardware/GPU) backends. It is
designed as the rendering foundation for Zune/MUI, but can be used standalone.

The library automatically selects the best available backend. When OpenGL
(mesa3dgl) is available and the target has a Window, it uses GPU-accelerated
rendering with FBO support. Otherwise it falls back to CyberGraphics.

---

## Concepts

**RenderContext** - The central object. Created from a Window, it holds the
backend state, current render target, font, clip region, and caches. All
drawing functions take a RenderContext as their first argument.

**DrawingBoard** - An off-screen rendering surface (double buffer). Always has
a BitMap for legacy compatibility. When the OpenGL backend is active, it also
gets an FBO for GPU-accelerated rendering. Switch between targets with
`ZuneSetTarget()`.

**ZuneBrush** - Describes how shapes are filled. Supports solid colors,
linear/radial gradients, textures, pen colors, and patterns. Use the
`ZUNE_BRUSH_SOLID()` macro for the common case of a solid ARGB color.

**ZuneTexture** - An image that can be drawn, scaled, tinted, tiled, or used
as a brush fill. Can be created from pixel data, files (via datatypes), or by
capturing a DrawingBoard.

**Colors** - All colors are 32-bit ARGB. Use the `ZUNE_COLOR_ARGB32(a,r,g,b)`
macro or the predefined constants (`ZUNE_RED`, `ZUNE_WHITE`, etc.).

**Coordinates** - Screen-style: Y=0 at the top, increasing downward. All
coordinates are relative to the current render target (DrawingBoard or Window
inner area).

---

## Quick Start

```c
#include <libraries/zunegfx.h>
#include <proto/zunegfx.h>

struct Library *ZuneGfxBase;
struct RenderContext *rctx;
struct DrawingBoard *board;

ZuneGfxBase = OpenLibrary("zunegfx.library", 1);

/* Create context bound to a window */
rctx = ZuneCreateRenderContextForWindow(
    window, screen->ViewPort.ColorMap, BACKEND_BEST_AVAILABLE);

/* Create off-screen buffer */
board = ZuneCreateDrawingBoardForRenderContext(rctx, width, height, 0);

/* Draw to the buffer */
ZuneSetTarget(rctx, board);
ZuneClearRenderContext(rctx, ZUNE_DARKGRAY);
ZuneDrawRectangleXYWH(rctx, 10, 10, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_RED));
ZuneFillCircleAAAt(rctx, 200, 100, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));
ZuneDrawTextAt(rctx, 10, 100, "Hello", 5, ZUNE_WHITE);

/* Present to screen */
ZunePresent(rctx, 0, 0,
            window->BorderLeft, window->BorderTop,
            board->width, board->height);

/* Cleanup */
ZuneDestroyDrawingBoard(rctx, board);
ZuneDestroyRenderContext(rctx);
CloseLibrary(ZuneGfxBase);
```

---

## RenderContext Management

### ZuneCreateRenderContextForWindow(window, colormap, backend_type)

Creates a RenderContext bound to a Window. The window is required for
OpenGL context creation. `backend_type` selects the rendering backend:

| Constant | Value | Description |
|---|---|---|
| `BACKEND_BEST_AVAILABLE` | 0 | Auto-select (OpenGL if available) |
| `BACKEND_SOFTWARE` | 1 | Software only |
| `BACKEND_CYBERGFX` | 2 | CyberGraphics (CPU) |
| `BACKEND_OPENGL` | 3 | OpenGL (GPU) |

Returns `NULL` on failure.

### ZuneCreateRenderContextA(tags)

Tag-based creation. Available tags:

| Tag | Type | Description |
|---|---|---|
| `ZUNE_RenderContext_Window` | `struct Window *` | Required |
| `ZUNE_RenderContext_ColorMap` | `struct ColorMap *` | Required |
| `ZUNE_RenderContext_Backend` | `UWORD` | Default: `BEST_AVAILABLE` |

### ZuneDestroyRenderContext(rctx)

Frees the RenderContext and all associated backend resources.

### ZuneClearRenderContext(rctx, color)

Fills the current render target with a solid ARGB color.

---

## DrawingBoard Management

### ZuneCreateDrawingBoardForRenderContext(rctx, width, height, flags)

Creates an off-screen DrawingBoard. Always has a BitMap; the OpenGL
backend adds an FBO for hardware rendering.

| Flag | Description |
|---|---|
| `ZUNE_DRAWINGBOARD_HARDWARE` | Prefer hardware acceleration |
| `ZUNE_DRAWINGBOARD_ALPHA` | Support alpha blending |
| `ZUNE_DRAWINGBOARD_CACHED` | Cache in video memory |
| `ZUNE_DRAWINGBOARD_TEMP` | Optimize for short-lived usage |
| `ZUNE_DRAWINGBOARD_LINEARMEM` | Force linear memory layout |

### ZuneCreateDrawingBoardA(tags)

Tag-based creation. Available tags:

| Tag | Type | Description |
|---|---|---|
| `ZUNE_DrawingBoard_RenderContext` | `struct RenderContext *` | Required |
| `ZUNE_DrawingBoard_Width` | `UWORD` | Required |
| `ZUNE_DrawingBoard_Height` | `UWORD` | Required |
| `ZUNE_DrawingBoard_Flags` | `ULONG` | Default: 0 |

### ZuneSetTarget(rctx, board)

Switches the render target. Pass `NULL` for board to render directly to
the window's RastPort. Pass a DrawingBoard to render off-screen.

For OpenGL, switching between DrawingBoards uses `glBindFramebuffer()`
which is very fast. Switching to a window uses `glAMakeCurrent()`.

Returns `TRUE` on success.

### ZuneDestroyDrawingBoard(rctx, board)

Frees the DrawingBoard and its associated resources (bitmap, FBO).

### ZuneClearDrawingBoard(rctx, color)

Fills the current DrawingBoard with a solid ARGB color.

---

## Drawing - Rectangles

All rectangle functions use `struct ZuneRect {WORD x, y; UWORD width, height}`.
Convenience macros with `XYWH` suffix accept individual coordinates.

### ZuneFillRectangle(rctx, rect, brush) / ZuneDrawRectangleXYWH(rctx, x, y, w, h, brush)

Draws a filled rectangle using the given brush.

**Example - solid color:**
```c
ZuneDrawRectangleXYWH(rctx, 50, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_RED));
```

**Example - gradient:**
```c
struct ZuneGradientStop stops[] = {
    {0.0f, ZUNE_COLOR_ARGB32(255, 255, 0, 0)},
    {1.0f, ZUNE_COLOR_ARGB32(255, 0, 0, 255)},
};
struct ZuneBrush brush = {
    .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
    .internal = {0},
    .data = {.linear = {
        .start = ZUNE_POINT_LITERAL(0, 0),
        .end = ZUNE_POINT_LITERAL(0, 100),
        .stops = stops, .stop_count = 2
    }}
};
ZuneDrawRectangleXYWH(rctx, 50, 50, 100, 80, &brush);
```

### ZuneFillRectangleRounded / ZuneDrawRectangleRoundedXYWH

Filled rectangle with rounded corners. The corner radius is clamped
to half the smallest dimension.

### ZuneDrawRectangleOutline / ZuneDrawRectangleOutlineXYWH

1-pixel outlined rectangle.

### ZuneDrawRectangleOutlineStyled / ZuneDrawRectangleOutlineStyledXYWH

Outlined rectangle with custom line width.

### ZuneDrawRectangleRoundedOutline / ZuneDrawRectangleRoundedOutlineXYWH

1-pixel outlined rounded rectangle.

### ZuneDrawRectangleRoundedOutlineStyled / ZuneDrawRectangleRoundedOutlineStyledXYWH

Outlined rounded rectangle with custom line width.

### ZuneDrawRectangleRoundedStyled / ZuneDrawRectangleRoundedStyledXYWH

Combined fill and border in a single call. More efficient than
separate fill + outline calls.

```c
ZuneDrawRectangleRoundedStyledXYWH(rctx, 50, 50, 140, 80, 15, 3,
                                   ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), ZUNE_RED);
```

---

## Drawing - Rectangles (Antialiased)

### ZuneFillRectangleRoundedAA / ZuneFillRectangleRoundedAAXYWH

Antialiased filled rounded rectangle.

### ZuneFillRectangleRoundedStyledAA / ZuneFillRectangleRoundedStyledAAXYWH

Antialiased filled rounded rectangle with border.

### ZuneDrawRectangleRoundedOutlineAA / ZuneDrawRectangleRoundedOutlineAAXYWH

Antialiased outlined rounded rectangle.

### ZuneDrawRectangleRoundedOutlineStyledAA / ZuneDrawRectangleRoundedOutlineStyledAAXYWH

Antialiased outlined rounded rectangle with custom line width.

---

## Drawing - Circles

All circle functions use `struct ZunePoint {WORD x, y}` for the center.
Convenience macros with `At` suffix accept individual coordinates.

### ZuneFillCircle / ZuneDrawCircleAt

Filled circle.

### ZuneDrawCircleOutline / ZuneDrawCircleOutlineAt

1-pixel circle outline.

### ZuneDrawCircleOutlineStyled / ZuneDrawCircleOutlineStyledAt

Circle outline with custom line width.

---

## Drawing - Circles (Antialiased)

### ZuneFillCircleAA / ZuneFillCircleAAAt

Antialiased filled circle.

### ZuneDrawCircleOutlineStyledAA / ZuneDrawCircleOutlineStyledAAAt

Antialiased circle outline with custom line width.

### ZuneFillCircleStyledAA / ZuneFillCircleStyledAAAt

Antialiased filled circle with border.

### ZuneDrawCircleAA / ZuneDrawCircleAAAt

Antialiased circle outline (1-pixel).

### ZuneSetAntialiasingQuality(rctx, quality) / ZuneGetAntialiasingQuality(rctx)

Set/get the AA supersampling quality level for the CyberGfx backend.
Higher values produce smoother edges at the cost of performance.

---

## Drawing - Lines

### ZuneDrawLine / ZuneDrawLinePoints

1-pixel line.

### ZuneDrawLineStyled / ZuneDrawLineStyledPoints

Line with custom width.

### ZuneDrawLineAA / ZuneDrawLineAAPoints

Antialiased 1-pixel line.

### ZuneDrawLineStyledAA / ZuneDrawLineStyledAAPoints

Antialiased line with custom width.

---

## Drawing - Polylines and Polygons

### ZuneDrawPolyline(rctx, points, count, color)

Draws connected line segments through an array of points.
Draws from `points[0]` to `points[1]` to ... to `points[count-1]`.
Requires `count >= 2`.

### ZuneDrawPolylineStyled(rctx, points, count, lineWidth, color)

Polyline with custom line width.

### ZuneFillPolygon(rctx, points, count, brush)

Fills a closed polygon. The polygon is automatically closed (last
point connects to first). Requires `count >= 3`. Supports all brush
types (solid, gradient, texture).

```c
/* Filled star */
struct ZunePoint star[10];
for (int i = 0; i < 10; i++) {
    float angle = (float)(i * 36 - 90) * 3.14159f / 180.0f;
    int r = (i % 2 == 0) ? 50 : 20;
    star[i].x = cx + (WORD)(r * cosf(angle));
    star[i].y = cy + (WORD)(r * sinf(angle));
}
ZuneFillPolygon(rctx, star, 10, ZUNE_BRUSH_SOLID(ZUNE_YELLOW));
```

---

## Drawing - Pixels

### ZuneDrawPixel / ZuneDrawPixelAt

Draws a single pixel. Uses the rendering backend (OpenGL or CyberGfx).

### ZuneGetPixel / GetPixelAt

Reads a pixel from the current render target. Returns ARGB32.

### ZuneSetPixel / SetPixelAt

Sets a pixel on the current render target. For DrawingBoards, this
writes to the underlying bitmap directly.

---

## Text Rendering

### ZuneSetFont(rctx, font)

Sets the current font for text operations. Accepts any `TextFont*`
including TrueType fonts loaded via `OpenDiskFont()`. The font is
stored on the RenderContext.

```c
ZuneSetFont(rctx, window->RPort->Font);
```

### ZuneTextLength(rctx, string, count)

Measures the pixel width of `count` characters from `string` using
the current font. Returns the width in pixels.

### ZuneTextFit(rctx, string, count, maxWidth)

Returns how many characters from `string` fit within `maxWidth`
pixels using the current font.

### ZuneDrawText / ZuneDrawTextAt

Draws text at the given position with transparent background (JAM1).
`position.y` is the TOP of the text line (not baseline); the library
adds `tf_Baseline` internally.

```c
ZuneDrawTextAt(rctx, 20, 50, "Hello, AROS!", 12, ZUNE_WHITE);
```

### ZuneDrawTextBackground / ZuneDrawTextBackgroundAt

Draws text with a solid background color (JAM2 mode). Useful for
selections and highlighted text.

```c
ZuneDrawTextBackgroundAt(rctx, 20, 50, "Selected", 8, ZUNE_WHITE, ZUNE_BLUE);
```

---

## Texture Management

### ZuneCreateTexture(rctx, width, height, depth, format, flags)

Creates an empty texture.

**Formats:**

| Constant | Value | Description |
|---|---|---|
| `ZUNE_TEXTURE_FORMAT_ARGB32` | 0 | 32-bit ARGB |
| `ZUNE_TEXTURE_FORMAT_RGB24` | 1 | 24-bit RGB |
| `ZUNE_TEXTURE_FORMAT_L8` | 4 | 8-bit luminance |
| `ZUNE_TEXTURE_FORMAT_A8` | 5 | 8-bit alpha |

**Flags:**

| Flag | Description |
|---|---|
| `ZUNE_TEXTURE_HARDWARE` | Prefer GPU storage |
| `ZUNE_TEXTURE_MIPMAPS` | Generate mipmaps |
| `ZUNE_TEXTURE_FILTERING` | Enable bilinear filtering |
| `ZUNE_TEXTURE_WRAPPING` | Enable texture wrapping (for tiling) |
| `ZUNE_TEXTURE_DYNAMIC` | Frequently updated |
| `ZUNE_TEXTURE_ALPHA` | Has alpha channel |
| `ZUNE_TEXTURE_OPAQUE` | All pixels fully opaque (optimization) |

### ZuneCreateTextureFromData(rctx, data, width, height, depth, format, pitch, flags)

Creates a texture from existing pixel data. The data is copied.

```c
ULONG pixels[64 * 64];
/* ... fill pixels ... */
tex = ZuneCreateTextureFromData(rctx, pixels, 64, 64, 32,
          ZUNE_TEXTURE_FORMAT_ARGB32, 64 * 4, ZUNE_TEXTURE_HARDWARE);
```

### ZuneCreateTextureFromFile(rctx, filename, screen, flags)

Loads a texture from an image file using datatypes. Supports any format
that has a datatype handler installed (PNG, JPEG, BMP, IFF ILBM, etc.).

```c
tex = ZuneCreateTextureFromFile(rctx, "PROGDIR:background.png",
          window->WScreen, ZUNE_TEXTURE_HARDWARE);
```

### ZuneCreateTextureFromDrawingBoard(rctx, flags)

Captures the current DrawingBoard into a texture.

### ZuneCreateTextureFromDatatype(rctx, dt_object, flags)

Creates a texture from a pre-opened DataType object.

### ZuneCreateTextureA(tags)

Unified tag-based creation. Available tags:

| Tag | Type | Description |
|---|---|---|
| `ZUNE_Texture_RenderContext` | `struct RenderContext *` | Optional |
| `ZUNE_Texture_Width` | `UWORD` | Required (unless using a source) |
| `ZUNE_Texture_Height` | `UWORD` | Required (unless using a source) |
| `ZUNE_Texture_Depth` | `UBYTE` | Default: 32 |
| `ZUNE_Texture_Format` | `ULONG` | Default: ARGB32 |
| `ZUNE_Texture_Flags` | `ULONG` | |
| `ZUNE_Texture_Data` | `APTR` | Pixel data |
| `ZUNE_Texture_Pitch` | `ULONG` | Required if Data is set |
| `ZUNE_Texture_SourceDrawingBoard` | `BOOL` | Use rctx's target_board |
| `ZUNE_Texture_SourceDatatype` | `APTR` | DataType object |
| `ZUNE_Texture_SourceFile` | `CONST_STRPTR` | Image file path |
| `ZUNE_Texture_Screen` | `struct Screen *` | For file loading |

### ZuneDestroyTexture(rctx, texture)

Frees a texture and its resources.

### ZuneIsTextureValid(texture)

Returns `TRUE` if the texture is valid and ready for use.

---

## Texture Data Operations

### ZuneUpdateTextureData(rctx, texture, data, rect)

Updates a rectangular region of the texture with new pixel data.

### ZuneLockTexturePixels(rctx, texture, pitch)

Locks the texture for direct pixel access. Returns a pointer to the
pixel data and sets `*pitch` to the bytes per row. Call
`ZuneUnlockTexturePixels()` when done.

### ZuneUnlockTexturePixels(rctx, texture)

Unlocks previously locked texture pixels.

### ZuneGetTexturePixel(rctx, texture, point)

Reads a single pixel from a texture. Returns ARGB32.

### ZuneSetTexturePixel(rctx, texture, point, color)

Writes a single pixel to a texture.

---

## Texture Rendering

### ZuneDrawTexture(rctx, texture, position)

Draws a texture at its original size at the given position.

### ZuneDrawTextureScaled(rctx, texture, dest_rect)

Draws a texture scaled to fit the destination rectangle.

### ZuneDrawTextureRegion(rctx, texture, src_rect, dest_rect)

Draws a sub-region of a texture scaled to the destination rectangle.

### ZuneDrawTextureTinted(rctx, texture, position, tint_color)

Draws a texture with a color tint applied.

### ZuneDrawTextureScaledTinted(rctx, texture, dest_rect, tint_color)

Draws a scaled texture with a color tint.

### ZuneDrawTextureRegionTinted(rctx, texture, src_rect, dest_rect, tint_color)

Draws a sub-region of a texture with a color tint.

### ZuneDrawTextureTiled(rctx, texture, dest_rect)

Tiles (repeats) a texture to fill the destination rectangle.

---

## Clipping

### ZuneSetClipRegion(rctx, region)

Installs a clipping region on the RenderContext. All subsequent drawing
operations are clipped to this region.

The region is copied internally. The caller retains ownership and must
dispose it after this call.

**Implementation details:**

| Target | Mechanism |
|---|---|
| CyberGfx / RastPort | `InstallClipRegion()` on the Layer |
| OpenGL | `glScissor()` with the region's bounding box |
| CyberGfx / DrawingBoard | No clipping (no-op) |

Returns `TRUE` on success.

### ZuneClearClipRegion(rctx)

Removes the active clipping region. All subsequent drawing operations
are unclipped. Restores the previous Layer clip region if one was saved.

### ZuneCreateCircleRegion(center, radius) / ZuneCreateCircleRegionAt(cx, cy, radius)

Creates a Region approximating a circle, built from horizontal scanline
rectangles. The caller must `DisposeRegion()` when done.

### ZuneCreateRoundedRectRegion(rect, corner_radius) / ZuneCreateRoundedRectRegionRect(x, y, w, h, radius)

Creates a Region for a rounded rectangle. Pass `corner_radius=0` for a
plain rectangle. The caller must `DisposeRegion()` when done.

### ZuneCombineRegions(r1, r2, operation)

Combines two regions using a boolean operation. Returns a new Region.

**Example - clip to a circle:**
```c
struct Region *circle = ZuneCreateCircleRegionAt(120, 120, 60);
ZuneSetClipRegion(rctx, circle);
/* ... draw ... */
ZuneClearClipRegion(rctx);
DisposeRegion(circle);
```

**Example - combine circle and rectangle:**
```c
struct Region *circle = ZuneCreateCircleRegionAt(100, 100, 50);
struct Region *rect = ZuneCreateRoundedRectRegionRect(80, 80, 120, 60, 0);
OrRegionRegion(rect, circle); /* Merge rect into circle */
ZuneSetClipRegion(rctx, circle);
/* ... draw ... */
ZuneClearClipRegion(rctx);
DisposeRegion(circle);
DisposeRegion(rect);
```

---

## Blitting and Presentation

### ZunePresent(rctx, src_x, src_y, dst_x, dst_y, width, height)

Presents DrawingBoard content to the window. This is the primary
function for double-buffered rendering. Automatically syncs the
OpenGL FBO to bitmap before blitting.

```c
ZunePresent(rctx, 0, 0,
            window->BorderLeft, window->BorderTop,
            board->width, board->height);
```

### ZuneBlit(src_rctx, dst_rctx, src_x, src_y, dst_x, dst_y, width, height)

General-purpose blit between RenderContexts. Handles all combinations:
DrawingBoard to DrawingBoard, DrawingBoard to screen, etc.

When both source and destination are FBO-backed DrawingBoards, this
uses a zero-copy GPU path.

### ZuneCapture(rctx, src_rp, src_x, src_y, dst_x, dst_y, width, height)

Captures pixels from a RastPort into the current DrawingBoard.
Used to read background content for proper alpha blending.

---

## Synchronization

When using the OpenGL backend, the DrawingBoard has both a BitMap and an
FBO. These must be synchronized when mixing ZuneGfx and legacy drawing.

### ZuneSync(rctx)

Copies the FBO contents to the DrawingBoard's BitMap. Call this after
ZuneGfx drawing and before direct bitmap operations (`BltBitMapRastPort`,
`FillPixelArray`, etc.). No-op for CyberGfx backend.

### ZuneReload(rctx)

Copies the BitMap contents back to the FBO. Call this after direct
bitmap operations and before resuming ZuneGfx drawing. This is the
inverse of `ZuneSync()`. No-op for CyberGfx backend.

```c
/* ZuneGfx drawing */
ZuneFillRectangleRoundedAAXYWH(rctx, 50, 30, 100, 80, 15,
                               ZUNE_BRUSH_SOLID(ZUNE_RED));

/* Switch to legacy drawing */
ZuneSync(rctx);
FillPixelArray(board->rastport, 200, 40, 100, 60, 0xFFFFFF00);

/* Switch back to ZuneGfx drawing */
ZuneReload(rctx);
ZuneFillRectangleRoundedAAXYWH(rctx, 350, 30, 100, 80, 15,
                               ZUNE_BRUSH_SOLID(ZUNE_CYAN));
```

---

## Batching

### ZuneBeginBatch(rctx)

Begins batching mode. Drawing commands are deferred and the
framebuffer is not flushed until `ZuneEndBatch()` or `ZuneFlushBatch()`.
Reduces overhead from frequent buffer swaps.

### ZuneEndBatch(rctx)

Ends batching mode and flushes all pending operations.

### ZuneFlushBatch(rctx)

Flushes pending operations without ending batch mode. Useful for
mid-batch synchronization points.

### ZuneIsBatchingEnabled(rctx)

Returns `TRUE` if batching is currently active.

### ZuneGetBatchCount(rctx)

Returns the number of batched operations pending.

---

## Direct Pixel Access

### ZuneLockDrawingBoardPixels(rctx, pitch)

Locks the current DrawingBoard for direct pixel access. Returns a
pointer to the pixel buffer and sets `*pitch` to bytes per row.
Returns `NULL` if locking fails.

While locked, you can still call ZuneGfx drawing functions -- they
will operate on the locked buffer.

### ZuneUnlockDrawingBoardPixels(rctx)

Unlocks the DrawingBoard after direct pixel access.

---

## Color Utilities

### ZuneRGBToColor(r, g, b)

Creates an opaque ARGB32 color from R, G, B components (0-255).

### ZuneARGBToColor(a, r, g, b)

Creates an ARGB32 color from A, R, G, B components (0-255).

### ZuneBlendColors(color1, color2, alpha)

Blends two ARGB32 colors. `alpha=0` returns color1, `alpha=255` returns
color2. Values in between produce a linear interpolation.

---

## Color Macros

| Macro | Description |
|---|---|
| `ZUNE_COLOR_ARGB32(a, r, g, b)` | Build ARGB32 from components |
| `ZUNE_COLOR_RGB24(r, g, b)` | Build opaque ARGB32 |
| `ZUNE_COLOR_TRANSPARENT` | Fully transparent black |
| `ZUNE_GET_ALPHA(c)` | Extract alpha component |
| `ZUNE_GET_RED(c)` | Extract red component |
| `ZUNE_GET_GREEN(c)` | Extract green component |
| `ZUNE_GET_BLUE(c)` | Extract blue component |

**Predefined colors:**
`ZUNE_BLACK`, `ZUNE_WHITE`, `ZUNE_RED`, `ZUNE_GREEN`, `ZUNE_BLUE`,
`ZUNE_YELLOW`, `ZUNE_MAGENTA`, `ZUNE_CYAN`, `ZUNE_GRAY`,
`ZUNE_LIGHTGRAY`, `ZUNE_DARKGRAY`

---

## Brush System

`struct ZuneBrush` describes fill styles for shapes. The `type` field selects
the fill mode, and the `data` union holds type-specific parameters.

### Brush Types

**`ZUNE_BRUSH_TYPE_SOLID`** - Solid color fill.
- `data.solid.color` = ARGB32 color value.
- Use the `ZUNE_BRUSH_SOLID(color)` macro for inline usage.

**`ZUNE_BRUSH_TYPE_LINEAR_GRADIENT`** - Linear gradient between two points with multiple color stops.
- `data.linear.start`, `.end` = gradient axis endpoints (ZunePoint).
- `data.linear.stops` = array of `ZuneGradientStop`.
- `data.linear.stop_count` = number of stops.

**`ZUNE_BRUSH_TYPE_RADIAL_GRADIENT`** - Radial gradient from a center point.
- `data.radial.center` = center point.
- `data.radial.radius` = gradient radius.
- `data.radial.stops` = array of `ZuneGradientStop`.
- `data.radial.stop_count` = number of stops.

**`ZUNE_BRUSH_TYPE_TEXTURE`** - Texture fill with wrapping and filtering options.
- `data.texture.texture` = ZuneTexture pointer.
- `data.texture.source` = source rectangle within the texture.
- `data.texture.wrap_u`, `.wrap_v` = `ZUNE_BRUSH_WRAP_CLAMP` / `REPEAT` / `MIRROR`.
- `data.texture.filter` = `ZUNE_BRUSH_FILTER_NEAREST` / `LINEAR`.

**`ZUNE_BRUSH_TYPE_PEN`** - Amiga pen-based color (for legacy Zune compatibility).
- `data.pen.pen` = pen index.
- Use the `ZUNE_BRUSH_PEN(pen)` macro.

**`ZUNE_BRUSH_TYPE_PATTERN`** - 2-row 16-bit pattern with foreground/background pens.
- `data.pattern.fg_pen`, `.bg_pen` = pen indices.
- `data.pattern.pattern` = pointer to 2 UWORD pattern rows.
- `data.pattern.colormap` = ColorMap for pen-to-RGB conversion.

**`ZUNE_BRUSH_TYPE_DATATYPE`** - DataTypes-backed texture fill.
- `data.datatype.texture` = ZuneTexture created from datatype.

### Convenience Macros

| Macro | Description |
|---|---|
| `ZUNE_BRUSH_SOLID(color)` | Pointer to a solid brush (for function args) |
| `ZUNE_BRUSH_PEN(pen)` | Pointer to a pen brush |
| `ZUNE_BRUSH_LITERAL_SOLID(c)` | Solid brush as a value (for assignment) |
| `ZUNE_BRUSH_LITERAL_PEN(p)` | Pen brush as a value |

### Gradient Stop

```c
struct ZuneGradientStop {
    float position;  /* 0.0 to 1.0 */
    ULONG color;     /* ARGB32 */
};
```

---

## OpenGL Context Sharing

### ZuneGetMasterGLContext()

Returns the master GL context for sharing with other components
(e.g., a compositor). Other components can pass this context as
`GLA_ShareContext` when creating their own GL contexts to share
textures and other resources.

Returns `NULL` if no master context has been created yet.

---

## Cache Utilities

### ZuneInitPenCache(rctx, pens, count)

Initializes a pen cache with pre-allocated pens for optimized
pen-to-color lookups.

---

## Backend Selection

The library supports multiple rendering backends:

**CyberGraphics (`BACKEND_CYBERGFX`)** - CPU-based rendering using CyberGraphics
functions (`FillPixelArray`, `WritePixelArray`, etc.) and direct pixel buffer
access. Works on all CyberGraphics-capable screens. For antialiased operations,
uses supersampling on locked pixel buffers.

**OpenGL (`BACKEND_OPENGL`)** - GPU-accelerated rendering using AROS mesa3dgl.
Requires a Window for GL context creation. Uses FBOs for off-screen
DrawingBoards and GLSL shaders for antialiased rounded rectangles. Falls back
to CyberGraphics for operations that cannot be GPU-accelerated.

**`BACKEND_BEST_AVAILABLE`** - Automatically selects OpenGL if gl.library is
available and the target has a Window, otherwise CyberGraphics.

The backend is selected at RenderContext creation time and remains fixed
for the lifetime of that RenderContext.
