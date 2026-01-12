# ZuneRenderer Tiled Texture Examples

This document demonstrates how to create tiled texture backgrounds using the ZuneRenderer library.

## Overview

The ZuneRenderer library now includes a `ZuneDrawTextureTiled()` function that allows you to easily tile a texture across any rectangular area, including the entire screen background.

## Function Signature

```c
void ZuneDrawTextureTiled(struct RenderPort *rp, struct ZuneTexture *texture, struct ZuneRect *dest_rect);
```

**Parameters:**
- `rp` - The RenderPort to draw on
- `texture` - The texture to tile
- `dest_rect` - The rectangular area to fill with tiled texture

## Basic Tiled Background Example

Here's a complete example of how to tile a texture across the entire background:

```c
void CreateTiledBackground(struct RenderPort *rp)
{
    /* Create a small tile texture (32x32 checkerboard) */
    ULONG tile_width = 32;
    ULONG tile_height = 32;
    ULONG data_size = tile_width * tile_height * 4; /* ARGB32 format */
    
    APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);
    if (pixel_data) {
        ULONG *pixels = (ULONG *)pixel_data;
        ULONG i, j;

        /* Create checkerboard pattern */
        for (i = 0; i < tile_height; i++) {
            for (j = 0; j < tile_width; j++) {
                BOOL checker = ((i / 8) + (j / 8)) % 2;
                ULONG color;
                
                if (checker) {
                    color = ZUNE_COLOR_ARGB32(255, 100, 150, 200); /* Light blue */
                } else {
                    color = ZUNE_COLOR_ARGB32(255, 200, 100, 150); /* Light pink */
                }
                
                pixels[i * tile_width + j] = color;
            }
        }

        /* Create texture from pixel data */
        ULONG pitch = tile_width * 4;
        struct ZuneTexture *tile_texture = CreateTextureFromData(
            pixel_data, tile_width, tile_height, 32,
            ZUNE_TEXTURE_FORMAT_ARGB32, pitch, ZUNE_TEXTURE_HARDWARE
        );

        if (tile_texture) {
            /* Tile across entire screen background */
            struct ZuneRect background_rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            ZuneDrawTextureTiled(rp, tile_texture, &background_rect);

            /* Clean up */
            DestroyTexture(tile_texture);
        }

        FreeMem(pixel_data, data_size);
    }
}
```

## Different Tiling Patterns

### Brick Pattern
```c
struct ZuneTexture *CreateBrickTexture(UWORD width, UWORD height)
{
    ULONG data_size = width * height * 4;
    APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);
    
    if (!pixel_data) return NULL;

    ULONG *pixels = (ULONG *)pixel_data;
    ULONG brick_color = ZUNE_COLOR_ARGB32(255, 180, 100, 60);   /* Brick red */
    ULONG mortar_color = ZUNE_COLOR_ARGB32(255, 200, 200, 200); /* Gray mortar */

    for (UWORD i = 0; i < height; i++) {
        for (UWORD j = 0; j < width; j++) {
            BOOL is_mortar = FALSE;
            
            /* Horizontal mortar lines */
            if (i % (height/2) <= 1) {
                is_mortar = TRUE;
            }
            
            /* Vertical mortar lines with brick offset */
            UWORD offset = (i / (height/2)) % 2 ? width/4 : 0;
            UWORD adjusted_j = (j + offset) % width;
            if (adjusted_j % (width/2) <= 1) {
                is_mortar = TRUE;
            }

            pixels[i * width + j] = is_mortar ? mortar_color : brick_color;
        }
    }

    ULONG pitch = width * 4;
    struct ZuneTexture *texture = CreateTextureFromData(
        pixel_data, width, height, 32, ZUNE_TEXTURE_FORMAT_ARGB32, 
        pitch, ZUNE_TEXTURE_HARDWARE
    );

    FreeMem(pixel_data, data_size);
    return texture;
}
```

### Usage with Brick Pattern
```c
void CreateBrickBackground(struct RenderPort *rp)
{
    struct ZuneTexture *brick_texture = CreateBrickTexture(64, 32);
    
    if (brick_texture) {
        /* Tile across entire background */
        struct ZuneRect full_screen = {0, 0, DEMO_WIDTH, DEMO_HEIGHT};
        ZuneDrawTextureTiled(rp, brick_texture, &full_screen);
        
        DestroyTexture(brick_texture);
    }
}
```

## Partial Area Tiling

You can also tile textures in specific rectangular areas:

```c
void CreatePartialTiledAreas(struct RenderPort *rp, struct ZuneTexture *texture)
{
    /* Define multiple areas to tile */
    struct ZuneRect areas[] = {
        {50, 50, 200, 150},    /* Top-left area */
        {300, 80, 250, 120},   /* Top-right area */
        {100, 250, 180, 100},  /* Bottom-left area */
        {350, 280, 200, 80}    /* Bottom-right area */
    };

    /* Tile the texture in each area */
    for (int i = 0; i < 4; i++) {
        ZuneDrawTextureTiled(rp, texture, &areas[i]);
        
        /* Add border around each tiled area */
        ZuneDrawRectangleOutline(rp, 
            areas[i].x - 2, areas[i].y - 2,
            areas[i].width + 4, areas[i].height + 4, 
            ZUNE_BLACK
        );
    }
}
```

## Multi-Texture Tiling

Create different sections with different tiled textures:

```c
void CreateMultiTextureSections(struct RenderPort *rp)
{
    struct ZuneTexture *texture1 = CreateCheckerboardTexture(16);
    struct ZuneTexture *texture2 = CreateBrickTexture(32, 16);
    struct ZuneTexture *texture3 = CreateDotPatternTexture(24);

    if (texture1 && texture2 && texture3) {
        UWORD screen_width = DEMO_WIDTH;
        UWORD section_width = screen_width / 3;

        /* Left section */
        struct ZuneRect left = {0, 0, section_width, DEMO_HEIGHT};
        ZuneDrawTextureTiled(rp, texture1, &left);

        /* Middle section */
        struct ZuneRect middle = {section_width, 0, section_width, DEMO_HEIGHT};
        ZuneDrawTextureTiled(rp, texture2, &middle);

        /* Right section */
        struct ZuneRect right = {2 * section_width, 0, section_width, DEMO_HEIGHT};
        ZuneDrawTextureTiled(rp, texture3, &right);

        /* Add section dividers */
        ZuneDrawLine(rp, section_width, 0, section_width, DEMO_HEIGHT, ZUNE_WHITE);
        ZuneDrawLine(rp, 2 * section_width, 0, 2 * section_width, DEMO_HEIGHT, ZUNE_WHITE);
    }

    /* Clean up */
    if (texture1) DestroyTexture(texture1);
    if (texture2) DestroyTexture(texture2);
    if (texture3) DestroyTexture(texture3);
}
```

## Manual Tiling (Alternative Method)

If you prefer more control, you can also tile manually using loops:

```c
void ManualTileTexture(struct RenderPort *rp, struct ZuneTexture *texture, 
                      UWORD dest_x, UWORD dest_y, UWORD dest_width, UWORD dest_height)
{
    UWORD texture_width = texture->width;
    UWORD texture_height = texture->height;
    
    /* Calculate how many tiles we need */
    UWORD tiles_x = (dest_width + texture_width - 1) / texture_width;
    UWORD tiles_y = (dest_height + texture_height - 1) / texture_height;
    
    /* Draw tiles row by row */
    for (UWORD ty = 0; ty < tiles_y; ty++) {
        for (UWORD tx = 0; tx < tiles_x; tx++) {
            struct ZunePoint pos;
            pos.x = dest_x + tx * texture_width;
            pos.y = dest_y + ty * texture_height;
            
            /* Only draw if tile is at least partially visible */
            if (pos.x < dest_x + dest_width && pos.y < dest_y + dest_height) {
                ZuneDrawTexture(rp, texture, &pos);
            }
        }
    }
}
```

## Integration in Your Demo

The tiled texture functionality has been integrated into `rect_demo.c`. To see it in action:

1. Compile the ZuneRenderer library and examples
2. Run `rect_demo`
3. Navigate to "Demo 14: Testing tiled texture rendering..."

## Key Benefits

1. **Efficient**: `ZuneDrawTextureTiled()` handles clipping and optimization automatically
2. **Flexible**: Works with any texture size and destination rectangle
3. **Seamless**: Properly handles partial tiles at edges
4. **Compatible**: Works with all existing texture formats and flags

## Tips for Best Results

1. **Texture Size**: Smaller textures (16x16 to 64x64) usually work best for tiling
2. **Seamless Patterns**: Design your textures to tile seamlessly at the edges
3. **Performance**: Use `ZUNE_TEXTURE_HARDWARE` flag for better performance when possible
4. **Memory**: Remember to free pixel data after creating textures
5. **Clipping**: The function automatically handles clipping at destination boundaries

## Texture Pattern Ideas

- Checkerboard patterns for testing
- Brick/stone textures for architectural themes
- Fabric/carpet textures for UI backgrounds
- Circuit board patterns for technical interfaces
- Wood grain for natural themes
- Abstract geometric patterns