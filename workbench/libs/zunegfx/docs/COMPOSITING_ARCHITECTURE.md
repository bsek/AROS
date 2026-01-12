# AROS Layer Compositing Architecture

This document describes a proposed architecture for adding transparent window support to AROS through hardware-accelerated compositing.

## Overview

The goal is to enable true window transparency where windows can have alpha channels and be blended with windows behind them. This requires changes at multiple levels of the graphics stack, but is designed to minimize changes to ROM while keeping the complex compositing logic in loadable components.

## Current Limitations

The existing AROS layer system assumes opaque windows:
- When a layer covers another, the covered portion is completely hidden
- `ClipRects` track visible vs hidden areas
- `BltBitMap()` uses standard copy (minterm `0x0c0`), no blending
- `HIDD_BM_PutAlphaImage()` exists but is software-only in most drivers

AROS does have a screen-level compositor (`workbench/devs/monitors/Compositor/`) that supports `COMPF_ALPHA`, but this operates on entire screens, not individual windows.

## Proposed Architecture

### Design Principles

1. **Minimal ROM changes** - Only essential hooks and data structures in ROM
2. **Hardware acceleration** - Use OpenGL for compositing performance
3. **Backward compatible** - Non-alpha windows work exactly as before
4. **Graceful fallback** - System works without compositor (alpha windows become opaque)

### Component Overview

```
+------------------+     +------------------+     +----------------------+
|   Applications   |     |    Wanderer      |     | Standard Intuition   |
| (Zune/MUI apps)  |     | (Desktop with    |     | Apps (work as before)|
|                  |     |  transparency)   |     |                      |
+--------+---------+     +--------+---------+     +-----------+----------+
         |                        |                           |
         v                        v                           v
+-------------------------------------------------------------------------+
|                          MUIMASTER.LIBRARY                               |
|  +-------------------------------------------------------------------+  |
|  | MUI_RenderInfo                                                     |  |
|  | - mri_RenderPort (zunerenderer)                                   |  |
|  | - mri_DrawingBoard (ARGB for alpha windows)                       |  |
|  | - mri_Window                                                       |  |
|  +-------------------------------------------------------------------+  |
|  NEW: MUIA_Window_Alpha (BOOL), MUIA_Window_AlphaValue (0-255)         |
+--------+----------------------------------------------------------------+
         |
         v
+------------------+          +-----------------------------------------------+
| ZUNERENDERER     |          |                  INTUITION                     |
| .LIBRARY         |          +-----------------------------------------------+
+------------------+          | OpenWindow() / OpenWindowTags()                |
| - RenderPort     |          | NEW TAGS:                                      |
| - DrawingBoard   |          |   - WA_Alpha (BOOL)                           |
| - OpenGL Backend |          |   - WA_AlphaValue (UBYTE 0-255)               |
| - FBO support    |          |   - WA_AlphaRegion (Region*)                  |
+--------+---------+          |                                                |
         |                    | Propagates to LA_Alpha when creating layer    |
         |                    +---------------------+-------------------------+
         |                                          |
         v                                          v
+-------------------------------------------------------------------------+
|                            HYPERLAYERS (ROM)                             |
+-------------------------------------------------------------------------+
| MINIMAL CHANGES:                                                         |
|                                                                          |
| New tags in CreateLayerTagList():                                        |
|   - LA_Alpha (BOOL)                                                     |
|   - LA_AlphaValue (UBYTE)                                               |
|   - LA_AlphaRegion (Region*)                                            |
|                                                                          |
| Extended struct IntLayer:                                                |
|   - UBYTE il_Alpha (0-255, 255=opaque)                                  |
|   - UBYTE il_AlphaFlags (LAYERF_ALPHA, etc.)                            |
|                                                                          |
| Extended Layer_Info:                                                     |
|   - struct Hook *li_CompositorHook (external compositor callback)       |
|                                                                          |
| Modified _ShowPartsOfLayer():                                            |
|   - If LAYERF_ALPHA && li_CompositorHook: call hook                     |
|   - Else: existing opaque blit code                                     |
|                                                                          |
| ~100-150 lines of new code total                                         |
+--------------------------------+----------------------------------------+
                                 |
                                 | Hook call when alpha layer needs display
                                 v
+-------------------------------------------------------------------------+
|                    LAYER COMPOSITOR (Outside ROM)                        |
|                 workbench/devs/monitors/LayerCompositor/                 |
+-------------------------------------------------------------------------+
| Registers as compositor for Layer_Info                                   |
| Installs li_CompositorHook                                              |
|                                                                          |
| struct LayerCompositor:                                                  |
|   - Layer_Info *layer_info                                              |
|   - Screen *screen                                                      |
|   - GLuint compositor_fbo                                               |
|   - GLuint compositor_texture                                           |
|   - BitMap *composite_bitmap (ARGB)                                     |
|   - APTR gl_context                                                     |
|                                                                          |
| API:                                                                     |
|   - CreateLayerCompositor(Layer_Info*, Screen*)                         |
|   - DestroyLayerCompositor(compositor)                                  |
|   - CompositeRegion(compositor, damage_region)                          |
|   - BlendLayerToComposite(compositor, layer, rect)                      |
|                                                                          |
| Compositing loop (back to front):                                        |
|   1. For each layer overlapping damage region                           |
|   2. Upload layer bitmap to GL texture (if dirty)                       |
|   3. If LAYERF_ALPHA: enable blending, set global alpha                 |
|   4. Draw textured quad                                                 |
|   5. Blit compositor FBO to display                                     |
+--------------------------------+----------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------------------+
|                     GRAPHICS HIDD / OPENGL                               |
+-------------------------------------------------------------------------+
| OpenGL provides hardware-accelerated:                                    |
|   - Texture management (layer bitmaps as textures)                      |
|   - Framebuffer Objects (compositor render target)                      |
|   - Alpha blending (glBlendFunc)                                        |
|   - Efficient batched rendering                                         |
|                                                                          |
| Optional HIDD extensions:                                                |
|   - HIDD_Gfx_CompositeRect()                                            |
|   - HIDD_BM_GetGLTexture()                                              |
|   - OpenGLBitMap subclass with texture_id, fbo_id                       |
+-------------------------------------------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------------------+
|                          GPU HARDWARE                                    |
+-------------------------------------------------------------------------+
| Hardware alpha blending, texture sampling, fragment processing          |
+-------------------------------------------------------------------------+
```

## Data Flow: Transparent Window Rendering

```
1. APPLICATION DRAWS
   +------------------+
   | MUI Widget       |
   | MUIM_Draw()      |
   +--------+---------+
            |
            v
   Draws to mri->mri_RastPort (DrawingBoard bitmap)
   Using SetAPen(), RectFill(), or zunerenderer API
            |
            v
2. DRAWINGBOARD UPDATED
   +----------------------------------+
   | DrawingBoard->bitmap (ARGB)      |
   | Contains widget graphics + alpha |
   +--------+-------------------------+
            |
            v
3. SYNC TO WINDOW
   +----------------------------------+
   | ZuneBlitToWindow() / EndRefresh()|
   | Copy DrawingBoard to window      |
   | Mark layer as COMPOSITEDIRTY     |
   +--------+-------------------------+
            |
            v
4. LAYER SYSTEM TRIGGER
   +----------------------------------+
   | hyperlayers detects dirty layer  |
   | Calls _ShowPartsOfLayer()        |
   | -> Calls li_CompositorHook       |
   +--------+-------------------------+
            |
            v
5. COMPOSITOR RUNS (OpenGL)
   +------------------------------------------+
   | CompositeRegion(compositor, damage)      |
   |                                          |
   | For each layer (back to front):          |
   |   - Upload bitmap to GL texture          |
   |   - glEnable(GL_BLEND)                   |
   |   - glBlendFunc(SRC_ALPHA, ONE_MINUS...) |
   |   - Draw textured quad                   |
   +--------+---------------------------------+
            |
            v
6. BLIT TO DISPLAY
   +----------------------------------+
   | Copy compositor FBO to screen    |
   | HIDD_BM_UpdateRect()             |
   +----------------------------------+
```

## ROM Changes Detail

### hyperlayers/layers_intern.h

```c
/* New flags */
#define LAYERF_ALPHA           (1 << 8)  /* Layer has alpha channel */
#define LAYERF_COMPOSITEDIRTY  (1 << 9)  /* Needs recomposite */

/* Extended IntLayer structure */
struct IntLayer {
    struct Layer layer;
    /* ... existing fields ... */
    
    /* NEW fields for alpha support */
    UBYTE il_Alpha;           /* Global alpha 0-255, 255=opaque */
    UBYTE il_AlphaFlags;      /* LAYERF_ALPHA etc. */
    APTR  il_CompositorData;  /* Compositor-private data */
};

/* Extended Layer_Info */
struct LayerInfo_extra {
    /* ... existing fields ... */
    
    /* NEW: External compositor hook */
    struct Hook *lie_CompositorHook;
};
```

### hyperlayers/createlayertaglist.c

```c
/* In tag parsing loop, add cases: */
case LA_Alpha:
    IL(layer)->il_AlphaFlags |= LAYERF_ALPHA;
    break;
    
case LA_AlphaValue:
    IL(layer)->il_Alpha = (UBYTE)tag->ti_Data;
    break;
    
case LA_AlphaRegion:
    /* Store per-pixel alpha region if needed */
    break;
```

### hyperlayers/basicfuncs.c

```c
/* In _ShowPartsOfLayer() or equivalent */
void _ShowPartsOfLayer(struct Layer_Info *li, struct Layer *l, 
                       struct Region *region, struct LayersBase *LayersBase)
{
    /* Check for alpha layer with external compositor */
    if ((IL(l)->il_AlphaFlags & LAYERF_ALPHA) && 
        LIB(li)->lie_CompositorHook) 
    {
        struct CompositorMsg msg;
        msg.layer = l;
        msg.region = region;
        msg.method =
COMP_SHOWLAYER;
        
        CallHookPkt(LIB(li)->lie_CompositorHook, li, &msg);
        return;  /* Compositor handles everything */
    }
    
    /* Existing opaque blit code unchanged */
    /* ... */
}
```

### intuition/openwindow.c

```c
/* In tag parsing, add: */
case WA_Alpha:
    windowdata->use_alpha = (BOOL)tag->ti_Data;
    break;
    
case WA_AlphaValue:
    windowdata->alpha_value = (UBYTE)tag->ti_Data;
    break;

/* When creating the window's layer: */
if (windowdata->use_alpha) {
    layer_tags[n].ti_Tag = LA_Alpha;
    layer_tags[n++].ti_Data = TRUE;
    
    layer_tags[n].ti_Tag = LA_AlphaValue;
    layer_tags[n++].ti_Data = windowdata->alpha_value;
    
    /* Request ARGB bitmap for the layer */
    /* ... */
}
```

### compiler/include/graphics/layers.h

```c
/* New tags */
#define LA_Alpha        (LA_Dummy + 10)  /* BOOL - layer has alpha */
#define LA_AlphaValue   (LA_Dummy + 11)  /* UBYTE - global alpha 0-255 */
#define LA_AlphaRegion  (LA_Dummy + 12)  /* Region* - per-pixel alpha mask */
```

### compiler/include/intuition/intuition.h

```c
/* New window tags */
#define WA_Alpha        (WA_Dummy + 150)  /* BOOL - window has alpha */
#define WA_AlphaValue   (WA_Dummy + 151)  /* UBYTE - global alpha 0-255 */
#define WA_AlphaRegion  (WA_Dummy + 152)  /* Region* - transparency region */
```

## Layer Compositor Implementation

The compositor lives outside ROM in `workbench/devs/monitors/LayerCompositor/` or could be integrated into the existing `Compositor/` module.

### Key Functions

```c
/* Create compositor for a screen's layer info */
struct LayerCompositor *CreateLayerCompositor(
    struct Layer_Info *li,
    struct Screen *screen)
{
    struct LayerCompositor *comp = AllocMem(...);
    
    /* Initialize OpenGL context */
    comp->gl_context = CreateGLContext(screen);
    
    /* Create compositor FBO */
    glGenFramebuffers(1, &comp->compositor_fbo);
    glGenTextures(1, &comp->compositor_texture);
    /* ... setup FBO ... */
    
    /* Install hook in layer info */
    comp->hook.h_Entry = CompositorHookFunc;
    comp->hook.h_Data = comp;
    LIB(li)->lie_CompositorHook = &comp->hook;
    
    return comp;
}

/* Main compositing function */
void CompositeRegion(struct LayerCompositor *comp, 
                     struct Region *damage)
{
    glBindFramebuffer(GL_FRAMEBUFFER, comp->compositor_fbo);
    glViewport(0, 0, comp->width, comp->height);
    
    /* Clear with background */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    /* Composite all layers back to front */
    struct Layer *layer;
    for (layer = backmost; layer; layer = layer->front) {
        if (!LayerOverlapsRegion(layer, damage))
            continue;
            
        /* Upload layer bitmap to texture if dirty */
        SyncLayerTexture(comp, layer);
        
        /* Setup blending for alpha layers */
        if (IL(layer)->il_AlphaFlags & LAYERF_ALPHA) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            float alpha = IL(layer)->il_Alpha / 255.0f;
            glColor4f(1.0f, 1.0f, 1.0f, alpha);
        } else {
            glDisable(GL_BLEND);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        /* Draw layer */
        glBindTexture(GL_TEXTURE_2D, GetLayerTexture(layer));
        DrawTexturedQuad(layer->bounds);
    }
    
    /* Blit result to screen */
    BlitFBOToScreen(comp, damage);
}
```

## Integration with Wanderer

Wanderer can use transparent windows for:
- Desktop icons with alpha (already partially supported via drag images)
- Transparent window backgrounds
- Visual effects

```c
/* In iconwindow.c - open window with transparency */
Object *window = NewObject(MUIC_Window, NULL,
    MUIA_Window_Alpha, TRUE,
    MUIA_Window_AlphaValue, 200,  /* Slightly transparent */
    /* ... other tags ... */
    TAG_DONE);
```

## Integration with zunerenderer

zunerenderer already has:
- OpenGL backend with FBO support
- DrawingBoard with ARGB bitmaps
- Texture management

The Layer Compositor can share OpenGL resources with zunerenderer:
- Same GL context (AROS mesa3dgl limitation: one context)
- Texture upload utilities
- FBO management code

## Fallback Behavior

If the Layer Compositor is not loaded:
- `li_CompositorHook` remains NULL
- Alpha layers fall back to opaque blitting
- System functions normally, just without transparency

Software fallback in ROM (optional):
```c
/* In _ShowPartsOfLayer, if no compositor but alpha layer */
if ((IL(l)->il_AlphaFlags & LAYERF_ALPHA) && 
    !LIB(li)->lie_CompositorHook) 
{
    /* Use HIDD_BM_PutAlphaImage for software blending */
    /* Slow but functional */
}
```

## Summary of Changes

| Component | Location | Changes | Size |
|-----------|----------|---------|------|
| hyperlayers | ROM | Tags, flags, hook call | ~100-150 lines |
| intuition | ROM | WA_Alpha tags | ~30 lines |
| Layer Compositor | workbench/devs/ | All compositing logic | ~2000+ lines |
| Graphics HIDD | Optional | OpenGL bitmap class | Medium |
| zunerenderer | workbench/libs/ | Minimal, share GL code | Small |
| muimaster | workbench/libs/ | MUIA_Window_Alpha | ~50 lines |
| Wanderer | workbench/system/ | Use new alpha attributes | Small |

## Performance Considerations

1. **Texture uploads** - Only upload dirty layers, cache textures
2. **Damage regions** - Only recomposite damaged areas
3. **Batching** - Minimize GL state changes
4. **FBO reuse** - Don't recreate FBOs unnecessarily
5. **Fallback** - Skip compositing for fully opaque window stacks

## Future Extensions

1. **Window shadows** - Compositor can add drop shadows
2. **Blur effects** - Background blur for frosted glass effect
3. **Animations** - Smooth window transitions
4. **Per-pixel alpha regions** - Complex window shapes
5. **Hardware overlays** - Video playback optimization

## References

- Existing AROS Compositor: `workbench/devs/monitors/Compositor/`
- zunerenderer: `workbench/libs/zunegfx/`
- hyperlayers: `rom/hyperlayers/`
- intuition: `rom/intuition/`
- MorphOS compositing (inspiration): LA_TransRegion, LA_TransHook
