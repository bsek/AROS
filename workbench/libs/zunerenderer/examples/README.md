# Zune Renderer Library - Examples

This directory contains example programs demonstrating the capabilities of the Zune Renderer Library for AROS. The library provides a simplified, high-performance graphics API for the Zune UI toolkit with support for off-screen rendering, hardware acceleration, and direct pixel access.

## Examples Overview

### 1. simple_demo
**File:** `simple_demo.c`  
**Description:** Comprehensive demonstration of all major library features.

**Features demonstrated:**
- Basic drawing operations (rectangles, lines, pixels)
- Performance comparison between batched and unbatched drawing
- Off-screen rendering with DrawingBoards
- Direct pixel access and manipulation
- Surface blitting operations
- Resource management and cleanup

**What you'll see:**
- Colored rectangles and geometric patterns
- Performance differences with batching enabled
- Off-screen buffer creation and blitting
- Direct pixel manipulation for gradients and effects

### 2. simplified_batching_example  
**File:** `simplified_batching_example.c`  
**Description:** Focused demonstration of the batching system for improved performance.

**Features demonstrated:**
- Unbatched vs batched drawing performance comparison
- Automatic batch flushing on color changes
- Manual batch control with BeginBatch/EndBatch
- Color grouping effects on batch efficiency
- Batch statistics and monitoring

**What you'll see:**
- Performance comparisons between different drawing methods
- Visual demonstration of how batching groups operations
- Statistics showing batch counts and efficiency

### 3. simplified_drawingboard_example
**File:** `simplified_drawingboard_example.c`  
**Description:** Comprehensive demonstration of DrawingBoard (off-screen surface) capabilities.

**Features demonstrated:**
- Creating and managing multiple DrawingBoards
- Direct pixel access with lock/unlock operations
- Fast drawing operations on locked surfaces
- Various blitting operations between surfaces
- Double buffering technique for smooth animation
- Procedural texture generation

**What you'll see:**
- Off-screen rendering and surface management
- Direct pixel manipulation for gradients and patterns
- Double-buffered animation effects
- Procedural texture creation and tiling

## Building the Examples

The examples are built using the AROS build system (mmake).

### Prerequisites
- AROS development environment properly set up
- Zune Renderer Library built and installed
- Required system libraries (graphics.library, intuition.library)

### Build Commands

To build all examples:
```bash
mmake workbench-libs-zunerenderer-examples
```

To build a specific example:
```bash
mmake workbench-libs-zunerenderer-examples-simple_demo
mmake workbench-libs-zunerenderer-examples-simplified_batching_example  
mmake workbench-libs-zunerenderer-examples-simplified_drawingboard_example
```

The compiled examples will be placed in the AROS distribution directory.

## Running the Examples

All examples are interactive and will open a screen and window for demonstration. Each example includes multiple demo phases that you can step through:

1. Launch the example from AROS
2. Each demo phase will display visual results
3. Press any key or use the window controls to advance to the next phase
4. Close the window or complete all phases to exit

### Example Output

Each example provides console output describing what operations are being performed:

```
Zune Renderer Library - Simple Demo
====================================

All systems initialized successfully!

Demo 1: Basic Drawing Operations
Basic drawing completed - rectangles, lines, and pixels drawn
Press any key or close window to continue...

Demo 2: Batched Drawing (Performance)
Batching demonstration completed
Note: Batching typically provides significant performance improvements
when drawing many primitives of the same type and color.
Press any key or close window to continue...
```

## Technical Notes

### Library Requirements
- **zunerenderer.library**: The main graphics library
- **graphics.library v39+**: AROS graphics system
- **intuition.library v39+**: AROS GUI system
- **cybergraphics.library**: Optional, for hardware acceleration

### Performance Considerations
- Batching provides significant performance improvements when drawing many primitives
- Hardware surfaces (when available) offer better performance than software surfaces
- Direct pixel access allows for high-speed pixel manipulation
- Color grouping in batched operations reduces the number of batch flushes

### Hardware Requirements
- The examples will work on any AROS system
- CyberGraphics support provides hardware acceleration when available
- Examples automatically detect available backends and adapt accordingly

## Troubleshooting

### Common Issues

**"Cannot open zunerenderer.library"**
- Ensure the Zune Renderer Library is properly built and installed
- Check that the library is in the correct LIBS: directory

**Display Issues**
- Examples require a working AROS display system
- Ensure your AROS system has proper graphics drivers

**Performance Issues**
- Software rendering will be slower than hardware acceleration
- Large DrawingBoards may consume significant memory

### Debug Information

All examples include detailed console output showing:
- Initialization status of each system
- Backend detection results (CyberGraphics availability)
- DrawingBoard properties and capabilities
- Performance statistics where applicable

## API Usage Examples

The examples serve as practical demonstrations of common API usage patterns:

```c
// Creating a DrawingBoard
struct DrawingBoard *board = CreateDrawingBoard(320, 240, 32, 
    ZUNE_DRAWINGBOARD_HARDWARE | ZUNE_DRAWINGBOARD_CACHED);

// Creating a RenderPort for off-screen rendering
struct RenderPort *rp = CreateRenderPortWithDrawingBoard(screen->ViewPort.ColorMap, board);

// Batched drawing for performance
BeginBatch(rp);
for (int i = 0; i < 1000; i++) {
    FillRectangle(rp, x, y, width, height, color);
}
EndBatch(rp);

// Direct pixel access
APTR pixels = LockDrawingBoardPixels(board, &pitch);
if (pixels) {
    SetPixel(board, x, y, color);
    UnlockDrawingBoardPixels(board);
}

// Resource cleanup
DestroyRenderPort(rp);
DestroyDrawingBoard(board);
```

## Further Information

- See `../include/zunerenderer.h` for complete API documentation
- Check the main library documentation for architectural details
- Review the source code for implementation examples

## AROS-Specific Implementation Notes

### Performance Testing and Timing

The `batching_performance_test` example demonstrates proper AROS timing methodology:

**❌ Common Mistake:**
```c
ULONG start = Delay(0);  // WRONG! Delay() is for sleeping, not timing
// ... do work ...
ULONG end = Delay(0);
ULONG duration = end - start;  // This doesn't work on AROS
```

**✅ Correct AROS Approach:**
```c
struct timeval tv;
GetSysTime(&tv);  // From timer.device - proper AROS timing
ULONG microseconds = tv.tv_secs * 1000000 + tv.tv_micro;
```

**Key Points:**
- Use `timer.device` with `UNIT_MICROHZ` for precise timing
- `Delay()` pauses execution - it's not for measuring time
- `GetSysTime()` from timer.device provides accurate system time
- Open timer.device properly with `CreateIORequest()` and `OpenDevice()`
- Always cleanup timer resources with `CloseDevice()` and `DeleteIORequest()`

This methodology ensures accurate performance measurements across all AROS systems.

## License

Copyright (C) 2025, The AROS Development Team. All rights reserved.

These examples are provided under the same license terms as the Zune Renderer Library.
