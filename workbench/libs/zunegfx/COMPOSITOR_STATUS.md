# Layer Compositor Status

## Date: 2026-01-16

## Summary

The compositor functions have been successfully exported via zunegfx.library. However, there is still an issue with the background window appearing black on first run.

## Completed Work

### 1. Compositor Library Export

The compositor functions are now properly exported from zunegfx.library instead of being statically linked into test programs.

**Files modified:**

- `src/compositor/layer_compositor.c` - Renamed all public functions to `*Internal` versions:
  - `CreateLayerCompositorInternal`
  - `CreateLayerCompositorSharedInternal`
  - `DestroyLayerCompositorInternal`
  - `ActivateLayerCompositorInternal`
  - `DeactivateLayerCompositorInternal`
  - `CompositorFindWindowInternal`
  - `CompositorRegisterWindowInternal`
  - `CompositorUnregisterWindowInternal`
  - `CompositorSetWindowAlphaInternal`
  - `CompositorMarkWindowDirtyInternal`
  - `CompositorUpdateInternal`
  - `CompositorRefreshInternal`
  - `CompositorSetShadowInternal`

- `src/compositor/layer_compositor.h` - Updated function declarations to Internal versions

- `src/zunegfx_compositor.c` - **NEW FILE** - AROS library wrappers (AROS_LH macros) that call the Internal functions. LVO numbers 106-118.

- `zunegfx.conf` - Added 13 compositor functions to the function list

- `mmakefile.src` - Added `src/zunegfx_compositor` to COMPOSITOR_FILES

- `examples/compositor_test.c` - Removed `#include "../src/compositor/layer_compositor.c"`, now uses library functions via `<proto/zunegfx.h>`

### 2. GL Context Sharing

GL context sharing is working correctly:
- All contexts share the same `pipe_screen` (0x0000761e857faa80)
- Master context is properly passed to compositor via `ZuneGetMasterGLContext()`
- FBO content is read back correctly (verified via debug output)

## Outstanding Issue: Black Background Window on First Run

### Symptoms
- Background window appears black on first run of compositor_test
- Works correctly if opengl_test is run first
- Alpha window renders correctly
- FBO content is correct when read back (RGBA = 28 50 78 ff)
- WritePixelArray writes correct data to DrawingBoard bitmap
- BltBitMapRastPort is called with correct source data

### Debug Evidence
```
ZuneRenderer: ZunePresent - src bitmap sample at 0,0: RGBA = 28 50 78 ff
ZuneRenderer: ZunePresent - board->rastport->BitMap=0000761e83bf2df0, board->bitmap=0000761e83bf2df0, match=YES
ZuneRenderer: ZunePresent - dst window sample at 10,25: RGBA = 28 50 78 00
ZuneRenderer: ZunePresent - BltBitMapRastPort completed
```

The source bitmap has correct data, but the destination window shows the correct RGB values only after BltBitMapRastPort - yet visually the window is black.

### Hypothesis
The issue is likely related to Mesa's internal blitting pipeline initialization. When opengl_test runs first, it initializes some internal state (possibly in `BltPipeResourceRastPort` or similar) that compositor_test benefits from on subsequent runs.

The "priming" code in compositor_test attempts to work around this but is not sufficient:
```c
/* WORKAROUND: Prime the GL context by doing a dummy window-based render */
ZuneSetTarget(bg_rp, NULL);  /* Target window directly */
ClearRenderPort(bg_rp, ZUNE_BLACK);
```

### Potential Investigation Areas

1. **Mesa BltPipeResourceRastPort initialization** - Check what opengl_test does differently that initializes this path

2. **glASwapBuffers timing** - The swap might need to happen before FBO operations

3. **Framebuffer binding state** - Check if the default framebuffer (FBO 0) needs to be rendered to first

4. **Bitmap format mismatch** - Source is 32-bit ARGB, destination screen is 24-bit RGB. The blit may not be handling this correctly on first run.

5. **Pipe resource caching** - Mesa may cache pipe resources per-bitmap, and first-run may not have the cache populated

### Compositor Activation Issue

The compositor hook cannot be installed because LayerInfo_extra is NULL:
```
[LayerCompositor] No LayerInfo_extra!
[CompositorTest] WARNING: Failed to activate compositor
```

This is a separate issue - the screen's LayerInfo doesn't have the extra structure allocated. This needs to be investigated in hyperlayers or intuition code.

## Files Overview

```
workbench/libs/zunegfx/
├── zunegfx.conf                    # Library function exports (13 compositor functions added)
├── mmakefile.src                   # Build configuration
├── include/
│   └── zunegfx.h                   # Public header with compositor structs/functions
├── src/
│   ├── zunegfx_compositor.c        # NEW: Library wrappers for compositor
│   ├── compositor/
│   │   ├── layer_compositor.c      # Internal implementations (*Internal functions)
│   │   └── layer_compositor.h      # Internal header
│   └── backends/
│       └── opengl/
│           └── opengl_backend.c    # FBO and GL context management
└── examples/
    └── compositor_test.c           # Test program (now uses library)
```

## Next Steps

1. Debug why BltBitMapRastPort doesn't visually update the window on first run despite correct source data

2. Compare what opengl_test does during initialization that makes subsequent compositor_test runs work

3. Investigate LayerInfo_extra allocation for compositor hook installation

4. Consider adding explicit Mesa pipeline initialization in zunegfx library init
