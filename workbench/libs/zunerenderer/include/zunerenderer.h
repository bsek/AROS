#ifndef LIBRARIES_ZUNERENDERER_H
#define LIBRARIES_ZUNERENDERER_H

/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Public API Header

    A modern, clean rendering library for AROS with unified target system
    and consistent naming conventions. Supports both immediate mode rendering
    to screens and buffered rendering to off-screen surfaces.
*/

#include "graphics/gfx.h"
#include "graphics/view.h"
#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_NODES_H
#include <exec/nodes.h>
#endif

#ifndef GRAPHICS_RASTPORT_H
#include <graphics/rastport.h>
#endif

#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif

#ifndef INTUITION_SCREENS_H
#include <intuition/screens.h>
#endif

/*****************************************************************************/
/* Library Information */
/*****************************************************************************/

#define ZUNERENDERER_NAME "zunerenderer.library"
#define ZUNERENDERER_VERSION 1

/*****************************************************************************/
/* Backend Types */
/*****************************************************************************/

#define BACKEND_BEST_AVAILABLE 0
#define BACKEND_SOFTWARE 1
#define BACKEND_CYBERGFX 2
#define BACKEND_OPENGL 3
#define BACKEND_COUNT 3

/*****************************************************************************/
/* Color System */
/*****************************************************************************/

/* Color creation macros */
#define ZUNE_COLOR_ARGB32(a, r, g, b)                                          \
  (((ULONG)(a) << 24) | ((ULONG)(r) << 16) | ((ULONG)(g) << 8) | (ULONG)(b))
#define ZUNE_COLOR_RGB24(r, g, b) ZUNE_COLOR_ARGB32(255, r, g, b)
#define ZUNE_COLOR_TRANSPARENT ZUNE_COLOR_ARGB32(0, 0, 0, 0)

/* Color component extraction */
#define ZUNE_GET_ALPHA(c) (((c) >> 24) & 0xFF)
#define ZUNE_GET_RED(c) (((c) >> 16) & 0xFF)
#define ZUNE_GET_GREEN(c) (((c) >> 8) & 0xFF)
#define ZUNE_GET_BLUE(c) ((c)&0xFF)

/* Common colors */
#define ZUNE_BLACK ZUNE_COLOR_RGB24(0, 0, 0)
#define ZUNE_WHITE ZUNE_COLOR_RGB24(255, 255, 255)
#define ZUNE_RED ZUNE_COLOR_RGB24(255, 0, 0)
#define ZUNE_GREEN ZUNE_COLOR_RGB24(0, 255, 0)
#define ZUNE_BLUE ZUNE_COLOR_RGB24(0, 0, 255)
#define ZUNE_YELLOW ZUNE_COLOR_RGB24(255, 255, 0)
#define ZUNE_MAGENTA ZUNE_COLOR_RGB24(255, 0, 255)
#define ZUNE_CYAN ZUNE_COLOR_RGB24(0, 255, 255)
#define ZUNE_GRAY ZUNE_COLOR_RGB24(128, 128, 128)
#define ZUNE_LIGHTGRAY ZUNE_COLOR_RGB24(192, 192, 192)
#define ZUNE_DARKGRAY ZUNE_COLOR_RGB24(64, 64, 64)

/*****************************************************************************/
/* Drawing Board Flags */
/*****************************************************************************/

#define ZUNE_DRAWINGBOARD_HARDWARE                                             \
  (1 << 0) /* Use hardware acceleration if available */
#define ZUNE_DRAWINGBOARD_ALPHA (1 << 1)  /* Support alpha blending */
#define ZUNE_DRAWINGBOARD_CACHED (1 << 2) /* Cache in video memory */
#define ZUNE_DRAWINGBOARD_TEMP                                                 \
  (1 << 3) /* Temporary surface, optimize for short usage */
#define ZUNE_DRAWINGBOARD_LINEARMEM                                            \
  (1 << 4) /* Force linear memory for direct pixel access (no friend bitmap) */

/*****************************************************************************/
/* Texture Flags */
/*****************************************************************************/

#define ZUNE_TEXTURE_HARDWARE (1 << 0)  /* Prefer hardware storage */
#define ZUNE_TEXTURE_MIPMAPS (1 << 1)   /* Generate mipmaps */
#define ZUNE_TEXTURE_FILTERING (1 << 2) /* Enable texture filtering */
#define ZUNE_TEXTURE_WRAPPING (1 << 3)  /* Enable texture wrapping */
#define ZUNE_TEXTURE_DYNAMIC (1 << 4)   /* Frequently updated texture */
#define ZUNE_TEXTURE_ALPHA (1 << 5)     /* Support alpha blending */
#define ZUNE_TEXTURE_OPAQUE (1 << 6)    /* All pixels fully opaque (optimization) */

/*****************************************************************************/
/* Texture Formats */
/*****************************************************************************/

#define ZUNE_TEXTURE_FORMAT_ARGB32 0 /* 32-bit ARGB */
#define ZUNE_TEXTURE_FORMAT_RGB24 1  /* 24-bit RGB */
#define ZUNE_TEXTURE_FORMAT_ARGB16 2 /* 16-bit ARGB */
#define ZUNE_TEXTURE_FORMAT_RGB16 3  /* 16-bit RGB */
#define ZUNE_TEXTURE_FORMAT_L8 4     /* 8-bit luminance */
#define ZUNE_TEXTURE_FORMAT_A8 5     /* 8-bit alpha */

/*****************************************************************************/
/* Core Structures */
/*****************************************************************************/

/* Rectangle structure */
struct ZuneRect {
  WORD x, y;
  UWORD width, height;
};

/* Point structure */
struct ZunePoint {
  WORD x, y;
};

struct ZuneTexture;

/* Brush system for fill styles */
enum ZuneBrushType {
  ZUNE_BRUSH_TYPE_SOLID = 0,
  ZUNE_BRUSH_TYPE_TEXTURE,
  ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
  ZUNE_BRUSH_TYPE_RADIAL_GRADIENT,
  ZUNE_BRUSH_TYPE_PEN,
  ZUNE_BRUSH_TYPE_PATTERN,  /* 2-row 16-bit pattern with fg/bg pens */
  ZUNE_BRUSH_TYPE_DATATYPE  /* DataTypes-backed texture (uses ZuneTexture) */
};

enum ZuneBrushWrapMode {
  ZUNE_BRUSH_WRAP_CLAMP = 0,
  ZUNE_BRUSH_WRAP_REPEAT,
  ZUNE_BRUSH_WRAP_MIRROR
};

enum ZuneBrushFilterMode {
  ZUNE_BRUSH_FILTER_NEAREST = 0,
  ZUNE_BRUSH_FILTER_LINEAR
};


struct InternalColor {
  ULONG original_pixel; /* As provided */
  UBYTE a, r, g, b;     /* Individual components */
  LONG pen;             /* Allocated pen */
  BOOL pen_allocated;   /* TRUE if pen is allocated */
};

struct ZuneGradientStop {
  float position; /* 0.0 - 1.0 */
  ULONG color;
};

struct ZuneBrush {
  enum ZuneBrushType type;
  ULONG flags;

  /* INTERNAL CACHE - DO NOT MODIFY DIRECTLY
   * These fields are populated automatically by ZuneRenderer when the brush
   * is used in rendering operations. They cache pre-computed data for optimal
   * performance in tight rendering loops. */
  struct {
    BOOL valid; /* TRUE if cache data is valid and up-to-date */

    struct InternalColor *color;

    /* Cached data for LINEAR_GRADIENT - incremental calculation */
    struct {
      float t_start;  /* Gradient t-value at rectangle origin */
      float t_step_x; /* Increment per pixel in x-direction */
      float t_step_y; /* Increment per pixel in y-direction */
      /* Pre-rasterized gradient texture for fast repeated drawing */
      ULONG *rasterized_pixels;  /* ARGB32 pixel data, NULL if not cached */
      UWORD rasterized_width;    /* Width the cache was created for */
      UWORD rasterized_height;   /* Height the cache was created for */
    } linear_cache;

    /* Cached data for TEXTURE - fast pixel access */
    struct {
      ULONG *pixels;    /* Direct pointer to texture pixel data */
      int pitch_pixels; /* Pitch in pixels (not bytes) */
      int src_x, src_y; /* Source rectangle start coordinates */
      int src_w, src_h; /* Source rectangle dimensions */
    } texture_cache;

    /* Cached data for PATTERN - pre-computed colors */
    struct {
      ULONG fg_color;   /* Foreground color (ARGB32) */
      ULONG bg_color;   /* Background color (ARGB32) */
    } pattern_cache;
  } internal;

  union {
    struct {
      ULONG color;
    } solid;
    struct {
      struct ZuneTexture *texture;
      struct ZuneRect source;
      enum ZuneBrushWrapMode wrap_u;
      enum ZuneBrushWrapMode wrap_v;
      enum ZuneBrushFilterMode filter;
    } texture;
    struct {
      struct ZunePoint start;
      struct ZunePoint end;
      const struct ZuneGradientStop *stops;
      UWORD stop_count;
    } linear;
    struct {
      struct ZunePoint center;
      UWORD radius;
      const struct ZuneGradientStop *stops;
      UWORD stop_count;
    } radial;
    struct {
      LONG pen;         /* Pen index as understood by the target RastPort */
      BOOL release_pen; /* TRUE if ZuneRenderer should release the pen */
      ULONG reserved;   /* Reserved for future use */
    } pen;
    struct {
      LONG fg_pen;           /* Foreground pen index */
      LONG bg_pen;           /* Background pen index */
      const UWORD *pattern;  /* Pointer to 2-row pattern data (16 bits each) */
      struct ColorMap *colormap; /* ColorMap for pen-to-RGB conversion */
    } pattern;
    struct {
      struct ZuneTexture *texture;      /* Texture created from datatype */
      struct ZuneRect source;           /* Optional source sub-rect */
      enum ZuneBrushWrapMode wrap_u;    /* Horizontal wrapping mode */
      enum ZuneBrushWrapMode wrap_v;    /* Vertical wrapping mode */
    } datatype;
  } data;
};

#define ZUNE_BRUSH_LITERAL_SOLID(color_value)                                  \
  ((struct ZuneBrush){                                                   \
      .type = ZUNE_BRUSH_TYPE_SOLID,                                           \
      .flags = 0,                                                              \
      .internal = {0},                                                         \
      .data = {.solid = {.color = (ULONG)(color_value)}}})

#define ZUNE_BRUSH_SOLID(color_value) (&ZUNE_BRUSH_LITERAL_SOLID(color_value))

#define ZUNE_BRUSH_LITERAL_PEN_EX(pen_value, should_release)                   \
  ((struct ZuneBrush){.type = ZUNE_BRUSH_TYPE_PEN,                       \
                            .flags = 0,                                        \
                            .internal = {0},                                   \
                            .data = {.pen = {.pen = (LONG)(pen_value),         \
                                             .release_pen = (should_release),  \
                                             .reserved = 0}}})

#define ZUNE_BRUSH_LITERAL_PEN(pen_value)                                      \
  ZUNE_BRUSH_LITERAL_PEN_EX((pen_value), FALSE)

#define ZUNE_BRUSH_PEN(pen_value) (&ZUNE_BRUSH_LITERAL_PEN(pen_value))

/* Helper macros for convenient literal construction */
#define ZUNE_RECT_LITERAL(x, y, w, h)                                          \
  ((struct ZuneRect){(WORD)(x), (WORD)(y), (UWORD)(w), (UWORD)(h)})
#define ZUNE_POINT_LITERAL(x, y) ((struct ZunePoint){(WORD)(x), (WORD)(y)})
#define ZUNE_RECT_PTR(x, y, w, h)                                              \
  (&(struct ZuneRect){(WORD)(x), (WORD)(y), (UWORD)(w), (UWORD)(h)})
#define ZUNE_POINT_PTR(x, y) (&(struct ZunePoint){(WORD)(x), (WORD)(y)})

/* Convenience macros for coordinate-based calls */
#define ZuneDrawRectangleXYWH(rp, x, y, w, h, brush)                           \
  ZuneFillRectangle((rp), ZUNE_RECT_PTR((x), (y), (w), (h)), (brush))
#define ZuneDrawRectangleRoundedXYWH(rp, x, y, w, h, radius, brush)            \
  ZuneFillRectangleRounded((rp), ZUNE_RECT_PTR((x), (y), (w), (h)), (radius),  \
                           (brush))
#define ZuneDrawRectangleOutlineXYWH(rp, x, y, w, h, color)                    \
  ZuneDrawRectangleOutline((rp), ZUNE_RECT_PTR((x), (y), (w), (h)), (color))
#define ZuneDrawRectangleRoundedOutlineXYWH(rp, x, y, w, h, radius, color)     \
  ZuneDrawRectangleRoundedOutline((rp), ZUNE_RECT_PTR((x), (y), (w), (h)),     \
                                  (radius), (color))
#define ZuneDrawRectangleOutlineStyledXYWH(rp, x, y, w, h, lineWidth, color)   \
  ZuneDrawRectangleOutlineStyled((rp), ZUNE_RECT_PTR((x), (y), (w), (h)),      \
                                 (lineWidth), (color))
#define ZuneDrawRectangleRoundedOutlineStyledXYWH(rp, x, y, w, h, radius,      \
                                                  lineWidth, color)            \
  ZuneDrawRectangleRoundedOutlineStyled(                                       \
      (rp), ZUNE_RECT_PTR((x), (y), (w), (h)), (radius), (lineWidth), (color))
#define ZuneDrawRectangleRoundedStyledXYWH(rp, x, y, w, h, radius, borderWidth,\
                                           fillBrush, borderColor)             \
  ZuneDrawRectangleRoundedStyled((rp), ZUNE_RECT_PTR((x), (y), (w), (h)),      \
                                 (radius), (borderWidth), (fillBrush),         \
                                 (borderColor))
#define ZuneDrawCircleAt(rp, cx, cy, radius, brush)                            \
  ZuneFillCircle((rp), ZUNE_POINT_PTR((cx), (cy)), (radius), (brush))
#define ZuneDrawCircleOutlineAt(rp, cx, cy, radius, color)                     \
  ZuneDrawCircleOutline((rp), ZUNE_POINT_PTR((cx), (cy)), (radius), (color))
#define ZuneDrawCircleOutlineStyledAt(rp, cx, cy, radius, lineWidth, color)    \
  ZuneDrawCircleOutlineStyled((rp), ZUNE_POINT_PTR((cx), (cy)), (radius),      \
                              (lineWidth), (color))
#define ZuneDrawLinePoints(rp, startX, startY, endX, endY, color)              \
  ZuneDrawLine((rp), ZUNE_POINT_PTR((startX), (startY)),                       \
               ZUNE_POINT_PTR((endX), (endY)), (color))
#define ZuneDrawLineStyledPoints(rp, startX, startY, endX, endY, width, color) \
  ZuneDrawLineStyled((rp), ZUNE_POINT_PTR((startX), (startY)),                 \
                     ZUNE_POINT_PTR((endX), (endY)), (width), (color))
#define ZuneDrawPixelAt(rp, x, y, color)                                       \
  ZuneDrawPixel((rp), ZUNE_POINT_PTR((x), (y)), (color))
#define ZuneDrawCircleAAAt(rp, cx, cy, radius, color)                          \
  ZuneDrawCircleAA((rp), ZUNE_POINT_PTR((cx), (cy)), (radius), (color))
#define ZuneFillCircleAAAt(rp, cx, cy, radius, brush)                          \
  ZuneFillCircleAA((rp), ZUNE_POINT_PTR((cx), (cy)), (radius), (brush))
#define ZuneDrawCircleOutlineStyledAAAt(rp, cx, cy, radius, borderWidth,       \
                                        color)                                 \
  ZuneDrawCircleOutlineStyledAA((rp), ZUNE_POINT_PTR((cx), (cy)), (radius),    \
                                (borderWidth), (color))
#define ZuneFillCircleStyledAAAt(rp, cx, cy, radius, borderWidth, brush,       \
                                 borderColor)                                  \
  ZuneFillCircleStyledAA((rp), ZUNE_POINT_PTR((cx), (cy)), (radius),           \
                         (borderWidth), (brush), (borderColor))
#define ZuneFillRectangleRoundedAAXYWH(rp, x, y, w, h, radius, brush)          \
  ZuneFillRectangleRoundedAA((rp), ZUNE_RECT_PTR((x), (y), (w), (h)),          \
                             (radius), (brush))
#define ZuneFillRectangleRoundedStyledAAXYWH(                                  \
    rp, x, y, w, h, radius, borderWidth, fillBrush, borderColor)               \
  ZuneFillRectangleRoundedStyledAA((rp), ZUNE_RECT_PTR((x), (y), (w), (h)),    \
                                   (radius), (borderWidth), (fillBrush),       \
                                   (borderColor))
#define ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rp, x, y, w, h, radius,    \
                                                    lineWidth, color)          \
  ZuneDrawRectangleRoundedOutlineStyledAA(                                     \
      (rp), ZUNE_RECT_PTR((x), (y), (w), (h)), (radius), (lineWidth), (color))
#define ZuneDrawRectangleRoundedOutlineAAXYWH(rp, x, y, w, h, radius, color)   \
  ZuneDrawRectangleRoundedOutlineAA((rp), ZUNE_RECT_PTR((x), (y), (w), (h)),   \
                                    (radius), (color))
#define ZuneDrawLineAAPoints(rp, startX, startY, endX, endY, color)            \
  ZuneDrawLineAA((rp), ZUNE_POINT_PTR((startX), (startY)),                     \
                 ZUNE_POINT_PTR((endX), (endY)), (color))
#define ZuneDrawLineStyledAAPoints(rp, startX, startY, endX, endY, width,      \
                                   color)                                      \
  ZuneDrawLineStyledAA((rp), ZUNE_POINT_PTR((startX), (startY)),               \
                       ZUNE_POINT_PTR((endX), (endY)), (width), (color))
#define GetPixelAt(rp, x, y) GetPixel((rp), ZUNE_POINT_PTR((x), (y)))
#define SetPixelAt(rp, x, y, color)                                            \
  SetPixel((rp), ZUNE_POINT_PTR((x), (y)), (color))
#define BlitDrawingBoardRects(src, dst, src_x, src_y, dest_x, dest_y, width,   \
                              height)                                          \
  BlitDrawingBoard((src), (dst),                                               \
                   ZUNE_RECT_PTR((src_x), (src_y), (width), (height)),         \
                   ZUNE_RECT_PTR((dest_x), (dest_y), (width), (height)))
#define BlitDrawingBoardToRenderPortRects(src, dst, src_x, src_y, dest_x,      \
                                          dest_y, width, height)               \
  BlitDrawingBoardToRenderPort(                                                \
      (src), (dst), ZUNE_RECT_PTR((src_x), (src_y), (width), (height)),        \
      ZUNE_RECT_PTR((dest_x), (dest_y), (width), (height)))
#define ZuneCreateCircleRegionAt(cx, cy, radius)                               \
  ZuneCreateCircleRegion(ZUNE_POINT_PTR((cx), (cy)), (radius))
#define ZuneCreateRoundedRectRegionRect(x, y, w, h, radius)                    \
  ZuneCreateRoundedRectRegion(ZUNE_RECT_PTR((x), (y), (w), (h)), (radius))
#define ZuneCreateEllipseRegionAt(cx, cy, rx, ry)                              \
  ZuneCreateEllipseRegion(ZUNE_POINT_PTR((cx), (cy)), (rx), (ry))

/* Forward declarations */
struct DrawingBoard;
struct RenderPort;
struct PenCache;
struct ColorCache;
struct PenColorCache;
struct ZuneTexture;

/* Drawing Board - Off-screen rendering surface */
struct DrawingBoard {
  struct Node node; /* For linking in lists */

  /* Surface properties */
  struct BitMap *bitmap; /* Underlaying bitmap */
  struct RastPort *rastport;
  UWORD width, height;       /* Surface dimensions */
  UBYTE depth;               /* Color depth */
  ULONG flags;               /* ZUNE_DRAWINGBOARD_* flags */
  struct ColorMap *colormap; /* Associated colormap for indexed surfaces */

  /* Direct pixel access */
  APTR pixels;        /* Direct pixel buffer */
  APTR lock_handle;   /* Lock handle for pixel access */
  ULONG pitch;        /* Bytes per row */
  ULONG pixel_format; /* Pixel format identifier */
  BOOL pixels_locked; /* Pixel buffer lock state */

  /* Backend information */
  BOOL hardware_surface; /* True if in video memory */
  APTR backend_data;     /* Backend-specific data (e.g., FBO for OpenGL) */

  /* Parent window for OpenGL FBO support */
  struct Window *parent_window; /* Window this DrawingBoard belongs to (for GL context) */

  /* State */
  BOOL valid; /* Surface is ready for use */
};

/* Render Port - Rendering context */
struct RenderPort {
  struct Node node; /* For linking in lists */

  /* Window binding (required for OpenGL context) */
  struct Window *window;             /* Window this RenderPort belongs to */

  /* Target surfaces */
  struct RastPort *target_rp;        /* Target RastPort (screen/window) */
  struct DrawingBoard *target_board; /* Current target DrawingBoard (or NULL for window) */
  struct ColorMap *colormap;         /* Color mapping */
  ULONG pixel_format;                /* Pixel format identifier */

  /* Backend information */
  ULONG backend_type;   /* Active backend */
  APTR backend_context; /* Backend-specific context (GL context, etc.) */
  APTR backend_vtable;  /* Backend function table */
  APTR hidd_bitmap_obj; /* Cached HIDD bitmap object for direct operations */
  struct PenCache *pen_cache; /* Backend-specific pen cache (graphics) */
  struct ColorCache *color_cache;     /* Per-RenderPort color cache */
  struct PenColorCache *pen_color_cache; /* Per-RenderPort pen color cache */

  /* Batching system */
  BOOL batching_enabled; /* Batch operations for performance */
  APTR batch_state;      /* Batching context */

  /* Clipping */
  struct Region *clip_region; /* Clipping region */
  BOOL clipping_enabled;      /* Clipping active */

  /* State */
  BOOL valid; /* RenderPort ready for use */
};

/* Texture - Texture data for rendering */

struct ZuneTexture {
  struct Node node; /* For linking in lists */

  /* Texture properties */
  UWORD width, height; /* Texture dimensions */
  UBYTE depth;         /* Color depth in bits */
  ULONG format;        /* Pixel format (ZUNE_TEXTURE_FORMAT_*) */
  ULONG flags;         /* ZUNE_TEXTURE_* flags */

  /* Data storage */
  APTR pixel_data; /* Raw pixel data */
  ULONG data_size; /* Size of pixel data in bytes */
  ULONG pitch;     /* Bytes per row */

  /* Direct pixel access */
  APTR lock_handle;   /* Lock handle for pixel access */
  BOOL pixels_locked; /* Pixel buffer lock state */

  /* Backend integration */
  APTR backend_handle;   /* Backend-specific texture handle */
  BOOL hardware_texture; /* TRUE if stored in video memory */
  ULONG backend_type;    /* Backend that owns this texture */

  /* State and management */
  BOOL valid;      /* Texture is ready for use */
  ULONG ref_count; /* Reference counting for cleanup */

  /* Pre-tiled cache for optimized tiled rendering.
   * When a texture is used for tiled rendering (e.g., backgrounds),
   * we create a larger pre-tiled version (e.g., 256x256) on first use.
   * This avoids repeatedly tiling small textures across large areas.
   * Similar to the legacy BackFillInfo system in datatypescache.c.
   *
   * We store both a native BitMap (for hardware BltBitMap) and ARGB32 pixels
   * (for software fallback). The BitMap path is preferred when available. */
  struct BitMap *tiled_cache_bitmap;  /* Native pre-tiled bitmap for BltBitMap */
  APTR tiled_cache_pixels;            /* Pre-tiled pixel data (ARGB32) for software path */
  UWORD tiled_cache_width;            /* Width of pre-tiled cache (e.g., 256) */
  UWORD tiled_cache_height;           /* Height of pre-tiled cache (e.g., 256) */
  ULONG tiled_cache_pitch;            /* Bytes per row in pixel cache */
};

/*****************************************************************************/
/* RenderPort Management */
/*****************************************************************************/

/*
 * CreateRenderPortForWindow - Create a RenderPort bound to a Window
 *
 * This is the primary way to create a RenderPort. The RenderPort is bound
 * to the window and automatically selects the best backend if backend_type is NULL
 * (OpenGL if available, otherwise CyberGraphics).
 *
 * The window reference is required for OpenGL to create a GL context.
 */
struct RenderPort *CreateRenderPortForWindow(struct Window *window,
                                             struct ColorMap *colormap,
                                             UWORD backend_type);

/*
 * CreateDrawingBoardForRenderPort - Create DrawingBoard bound to a RenderPort
 *
 * The DrawingBoard always has a BitMap for legacy compatibility (SetAPen, etc).
 * If OpenGL is active, an FBO is also created for accelerated rendering.
 */
struct DrawingBoard *CreateDrawingBoardForRenderPort(struct RenderPort *rp,
                                                     UWORD width, UWORD height, ULONG flags);

void DestroyRenderPort(struct RenderPort *rp);
void ClearRenderPort(struct RenderPort *rp, ULONG color);

/*****************************************************************************/
/* DrawingBoard Management */
/*****************************************************************************/

/*
 * ZuneSetTarget - Switch render target
 *
 * board = NULL: Render to window's RastPort
 * board != NULL: Render to DrawingBoard
 *
 * For OpenGL: Uses glBindFramebuffer() for fast switching
 * For CyberGfx: Updates internal target pointer
 */
BOOL ZuneSetTarget(struct RenderPort *rp, struct DrawingBoard *board);

void DestroyDrawingBoard(struct DrawingBoard *board);
void ClearDrawingBoard(struct RenderPort *rp, ULONG color);

/*
 * SyncDrawingBoard - Sync backend buffer to DrawingBoard bitmap
 *
 * For OpenGL backend: Copies FBO contents to the DrawingBoard's bitmap.
 * This is required before using CyberGfx/graphics.library directly on
 * the DrawingBoard's bitmap after OpenGL rendering.
 *
 * For CyberGfx backend: No-op (bitmap is already the render target).
 */
BOOL SyncDrawingBoard(struct DrawingBoard *board);

/*****************************************************************************/
/* Core Drawing API - Clean and Simple */
/*****************************************************************************/

/* Basic filled shapes */
void ZuneDrawRectangle(struct RenderPort *rp, struct ZuneRect *rect,
                       const struct ZuneBrush *brush);
void ZuneDrawCircle(struct RenderPort *rp, struct ZunePoint *center,
                    UBYTE radius, const struct ZuneBrush *brush);

/* Shape outlines */
void ZuneDrawRectangleOutline(struct RenderPort *rp, struct ZuneRect *rect,
                              ULONG color);
void ZuneDrawCircleOutline(struct RenderPort *rp, struct ZunePoint *center,
                           UBYTE radius, ULONG color);

/* Styled versions - with line width control */
void ZuneDrawRectangleOutlineStyled(struct RenderPort *rp,
                                    struct ZuneRect *rect, UBYTE lineWidth,
                                    ULONG color);
void ZuneDrawCircleOutlineStyled(struct RenderPort *rp,
                                 struct ZunePoint *center, UBYTE radius,
                                 UBYTE lineWidth, ULONG color);

/* Rounded rectangles */
void ZuneDrawRectangleRounded(struct RenderPort *rp, struct ZuneRect *rect,
                              UBYTE cornerRadius,
                              const struct ZuneBrush *brush);
void ZuneDrawRectangleRoundedOutline(struct RenderPort *rp,
                                     struct ZuneRect *rect, UBYTE cornerRadius,
                                     ULONG color);
void ZuneDrawRectangleRoundedOutlineStyled(struct RenderPort *rp,
                                           struct ZuneRect *rect,
                                           UBYTE cornerRadius, UBYTE lineWidth,
                                           ULONG color);

/* Rounded rectangles with fill and border combined */
void ZuneDrawRectangleRoundedStyled(struct RenderPort *rp,
                                    struct ZuneRect *rect, UBYTE cornerRadius,
                                    UBYTE borderWidth,
                                    const struct ZuneBrush *fillBrush,
                                    ULONG borderColor);

/* Lines */
void ZuneDrawLine(struct RenderPort *rp, struct ZunePoint *start,
                  struct ZunePoint *end, ULONG color);
void ZuneDrawLineStyled(struct RenderPort *rp, struct ZunePoint *start,
                        struct ZunePoint *end, UWORD width, ULONG color);

/* Single pixel */
void ZuneDrawPixel(struct RenderPort *rp, struct ZunePoint *point, ULONG color);

/*****************************************************************************/
/* Antialiased Versions - All shapes support AA */
/*****************************************************************************/

/* Circle AA */
void ZuneDrawCircleAA(struct RenderPort *rp, struct ZunePoint *center,
                      UBYTE radius, ULONG color);
void ZuneFillCircleAA(struct RenderPort *rp, struct ZunePoint *center,
                      UWORD radius, const struct ZuneBrush *brush);
void ZuneFillCircleStyledAA(struct RenderPort *rp, struct ZunePoint *center,
                            UWORD radius, UBYTE borderWidth,
                            const struct ZuneBrush *brush, ULONG borderColor);
void ZuneDrawCircleOutlineStyledAA(struct RenderPort *rp,
                                   struct ZunePoint *center, UWORD radius,
                                   UBYTE borderWidth, ULONG color);

/* AA Quality */

void ZuneSetAntialiasingQuality(struct RenderPort *rp, UBYTE quality);
UBYTE ZuneGetAntialiasingQuality(struct RenderPort *rp);

/* Rounded rectangles AA */
void ZuneFillRectangleRoundedAA(struct RenderPort *rp, struct ZuneRect *rect,
                                UBYTE cornerRadius,
                                const struct ZuneBrush *brush);
void ZuneFillRectangleRoundedStyledAA(struct RenderPort *rp,
                                      struct ZuneRect *rect, UBYTE cornerRadius,
                                      UBYTE borderWidth,
                                      const struct ZuneBrush *fillBrush,
                                      ULONG borderColor);
void ZuneDrawRectangleRoundedOutlineStyledAA(struct RenderPort *rp,
                                             struct ZuneRect *rect,
                                             UBYTE cornerRadius,
                                             UBYTE lineWidth, ULONG color);
void ZuneDrawRectangleRoundedOutlineAA(struct RenderPort *rp,
                                       struct ZuneRect *rect,
                                       UBYTE cornerRadius, ULONG color);

/* Lines AA */
void ZuneDrawLineAA(struct RenderPort *rp, struct ZunePoint *start,
                    struct ZunePoint *end, ULONG color);
void ZuneDrawLineStyledAA(struct RenderPort *rp, struct ZunePoint *start,
                          struct ZunePoint *end, UBYTE width, ULONG color);

/*****************************************************************************/
/* Direct Pixel Access */
/*****************************************************************************/

APTR LockDrawingBoardPixels(struct RenderPort *rp, ULONG *pitch);
void UnlockDrawingBoardPixels(struct RenderPort *rp);
ULONG GetPixel(struct RenderPort *rp, struct ZunePoint *point);
void SetPixel(struct RenderPort *rp, struct ZunePoint *point, ULONG color);

/*****************************************************************************/
/* Performance and Batching */
/*****************************************************************************/

void BeginBatch(struct RenderPort *rp);
void EndBatch(struct RenderPort *rp);
void FlushBatch(struct RenderPort *rp);
BOOL IsBatchingEnabled(struct RenderPort *rp);
ULONG GetBatchCount(struct RenderPort *rp);

/*****************************************************************************/
/* Blitting and Surface Operations */
/*****************************************************************************/

void BlitDrawingBoard(struct DrawingBoard *source, struct DrawingBoard *target,
                      struct ZuneRect *src_rect, struct ZuneRect *dest_rect);
void BlitDrawingBoardToRenderPort(struct DrawingBoard *source,
                                  struct RenderPort *target,
                                  struct ZuneRect *src_rect,
                                  struct ZuneRect *dest_rect);

/*****************************************************************************/
/* Texture Management */
/*****************************************************************************/

struct ZuneTexture *CreateTexture(UWORD width, UWORD height, UBYTE depth,
                                  ULONG format, ULONG flags);
struct ZuneTexture *CreateTextureFromData(APTR data, UWORD width, UWORD height,
                                          UBYTE depth, ULONG format,
                                          ULONG pitch, ULONG flags);
struct ZuneTexture *CreateTextureFromDrawingBoard(struct RenderPort *rp,
                                                  ULONG flags);
struct ZuneTexture *CreateTextureFromDatatype(APTR dt_object, ULONG flags);
struct ZuneTexture *CreateTextureFromFile(CONST_STRPTR filename,
                                          struct Screen *screen,
                                          ULONG flags);
void DestroyTexture(struct ZuneTexture *texture);

/*****************************************************************************/
/* Texture Data Operations */
/*****************************************************************************/

BOOL UpdateTextureData(struct ZuneTexture *texture, APTR data,
                       struct ZuneRect *rect);
APTR LockTexturePixels(struct ZuneTexture *texture, ULONG *pitch);
void UnlockTexturePixels(struct ZuneTexture *texture);
ULONG GetTexturePixel(struct ZuneTexture *texture, struct ZunePoint *point);
void SetTexturePixel(struct ZuneTexture *texture, struct ZunePoint *point,
                     ULONG color);

/*****************************************************************************/
/* Texture Rendering */
/*****************************************************************************/

void ZuneDrawTexture(struct RenderPort *rp, struct ZuneTexture *texture,
                     struct ZunePoint *position);
void ZuneDrawTextureScaled(struct RenderPort *rp, struct ZuneTexture *texture,
                           struct ZuneRect *dest_rect);
void ZuneDrawTextureRegion(struct RenderPort *rp, struct ZuneTexture *texture,
                           struct ZuneRect *src_rect,
                           struct ZuneRect *dest_rect);
void ZuneDrawTextureTinted(struct RenderPort *rp, struct ZuneTexture *texture,
                           struct ZunePoint *position, ULONG tint_color);
void ZuneDrawTextureScaledTinted(struct RenderPort *rp,
                                 struct ZuneTexture *texture,
                                 struct ZuneRect *dest_rect, ULONG tint_color);
void ZuneDrawTextureRegionTinted(struct RenderPort *rp,
                                 struct ZuneTexture *texture,
                                 struct ZuneRect *src_rect,
                                 struct ZuneRect *dest_rect, ULONG tint_color);
void ZuneDrawTextureTiled(struct RenderPort *rp, struct ZuneTexture *texture,
                          struct ZuneRect *dest_rect);
BOOL ZuneIsTextureValid(struct ZuneTexture *texture);

/*****************************************************************************/
/* Clipping */
/*****************************************************************************/

BOOL ZuneSetClipRegion(struct RenderPort *rp, struct Region *region);
void ZuneClearClipRegion(struct RenderPort *rp);
struct Region *ZuneCombineRegions(struct Region *r1, struct Region *r2,
                                  UBYTE operation);
struct Region *ZuneCreateCircleRegion(struct ZunePoint *center, WORD radius);
struct Region *ZuneCreateRoundedRectRegion(struct ZuneRect *rect,
                                           WORD corner_radius);

/*****************************************************************************/
/* Color Utilities */
/*****************************************************************************/

ULONG RGBToColor(UBYTE r, UBYTE g, UBYTE b);
ULONG ARGBToColor(UBYTE a, UBYTE r, UBYTE g, UBYTE b);
ULONG BlendColors(ULONG color1, ULONG color2, UBYTE alpha);

/*****************************************************************************/
/* Cache Utilities */
/*****************************************************************************/

void ZuneInitPenCache(struct RenderPort *rp, LONG *pens, UWORD count);

/*****************************************************************************/
/* Window Blitting */
/*****************************************************************************/

/*
 * ZuneBlitToWindow - Blit DrawingBoard content directly to window
 *
 * Blits from the RenderPort's DrawingBoard to its associated window.
 * This eliminates the need for a separate WindowRenderPort.
 */
void ZuneBlitToWindow(struct RenderPort *rp, WORD src_x, WORD src_y,
                      WORD dst_x, WORD dst_y, UWORD width, UWORD height);

#endif /* LIBRARIES_ZUNERENDERER_H */
