# SDF Antialiasing Test Application

## Overview

This Zune application replicates the functionality of the HTML SDF antialiasing test for the zunerenderer library. It provides an interactive interface to test and visualize different SDF (Signed Distance Field) rounded rectangle rendering settings.

## Features

- **Real-time Preview**: Interactive canvas showing SDF-rendered rounded rectangles
- **Shape Parameters**: Adjustable corner radius, width, and height
- **Border Controls**: Configurable border width with multiple methods (Centered, Outer, Inner, Sharp)
- **Antialiasing Settings**: Smoothness control and gamma correction options
- **Color Customization**: Hex color inputs for fill, border, and background colors

## Optimal Settings

Based on testing, the recommended settings are:
- **AA Smoothness**: 0.6
- **Gamma Correction**: Enabled for borders wider than 3px
- **Border Method**: Inner (recommended for best visual results)

## Controls

### Shape Parameters
- **Corner Radius**: 0-50px - Controls the roundness of rectangle corners
- **Width**: 50-350px - Rectangle width
- **Height**: 50-250px - Rectangle height

### Border Parameters
- **Border Width**: 0-20px - Thickness of the border
- **Method**: Border rendering method
  - *Centered*: Border extends both inward and outward from edge
  - *Outer*: Border extends outward from shape edge (CSS-like)
  - *Inner*: Border stays inside the shape (recommended)
  - *Sharp*: Sharp edge with minimal antialiasing

### Antialiasing
- **Smoothness**: 0.10-1.50 - Controls antialiasing transition smoothness
- **Manual Gamma**: Force gamma correction on/off
- **Auto Gamma**: Automatically enable gamma for borders >= threshold
- **Gamma Threshold**: 1.0-10.0px - Automatic gamma correction threshold

### Colors
- **Fill Color**: Interior color of the rectangle (hex format)
- **Border Color**: Border color (hex format)  
- **Background**: Canvas background color (hex format)

## Technical Details

The application uses the zunerenderer library's antialiased drawing functions:
- `ZuneDrawRectangleRoundedAA()` for filled shapes
- `ZuneDrawRectangleRoundedOutlineStyled()` for borders

The SDF implementation provides smooth, high-quality antialiasing that scales well at different sizes and is particularly effective for UI elements.

## Building

The application is built as part of the zunerenderer examples:

```bash
make workbench-libs-zunegfx-examples
```

## Usage

1. Launch the application
2. Adjust parameters using the sliders and controls
3. Observe real-time changes in the preview canvas
4. Experiment with different combinations to understand SDF rendering behavior

## Implementation Notes

- Uses a custom MUI Area class for the rendering canvas
- Renders directly to the MUI RastPort during draw events
- Integrates with zunerenderer's quality control system
- Supports real-time parameter updates with immediate visual feedback

This application serves as both a practical testing tool and a demonstration of the zunerenderer library's SDF antialiasing capabilities.