# ZuneRenderer Library - Texture Functions Documentation

## Overview

The ZuneRenderer library now includes comprehensive texture management functionality, allowing you to create, manipulate, and render textures with various formats and operations. This document describes all available texture functions and their usage.

## Texture Structure

```c
struct ZuneTexture {
    struct Node node;           /* For linking in lists */
    
    /* Texture properties */
    UWORD width, height;        /* Texture dimensions */
    UBYTE depth;                /* Color depth in bits */
    ULONG format;               /* Pixel format */
    ULONG flags;                /* Creation flags */
    
    /* Data storage */
    APTR pixel_data;            /* Raw pixel data */
    ULONG data_size;            /* Size of pixel data in bytes */
    ULONG pitch;                /* Bytes per row */
    
    /* Direct pixel access */
    APTR lock_handle;           /* Lock handle for pixel access */
    BOOL pixels_locked;         /* Pixel buffer lock state */
    
    /* Backend integration */
    APTR backend_handle;        /* Backend-specific texture handle */
    BOOL hardware_texture;      /* TRUE if stored in video memory */
    ULONG backend_type;         /* Backend that owns this texture */
    
    /* State and management */
    BOOL valid;                 /* Texture is ready for use */
    ULONG ref_count;            /* Reference counting for cleanup */
};
```

## Texture Formats

The library supports several pixel formats:

- `ZUNE_TEXTURE_FORMAT_ARGB32` - 32-bit ARGB (8 bits per component)
- `ZUNE_TEXTURE_FORMAT_RGB24` - 24-bit RGB (8 bits per component, no alpha)
- `ZUNE_TEXTURE_FORMAT_ARGB16` - 16-bit ARGB (4 bits per component)
- `ZUNE_TEXTURE_FORMAT_RGB16` - 16-bit RGB (5-6-5 format)
- `ZUNE_TEXTURE_FORMAT_L8` - 8-bit luminance
- `ZUNE_TEXTURE_FORMAT_A8` - 8-bit alpha only

## Texture Creation Flags

- `ZUNE_TEXTURE_HARDWARE` - Store texture in video memory if available
- `ZUNE_TEXTURE_MIPMAPS` - Generate mipmaps for the texture
- `ZUNE_TEXTURE_FILTERING` - Enable texture filtering
- `ZUNE_TEXTURE_WRAPPING` - Enable texture coordinate wrapping
- `ZUNE_TEXTURE_DYNAMIC` - Texture data will be updated frequently
- `ZUNE_TEXTURE_ALPHA` - Texture contains alpha channel data

## Function Reference

### Texture Management

#### CreateTexture()
```c
struct ZuneTexture *CreateTexture(UWORD width, UWORD height, UBYTE depth, ULONG format, ULONG flags);
```
Creates a new texture with the specified dimensions and format.

**Parameters:**
- `width` - Texture width in pixels
- `height` - Texture height in pixels  
- `depth` - Color depth in bits
- `format` - Pixel format (ZUNE_TEXTURE_FORMAT_*)
- `flags` - Texture creation flags (ZUNE_TEXTURE_*)

**Returns:** Pointer to new ZuneTexture structure, or NULL if creation failed.

**Example:**
```c
struct ZuneTexture *texture = CreateTexture(256, 256, 32, ZUNE_TEXTURE_FORMAT_ARGB32, ZUNE_TEXTURE_HARDWARE);
```

#### CreateTextureFromData()
```c
struct ZuneTexture *CreateTextureFromData(APTR data, UWORD width, UWORD height, UBYTE depth, ULONG format, ULONG pitch, ULONG flags);
```
Creates a new texture from existing pixel data.

**Parameters:**
- `data` - Pointer to source pixel data
- `width` - Texture width in pixels
- `height` - Texture height in pixels
- `depth` - Color depth in bits
- `format` - Pixel format
- `pitch` - Bytes per row in source data
- `flags` - Texture creation flags

**Returns:** Pointer to new ZuneTexture structure, or NULL if creation failed.

#### CreateTextureFromDrawingBoard()
```c
struct ZuneTexture *CreateTextureFromDrawingBoard(struct DrawingBoard *board, ULONG flags);
```
Creates a new texture from a DrawingBoard's pixel data.

**Parameters:**
- `board` - Source DrawingBoard (must not be NULL)
- `flags` - Texture creation flags

**Returns:** Pointer to new ZuneTexture structure, or NULL if creation failed.

#### DestroyTexture()
```c
void DestroyTexture(struct ZuneTexture *texture);
```
Destroys a texture and frees all associated resources.

**Parameters:**
- `texture` - Texture to destroy (may be NULL)

### Texture Data Operations

#### UpdateTextureData()
```c
BOOL UpdateTextureData(struct ZuneTexture *texture, APTR data, struct ZuneRect *rect);
```
Updates a rectangular region of texture data.

**Parameters:**
- `texture` - Target texture
- `data` - Source pixel data
- `rect` - Update region rectangle

**Returns:** TRUE if update succeeded, FALSE otherwise.

#### LockTexturePixels()
```c
APTR LockTexturePixels(struct ZuneTexture *texture, ULONG *pitch);
```
Locks texture pixels for direct access.

**Parameters:**
- `texture` - Texture to lock
- `pitch` - Pointer to store pitch value (may be NULL)

**Returns:** Pointer to pixel data, or NULL if locking failed.

**Note:** Always call UnlockTexturePixels() when finished with direct access.

#### UnlockTexturePixels()
```c
void UnlockTexturePixels(struct ZuneTexture *texture);
```
Unlocks texture pixels previously locked with LockTexturePixels().

#### GetTexturePixel()
```c
ULONG GetTexturePixel(struct ZuneTexture *texture, struct ZunePoint *point);
```
Gets the color value of a pixel in the texture.

**Parameters:**
- `texture` - Source texture
- `point` - Pixel coordinates

**Returns:** Color value in ARGB format, or 0 if coordinates are invalid.

#### SetTexturePixel()
```c
void SetTexturePixel(struct ZuneTexture *texture, struct ZunePoint *point, ULONG color);
```
Sets the color value of a pixel in the texture.

**Parameters:**
- `texture` - Target texture
- `point` - Pixel coordinates
- `color` - Color value in ARGB format

### Texture Rendering

#### ZuneDrawTexture()
```c
void ZuneDrawTexture(struct RenderPort *rp, struct ZuneTexture *texture, struct ZunePoint *position);
```
Draws a texture at the specified position at its original size.

**Parameters:**
- `rp` - Target RenderPort
- `texture` - Source texture
- `position` - Destination coordinates

#### ZuneDrawTextureScaled()
```c
void ZuneDrawTextureScaled(struct RenderPort *rp, struct ZuneTexture *texture, struct ZuneRect *dest_rect);
```
Draws a texture scaled to the specified dimensions.

**Parameters:**
- `rp` - Target RenderPort
- `texture` - Source texture
- `dest_rect` - Destination rectangle

#### ZuneDrawTextureRegion()
```c
void ZuneDrawTextureRegion(struct RenderPort *rp, struct ZuneTexture *texture, struct ZuneRect *src_rect, struct ZuneRect *dest_rect);
```
Draws a region of a texture with scaling. This allows you to draw only part of a texture and scale it to fit the destination area.

**Parameters:**
- `rp` - Target RenderPort
- `texture` - Source texture
- `src_rect` - Source region rectangle
- `dest_rect` - Destination rectangle

#### ZuneDrawTextureTinted()
```c
void ZuneDrawTextureTinted(struct RenderPort *rp, struct ZuneTexture *texture, struct ZunePoint *position, ULONG tint_color);
```
Draws a texture with color tinting applied.

**Parameters:**
- `rp` - Target RenderPort
- `texture` - Source texture
- `position` - Destination coordinates
- `tint_color` - Tint color in ARGB format

#### ZuneDrawTextureScaledTinted()
```c
void ZuneDrawTextureScaledTinted(struct RenderPort *rp, struct ZuneTexture *texture, struct ZuneRect *dest_rect, ULONG tint_color);
```
Draws a texture scaled with color tinting.

**Parameters:**
- `rp` - Target RenderPort
- `texture` - Source texture
- `dest_rect` - Destination rectangle
- `tint_color` - Tint color in ARGB format

#### ZuneDrawTextureRegionTinted()
```c
void ZuneDrawTextureRegionTinted(struct RenderPort *rp, struct ZuneTexture *texture, struct ZuneRect *src_rect, struct ZuneRect *dest_rect, ULONG tint_color);
```
Draws a region of a texture with scaling and color tinting.

**Parameters:**
- `rp` - Target RenderPort
- `texture` - Source texture
- `src_rect` - Source region rectangle
- `dest_rect` - Destination rectangle
- `tint_color` - Tint color in ARGB format

### Texture Utility Functions

#### IsTextureValid()
```c
BOOL IsTextureValid(struct ZuneTexture *texture);
```
Checks if a texture is valid and ready for use.

#### GetTextureInfo()
```c
void GetTextureInfo(struct ZuneTexture *texture, UWORD *width, UWORD *height, UBYTE *depth, ULONG *format);
```
Gets information about a texture's properties. Pass NULL for any parameter you don't need.

#### GetTextureSizeInBytes()
```c
ULONG GetTextureSizeInBytes(UWORD width, UWORD height, ULONG format);
```
Calculates the size in bytes for a texture with given parameters.

#### GetTextureFormatBPP()
```c
ULONG GetTextureFormatBPP(ULONG format);
```
Gets the bits per pixel for a texture format.

## Usage Examples

### Basic Texture Creation and Usage
```c
#include <proto/zunerenderer.h>

struct Library *ZuneRendererBase;
struct ZuneTexture *texture;

/* Open library */
ZuneRendererBase = OpenLibrary("zunerenderer.library", 1);

/* Create a 64x64 ARGB texture */
texture = CreateTexture(64, 64, 32, 0, 0); /* Use default format */

/* Draw some pixels */
struct ZunePoint pixel1 = {10, 10};
struct ZunePoint pixel2 = {20, 20};
SetTexturePixel(texture, &pixel1, 0xFFFF0000); /* Red pixel */
SetTexturePixel(texture, &pixel2, 0xFF00FF00); /* Green pixel */

/* Render the texture */
struct ZunePoint position = {100, 100};
ZuneDrawTexture(rp, texture, &position);

/* Cleanup */
DestroyTexture(texture);
CloseLibrary(ZuneRendererBase);
```

### Creating Texture from Data
```c
ULONG pixel_data[16*16]; /* 16x16 texture data */
struct ZuneTexture *texture;

/* Fill with gradient */
for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
        UBYTE intensity = (x * 16) + (y * 16);
        pixel_data[y * 16 + x] = 0xFF000000 | (intensity << 16) | (intensity << 8) | intensity;
    }
}

/* Create texture from data */
texture = CreateTextureFromData(pixel_data, 16, 16, 32, 
                               0, /* Use default format */ 
                               16 * sizeof(ULONG), 0);
```

### Direct Pixel Access
```c
APTR pixels;
ULONG pitch;
ULONG *pixel_ptr;

/* Lock pixels for direct access */
pixels = LockTexturePixels(texture, &pitch);
if (pixels) {
    /* Direct manipulation */
    pixel_ptr = (ULONG *)pixels;
    for (int i = 0; i < 64*64; i++) {
        pixel_ptr[i] = 0xFF808080; /* Gray */
    }
    
    /* Always unlock when done */
    UnlockTexturePixels(texture);
}
```

### Texture Rendering with Effects
```c
/* Draw texture normally */
struct ZunePoint pos1 = {0, 0};
ZuneDrawTexture(rp, texture, &pos1);

/* Draw scaled */
struct ZuneRect scaled_rect = {100, 0, 128, 128};
ZuneDrawTextureScaled(rp, texture, &scaled_rect);

/* Draw with red tint */
struct ZunePoint pos2 = {0, 100};
ZuneDrawTextureTinted(rp, texture, &pos2, 0x80FF0000);

/* Draw a region scaled with blue tint */
struct ZuneRect src_region = {16, 16, 32, 32};    /* Source region */
struct ZuneRect dest_region = {200, 100, 64, 64}; /* Destination */
ZuneDrawTextureRegionTinted(rp, texture, &src_region, &dest_region, 0x800000FF);
```

## Notes and Best Practices

1. **Memory Management**: Always call `DestroyTexture()` for every created texture to avoid memory leaks.

2. **Pixel Locking**: When using `LockTexturePixels()`, always call `UnlockTexturePixels()` when finished. Do not call other texture functions while pixels are locked.

3. **Format Support**: While all formats are defined, actual support may depend on the backend being used.

4. **Performance**: Hardware textures (`ZUNE_TEXTURE_HARDWARE` flag) may provide better performance for frequently drawn textures.

5. **Thread Safety**: The texture functions are not inherently thread-safe. Use appropriate synchronization if accessing textures from multiple threads.

6. **Coordinate Systems**: All coordinates use the standard AROS coordinate system with (0,0) at the top-left corner.

7. **Color Format**: Colors are specified in ARGB format (0xAARRGGBB) where AA is alpha, RR is red, GG is green, and BB is blue.

## Implementation Details

The texture functions are implemented in `zunerenderer_texture.c` and use AROS library macros for proper integration with the AROS system. The implementation includes:

- Automatic memory management with reference counting
- Support for multiple pixel formats with proper conversion
- Integration with the existing RenderPort and backend system
- Proper error handling and validation
- Efficient pixel-level operations with bounds checking

All functions follow AROS conventions and are fully compatible with the existing ZuneRenderer architecture.