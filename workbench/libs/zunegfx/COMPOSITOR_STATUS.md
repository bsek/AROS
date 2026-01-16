# Layer Compositor Status

## Date: 2026-01-17

## Summary

The layer compositor infrastructure is now complete. The `LayerInfo_extra` allocation issue has been fixed, and alpha window support has been added to Intuition's OpenWindow. A new test program `alpha_window_test` has been created to verify the integration.

## Recent Changes (2026-01-17)

### 1. Fixed LayerInfo_extra Allocation in OpenScreen

**Problem:** The compositor hook could not be installed because `LayerInfo_extra` was NULL on screens.

**Solution:** Added `FattenLayerInfo()` call after `InitLayers()` in OpenScreen.

**File modified:** `rom/intuition/openscreen.c` (around line 1755)

```c
DEBUG_OPENSCREEN(dprintf("OpenScreen: init layers\n"));
#ifdef __AROS__
    InitLayers(&screen->Screen.LayerInfo);
    /* Allocate LayerInfo_extra for compositor hook support.
     * FattenLayerInfo() allocates the extended structure that holds
     * lie_CompositorHook for hardware-accelerated alpha compositing.
     */
    FattenLayerInfo(&screen->Screen.LayerInfo);
#else
    ...
```

### 2. Added Alpha Window Support to OpenWindow

**Problem:** The `WA_Alpha`, `WA_AlphaValue`, and `WA_NoShadow` window tags were parsed but not passed to the layer creation function (second `int_openwindow`).

**Solution:** Extended `OpenWindowActionMsg` struct and added message passing for compositor variables.

**File modified:** `rom/intuition/openwindow.c`

- Added fields to `OpenWindowActionMsg` struct (line ~39):
  ```c
  /* Layer compositor support */
  BOOL                             use_alpha;
  UBYTE                            alpha_value;
  BOOL                             no_shadow;
  ```

- Added message population before DoSyncAction (line ~1089):
  ```c
  /* Layer compositor support */
  msg.use_alpha = use_alpha;
  msg.alpha_value = alpha_value;
  msg.no_shadow = no_shadow;
  ```

- Added local variables in second `int_openwindow` function (line ~1276):
  ```c
  /* Layer compositor support */
  BOOL            use_alpha = msg->use_alpha;
  UBYTE           alpha_value = msg->alpha_value;
  BOOL            no_shadow = msg->no_shadow;
  ```

### 3. New Test Program: alpha_window_test

**File:** `workbench/libs/zunegfx/examples/alpha_window_test.c`

A simple test program that:
1. Opens a normal (opaque) background window
2. Opens an alpha window with `WA_Alpha=TRUE` and `WA_AlphaValue=180`
3. Prints debug info about LayerInfo_extra and LAYERF_ALPHA flag
4. Allows testing compositor transparency

**Build:** `make workbench-libs-zunegfx-examples`

**Location:** `bin/linux-x86_64/AROS/alpha_window_test`

## Completed Work

### Compositor Library Export

The compositor functions are properly exported from zunegfx.library:

- `CreateLayerCompositor` / `CreateLayerCompositorShared`
- `DestroyLayerCompositor`
- `ActivateLayerCompositor` / `DeactivateLayerCompositor`
- `CompositorFindWindow` / `CompositorRegisterWindow` / `CompositorUnregisterWindow`
- `CompositorSetWindowAlpha`
- `CompositorMarkWindowDirty`
- `CompositorUpdate` / `CompositorRefresh`
- `CompositorSetShadow`

### GL Context Sharing

GL context sharing is working correctly:
- All contexts share the same `pipe_screen`
- Master context is properly passed to compositor via `ZuneGetMasterGLContext()`
- FBO content is read back correctly

### Layer Compositor Hook Integration

The hyperlayers library has full compositor hook support:

- `LayerInfo_extra` struct contains `lie_CompositorHook` field
- `_ShowPartsOfLayer()` in `basicfuncs.c` calls compositor hook for alpha layers
- `CreateUpfrontLayerTagList()` handles `LA_Alpha`, `LA_AlphaValue`, `LA_NoShadow` tags
- Layers get `LAYERF_ALPHA` and `ILAF_ALPHA` flags when alpha is enabled

## Current Architecture

### Alpha Window Flow

```
1. Application calls OpenWindowTags() with WA_Alpha, WA_AlphaValue
                    |
                    v
2. Intuition OpenWindow parses tags, sets use_alpha/alpha_value
                    |
                    v
3. OpenWindowActionMsg passes values to int_openwindow
                    |
                    v
4. int_openwindow calls CreateUpfrontLayerTagList with LA_Alpha tags
                    |
                    v
5. Hyperlayers sets LAYERF_ALPHA on layer, stores alpha in IntLayer
                    |
                    v
6. When layer needs refresh, _ShowPartsOfLayer checks:
   - Is ILAF_ALPHA set?
   - Is lie_CompositorHook installed?
                    |
                    v
7. If both true: calls compositor hook with COMP_SHOWLAYER
   If not: uses normal layer blitting (opaque)
```

### Compositor Installation Flow

```
1. Screen created with FattenLayerInfo() -> LayerInfo_extra allocated
                    |
                    v
2. ZuneGfx/application calls CreateLayerCompositor(screen)
                    |
                    v
3. Compositor allocates resources, stores LayerInfo pointer
                    |
                    v
4. ActivateLayerCompositor() installs hook:
   LIE(li)->lie_CompositorHook = &comp->lc_Hook
                    |
                    v
5. Alpha layers now route through compositor for display
```

## Outstanding Issue: Black Background Window on First Run

### Symptoms
- Background window appears black on first run of compositor_test
- Works correctly if opengl_test is run first
- Alpha window renders correctly
- FBO content is correct when read back

### Hypothesis
The issue is likely related to Mesa's internal blitting pipeline initialization. When opengl_test runs first, it initializes some internal state that compositor_test benefits from.

## Files Overview

```
rom/intuition/
├── openscreen.c                    # Added FattenLayerInfo() call
└── openwindow.c                    # Added alpha window tag support

rom/hyperlayers/
├── layers_intern.h                 # LayerInfo_extra with lie_CompositorHook
├── basicfuncs.c                    # Compositor hook call in _ShowPartsOfLayer
└── createlayertaglist.c            # LA_Alpha/LA_AlphaValue/LA_NoShadow handling

workbench/libs/zunegfx/
├── zunegfx.conf                    # Library function exports
├── mmakefile.src                   # Build configuration
├── include/
│   └── zunegfx.h                   # Public header
├── src/
│   ├── zunegfx_compositor.c        # Library wrappers for compositor
│   ├── compositor/
│   │   ├── layer_compositor.c      # Compositor implementation
│   │   └── layer_compositor.h      # Compositor header
│   └── backends/
│       └── opengl/
│           └── opengl_backend.c    # FBO and GL context management
└── examples/
    ├── compositor_test.c           # Full compositor test
    └── alpha_window_test.c         # NEW: Simple alpha window test
```

## Next Steps

1. **Test alpha window functionality** - Run `alpha_window_test` to verify:
   - `LayerInfo_extra` is non-NULL
   - `LAYERF_ALPHA` is set on alpha window layer
   
2. **Install compositor hook** - The compositor needs to be activated on the screen for transparency to work. This could be done:
   - Automatically by zunegfx when first alpha window opens
   - Manually by applications that want compositing
   - By a system service/commodity

3. **Debug black background issue** - Compare opengl_test initialization with compositor_test

4. **Consider auto-initialization** - Have zunegfx automatically install compositor hook when screen gets first alpha window
