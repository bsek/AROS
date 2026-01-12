# Zune Renderer Library

A high-performance, simplified graphics rendering library for the AROS Zune UI toolkit, providing hardware-accelerated drawing operations, off-screen rendering surfaces, and direct pixel access capabilities.

## Overview

The Zune Renderer Library is designed to provide modern graphics programming capabilities to AROS applications while maintaining simplicity and high performance. It bridges the gap between the traditional AROS graphics system and contemporary graphics programming needs, offering:

- **Simplified API**: Clean, intuitive interface that eliminates unnecessary complexity
- **Hardware Acceleration**: Automatic detection and utilization of CyberGraphics capabilities
- **Off-screen Rendering**: Full support for DrawingBoard surfaces with direct pixel access
- **Performance Optimization**: Built-in batching system for efficient rendering
- **Seamless Integration**: Works with existing AROS graphics and Intuition systems

## Key Features

### 🎨 **Rendering Operations**
- Rectangle filling and outline drawing
- Line drawing with anti-aliasing support
- Individual pixel manipulation
- Advanced color management with ARGB support
- Clipping rectangle support

### 🖼️ **Off-screen Surfaces (DrawingBoards)**
- Hardware and software surface creation
- Direct pixel buffer access for high-speed operations
- Multiple pixel format support
- Alpha channel and transparency handling
- Memory-efficient resource management

### ⚡ **Performance Optimization**
- Automatic operation batching for improved performance
- Smart batch flushing based on color changes
- Manual batch control for advanced use cases
- Hardware acceleration when available
- Optimized blitting operations

### 🔧 **Backend Integration**
- Automatic backend detection (graphics.library vs CyberGraphics)
- Fallback mechanisms for maximum compatibility
- Hardware surface allocation when supported
- Direct integration with AROS BitMap and RastPort systems

## Architecture

The library follows a simplified, layered architecture:

```
┌─────────────────────────────────┐
│          Public API             │ ← zunegfx.h
├─────────────────────────────────┤
│        Core Management          │ ← zunerenderer_core.c
│   (RenderPort, DrawingBoard)    │
├─────────────────────────────────┤
│      Drawing Operations         │ ← zunerenderer_drawing.c
│    (Primitives, Batching)       │
├─────────────────────────────────┤
│       Pixel Operations          │ ← zunerenderer_pixelops.c
│   (Direct Access, Fast Ops)     │
├─────────────────────────────────┤
│    DrawingBoard Management      │ ← zunerenderer_drawingboard.c
│   (Off-screen Surfaces)         │
├─────────────────────────────────┤
│      Backend Abstraction        │ ← zunerenderer_backends.c
│  (graphics.library/CyberGfx)    │
└─────────────────────────────────┘
```

## API Overview

### Core Structures

```c
struct RenderPort      // Main rendering context
struct DrawingBoard    // Off-screen rendering surface  
struct ZuneRect        // Rectangle definition
struct ZunePoint       // Point definition
```

### Essential Functions

```c
// RenderPort Management
struct RenderPort *CreateRenderPort(struct ColorMap *colormap, struct RastPort *rp);
struct RenderPort *CreateRenderPortWithDrawingBoard(struct ColorMap *colormap, struct DrawingBoard *board);
void DestroyRenderPort(struct RenderPort *rp);

// DrawingBoard Management  
struct DrawingBoard *CreateDrawingBoard(UWORD width, UWORD height, UBYTE depth, ULONG flags);
void DestroyDrawingBoard(struct DrawingBoard *board);

// Drawing Operations
void FillRectangle(struct RenderPort *rp, WORD x, WORD y, UWORD width, UWORD height, ULONG color);
void DrawLine(struct RenderPort *rp, WORD x1, WORD y1, WORD x2, WORD y2, ULONG color);
void DrawPixel(struct RenderPort *rp, WORD x, WORD y, ULONG color);

// Performance Batching
void BeginBatch(struct RenderPort *rp);
void EndBatch(struct RenderPort *rp);

// Direct Pixel Access
APTR LockDrawingBoardPixels(struct DrawingBoard *board, ULONG *pitch);
void UnlockDrawingBoardPixels(struct DrawingBoard *board);
```

## Building and Installation

### Prerequisites
- AROS development environment
- graphics.library v39+
- intuition.library v39+
- cybergraphics.library (optional, for hardware acceleration)

### Build Process

```bash
# Build the library
mmake workbench-libs-zunerenderer

# Build examples
mmake workbench-libs-zunegfx-examples

# Install includes
mmake workbench-libs-zunegfx-includes
```

The library will be installed to `LIBS:zunegfx.library` and headers to `INCLUDE:libraries/zunegfx.h`.

## Quick Start Example

```c
#include <intuition/intuition.h>
#include <libraries/zunegfx.h>
#include <proto/intuition.h>
#include <proto/zunegfx.h>

int main() {
    struct Library *ZuneGfxBase;
    struct RenderPort *rp;
    struct DrawingBoard *board;
    struct Screen *screen;
    struct ColorMap *colormap;
    
    // Open library
    ZuneGfxBase = OpenLibrary("zunegfx.library", 1);
    if (!ZuneGfxBase) return -1;

    // Acquire a ColorMap (use your target screen)
    screen = LockPubScreen(NULL);
    if (!screen) {
        CloseLibrary(ZuneGfxBase);
        return -1;
    }
    colormap = screen->ViewPort.ColorMap;
    
    // Create off-screen surface
    board = CreateDrawingBoard(320, 240, 32, ZUNE_DRAWINGBOARD_HARDWARE);
    rp = CreateRenderPortWithDrawingBoard(colormap, board);
    
    // Draw something
    ClearRenderPort(rp, ZUNE_BLUE);
    FillRectangle(rp, 50, 50, 100, 80, ZUNE_RED);
    
    // Cleanup
    DestroyRenderPort(rp);
    DestroyDrawingBoard(board);
    UnlockPubScreen(NULL, screen);
    CloseLibrary(ZuneGfxBase);
    return 0;
}
```

## Performance Guidelines

### Batching Best Practices
- Use `BeginBatch()` and `EndBatch()` for multiple drawing operations
- Group operations by color for maximum batch efficiency  
- Batch flushes automatically on color changes
- Monitor batch statistics with `GetBatchCount()`

### DrawingBoard Optimization
- Use `ZUNE_DRAWINGBOARD_HARDWARE` flag when possible
- Lock surfaces for direct pixel access during intensive operations
- Prefer `FastFillRect()` and similar functions for locked surfaces
- Use appropriate pixel formats for your use case

### Memory Management
- Always pair Create/Destroy calls
- Unlock pixel buffers when finished with direct access
- Use `ZUNE_DRAWINGBOARD_CACHED` for frequently accessed surfaces
- Monitor resource usage in applications with many surfaces

## File Structure

```
zunegfx/
├── README.md                    # This file
├── zunerenderer.conf           # Library configuration
├── zunerenderer_init.c         # Library initialization
├── mmakefile.src               # Build configuration
├── include/
│   └── zunegfx.h          # Public API header
├── src/
│   ├── zunegfx_intern.h   # Internal definitions
│   ├── zunerenderer_core.c     # Core management functions
│   ├── zunerenderer_drawing.c  # Drawing operations
│   ├── zunerenderer_pixelops.c # Pixel manipulation
│   ├── zunerenderer_drawingboard.c # Off-screen surfaces
│   └── zunerenderer_backends.c # Backend abstraction
└── examples/
    ├── README.md               # Examples documentation
    ├── mmakefile.src           # Examples build file
    ├── simple_demo.c           # Comprehensive demo
    ├── simplified_batching_example.c    # Batching demo
    └── simplified_drawingboard_example.c # DrawingBoard demo
```

## Color System

The library uses a comprehensive ARGB32 color system:

```c
// Color creation macros
#define ZUNE_COLOR_ARGB32(a, r, g, b)  // Full ARGB specification
#define ZUNE_COLOR_RGB24(r, g, b)      // RGB with full alpha

// Predefined colors  
#define ZUNE_BLACK, ZUNE_WHITE, ZUNE_RED, ZUNE_GREEN, ZUNE_BLUE
#define ZUNE_YELLOW, ZUNE_MAGENTA, ZUNE_CYAN, ZUNE_GRAY

// Color manipulation
ULONG RGBToColor(UBYTE r, UBYTE g, UBYTE b);
ULONG BlendColors(ULONG color1, ULONG color2, UBYTE alpha);
```

## DrawingBoard Flags

```c
#define ZUNE_DRAWINGBOARD_HARDWARE  // Prefer hardware surfaces
#define ZUNE_DRAWINGBOARD_ALPHA     // Enable alpha channel support
#define ZUNE_DRAWINGBOARD_CACHED    // Cache in video memory
#define ZUNE_DRAWINGBOARD_TEMP      // Temporary surface (fast alloc)
```

## Backend Capabilities

### Graphics.library Backend
- Compatible with all AROS systems
- Software rendering
- Standard AROS bitmap operations
- Palette-based color management

### CyberGraphics Backend
- Hardware acceleration when available
- Direct pixel format operations
- True-color rendering
- High-performance blitting
- Direct framebuffer access

## Thread Safety

The library is designed for single-threaded use per RenderPort/DrawingBoard. For multi-threaded applications:

- Each thread should have its own RenderPort instances
- DrawingBoard sharing between threads requires external synchronization
- Library initialization is not thread-safe

## Debugging and Diagnostics

### Debug Build
Enable debugging with `DEBUG=1` during compilation for additional runtime checks and logging.

### Diagnostic Functions
```c
BOOL IsRenderPortValid(struct RenderPort *rp);
BOOL IsDrawingBoardValid(struct DrawingBoard *board);
void GetRenderPortInfo(struct RenderPort *rp, ...);
void GetDrawingBoardInfo(struct DrawingBoard *board, ...);
```

## Limitations

- Maximum DrawingBoard size depends on available memory
- Hardware acceleration availability depends on system configuration
- Some advanced graphics features require CyberGraphics
- Thread safety requires external synchronization

## Migration from Other Graphics APIs

### From graphics.library
- Replace direct RastPort operations with RenderPort functions
- Use batching for performance improvements
- Consider off-screen rendering for complex scenes

### From CyberGraphics
- Similar pixel access patterns with DrawingBoard locking
- Simplified color management
- Automatic backend selection

## Contributing

The library follows AROS development standards:

- Code style matches AROS conventions
- All public functions documented in header
- Examples provided for new features
- Performance considerations documented

## Troubleshooting

### Common Issues

**Library won't open**
- Check LIBS: assignment
- Verify all dependencies are available
- Ensure proper AROS installation

**Performance issues**
- Enable batching for multiple operations
- Use hardware surfaces when possible
- Profile with debug builds

**Memory issues**
- Check for unmatched Create/Destroy pairs
- Unlock all pixel buffers
- Monitor DrawingBoard allocation sizes

### Debug Output
Enable with `#define DEBUG 1` for detailed operation logging.

## License

Copyright (C) 2025, The AROS Development Team. All rights reserved.

This library is part of the AROS Research Operating System and is distributed under the same licensing terms as AROS.

## Version History

- **v1.0**: Initial release with core functionality
  - RenderPort and DrawingBoard management
  - Basic drawing operations
  - Batching system
  - CyberGraphics integration
  - Direct pixel access
  - Comprehensive examples

---

For more information, see the example programs in the `examples/` directory and the complete API documentation in `include/zunegfx.h`.
