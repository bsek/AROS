#ifndef LIBRARIES_ZUNEGFX_H
#define LIBRARIES_ZUNEGFX_H

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Public API Header

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

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

/*****************************************************************************/
/* Library Information */
/*****************************************************************************/

#define ZUNEGFX_NAME "zunegfx.library"
#define ZUNEGFX_VERSION 1

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
/* Tag Definitions for Tag-Based Creation API                                */
/*****************************************************************************/

#define ZUNE_TAG_BASE       (TAG_USER + 0x00060000)

/* RenderContext creation tags (ZuneCreateRenderContextA) */
#define ZUNE_RenderContext_Window       (ZUNE_TAG_BASE + 1)   /* struct Window *     (required) */
#define ZUNE_RenderContext_ColorMap     (ZUNE_TAG_BASE + 2)   /* struct ColorMap *   (required) */
#define ZUNE_RenderContext_Backend      (ZUNE_TAG_BASE + 3)   /* UWORD backend type  (default: BACKEND_BEST_AVAILABLE) */

/* DrawingBoard creation tags (ZuneCreateDrawingBoardA) */
#define ZUNE_DrawingBoard_RenderContext (ZUNE_TAG_BASE + 16)  /* struct RenderContext * (required) */
#define ZUNE_DrawingBoard_Width        (ZUNE_TAG_BASE + 17)   /* UWORD               (required) */
#define ZUNE_DrawingBoard_Height       (ZUNE_TAG_BASE + 18)   /* UWORD               (required) */
#define ZUNE_DrawingBoard_Flags        (ZUNE_TAG_BASE + 19)   /* ULONG               (default: 0) */

/* Texture creation tags (ZuneCreateTextureA - unified) */
#define ZUNE_Texture_RenderContext     (ZUNE_TAG_BASE + 32)   /* struct RenderContext * (optional) */
#define ZUNE_Texture_Width             (ZUNE_TAG_BASE + 33)   /* UWORD               (required unless Source*) */
#define ZUNE_Texture_Height            (ZUNE_TAG_BASE + 34)   /* UWORD               (required unless Source*) */
#define ZUNE_Texture_Depth             (ZUNE_TAG_BASE + 35)   /* UBYTE               (default: 32) */
#define ZUNE_Texture_Format            (ZUNE_TAG_BASE + 36)   /* ULONG               (default: ARGB32) */
#define ZUNE_Texture_Flags             (ZUNE_TAG_BASE + 37)   /* ULONG               (default: 0) */
#define ZUNE_Texture_Data              (ZUNE_TAG_BASE + 38)   /* APTR pixel data     (optional) */
#define ZUNE_Texture_Pitch             (ZUNE_TAG_BASE + 39)   /* ULONG bytes/row     (required if Data) */
#define ZUNE_Texture_SourceDrawingBoard (ZUNE_TAG_BASE + 40)  /* BOOL - use rctx's target_board */
#define ZUNE_Texture_SourceDatatype    (ZUNE_TAG_BASE + 41)   /* APTR dt_object      (optional) */
#define ZUNE_Texture_SourceFile        (ZUNE_TAG_BASE + 42)   /* CONST_STRPTR        (optional) */
#define ZUNE_Texture_Screen            (ZUNE_TAG_BASE + 43)   /* struct Screen *     (for file loading) */

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
   * These fields are populated automatically by ZuneGfx when the brush
   * is used in rendering operations. They cache pre-computed data for optimal
   * performance in tight rendering loops. */
  struct {
    BOOL valid; /* TRUE if cache data is valid and up-to-date */

    struct InternalColor color;  /* Inline color cache (no allocation needed) */

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
      WORD pitch_pixels; /* Pitch in pixels (not bytes) */
      WORD src_x, src_y; /* Source rectangle start coordinates */
      UWORD src_w, src_h; /* Source rectangle dimensions */
    } texture_cache;

    /* Cached data for RADIAL_GRADIENT - pre-rasterized */
    struct {
      ULONG *rasterized_pixels;  /* ARGB32 pixel data, NULL if not cached */
      UWORD rasterized_width;    /* Width the cache was created for */
      UWORD rasterized_height;   /* Height the cache was created for */
      float center_x;            /* Absolute center X */
      float center_y;            /* Absolute center Y */
      float inv_radius;          /* 1.0f / radius for fast distance→t */
    } radial_cache;

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
      BOOL release_pen; /* TRUE if ZuneGfx should release the pen */
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
#define ZuneDrawRectangleXYWH(rctx, x, y, w, h, brush)                           \
  ZuneFillRectangle((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)), (brush))
#define ZuneDrawRectangleRoundedXYWH(rctx, x, y, w, h, radius, brush)            \
  ZuneFillRectangleRounded((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)), (radius),  \
                           (brush))
#define ZuneDrawRectangleOutlineXYWH(rctx, x, y, w, h, color)                    \
  ZuneDrawRectangleOutline((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)), (color))
#define ZuneDrawRectangleRoundedOutlineXYWH(rctx, x, y, w, h, radius, color)     \
  ZuneDrawRectangleRoundedOutline((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)),     \
                                  (radius), (color))
#define ZuneDrawRectangleOutlineStyledXYWH(rctx, x, y, w, h, lineWidth, color)   \
  ZuneDrawRectangleOutlineStyled((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)),      \
                                 (lineWidth), (color))
#define ZuneDrawRectangleRoundedOutlineStyledXYWH(rctx, x, y, w, h, radius,      \
                                                  lineWidth, color)            \
  ZuneDrawRectangleRoundedOutlineStyled(                                       \
      (rctx), ZUNE_RECT_PTR((x), (y), (w), (h)), (radius), (lineWidth), (color))
#define ZuneDrawRectangleRoundedStyledXYWH(rctx, x, y, w, h, radius, borderWidth,\
                                           fillBrush, borderColor)             \
  ZuneDrawRectangleRoundedStyled((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)),      \
                                 (radius), (borderWidth), (fillBrush),         \
                                 (borderColor))
#define ZuneDrawCircleAt(rctx, cx, cy, radius, brush)                            \
  ZuneFillCircle((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius), (brush))
#define ZuneDrawCircleOutlineAt(rctx, cx, cy, radius, color)                     \
  ZuneDrawCircleOutline((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius), (color))
#define ZuneDrawCircleOutlineStyledAt(rctx, cx, cy, radius, lineWidth, color)    \
  ZuneDrawCircleOutlineStyled((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius),      \
                              (lineWidth), (color))
#define ZuneDrawLinePoints(rctx, startX, startY, endX, endY, color)              \
  ZuneDrawLine((rctx), ZUNE_POINT_PTR((startX), (startY)),                       \
               ZUNE_POINT_PTR((endX), (endY)), (color))
#define ZuneDrawLineStyledPoints(rctx, startX, startY, endX, endY, width, color) \
  ZuneDrawLineStyled((rctx), ZUNE_POINT_PTR((startX), (startY)),                 \
                     ZUNE_POINT_PTR((endX), (endY)), (width), (color))
#define ZuneDrawPixelAt(rctx, x, y, color)                                       \
  ZuneDrawPixel((rctx), ZUNE_POINT_PTR((x), (y)), (color))
#define ZuneDrawCircleAAAt(rctx, cx, cy, radius, color)                          \
  ZuneDrawCircleAA((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius), (color))
#define ZuneFillCircleAAAt(rctx, cx, cy, radius, brush)                          \
  ZuneFillCircleAA((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius), (brush))
#define ZuneDrawCircleOutlineStyledAAAt(rctx, cx, cy, radius, borderWidth,       \
                                        color)                                 \
  ZuneDrawCircleOutlineStyledAA((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius),    \
                                (borderWidth), (color))
#define ZuneFillCircleStyledAAAt(rctx, cx, cy, radius, borderWidth, brush,       \
                                 borderColor)                                  \
  ZuneFillCircleStyledAA((rctx), ZUNE_POINT_PTR((cx), (cy)), (radius),           \
                         (borderWidth), (brush), (borderColor))
#define ZuneFillRectangleRoundedAAXYWH(rctx, x, y, w, h, radius, brush)          \
  ZuneFillRectangleRoundedAA((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)),          \
                             (radius), (brush))
#define ZuneFillRectangleRoundedStyledAAXYWH(                                  \
    rctx, x, y, w, h, radius, borderWidth, fillBrush, borderColor)               \
  ZuneFillRectangleRoundedStyledAA((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)),    \
                                   (radius), (borderWidth), (fillBrush),       \
                                   (borderColor))
#define ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, x, y, w, h, radius,    \
                                                    lineWidth, color)          \
  ZuneDrawRectangleRoundedOutlineStyledAA(                                     \
      (rctx), ZUNE_RECT_PTR((x), (y), (w), (h)), (radius), (lineWidth), (color))
#define ZuneDrawRectangleRoundedOutlineAAXYWH(rctx, x, y, w, h, radius, color)   \
  ZuneDrawRectangleRoundedOutlineAA((rctx), ZUNE_RECT_PTR((x), (y), (w), (h)),   \
                                    (radius), (color))
#define ZuneDrawLineAAPoints(rctx, startX, startY, endX, endY, color)            \
  ZuneDrawLineAA((rctx), ZUNE_POINT_PTR((startX), (startY)),                     \
                 ZUNE_POINT_PTR((endX), (endY)), (color))
#define ZuneDrawLineStyledAAPoints(rctx, startX, startY, endX, endY, width,      \
                                   color)                                      \
  ZuneDrawLineStyledAA((rctx), ZUNE_POINT_PTR((startX), (startY)),               \
                       ZUNE_POINT_PTR((endX), (endY)), (width), (color))
#define GetPixelAt(rctx, x, y) ZuneGetPixel((rctx), ZUNE_POINT_PTR((x), (y)))
#define SetPixelAt(rctx, x, y, color)                                            \
  ZuneSetPixel((rctx), ZUNE_POINT_PTR((x), (y)), (color))

/*
 * Blit helper macros - set target before blitting for convenience
 */

/* Blit from DrawingBoard to DrawingBoard */
#define ZuneBlitBoards(src_rctx, dst_rctx, src_x, src_y, dst_x, dst_y, w, h)       \
  do {                                                                         \
    ZuneSetTarget((src_rctx), (src_rctx)->target_board);                           \
    ZuneSetTarget((dst_rctx), (dst_rctx)->target_board);                           \
    ZuneBlit((src_rctx), (dst_rctx), (src_x), (src_y), (dst_x), (dst_y), (w), (h));\
  } while (0)

/* Blit from DrawingBoard to screen RastPort */
#define ZuneBlitBoardToScreen(src_rctx, dst_rctx, src_x, src_y, dst_x, dst_y, w, h)\
  do {                                                                         \
    ZuneSetTarget((src_rctx), (src_rctx)->target_board);                           \
    ZuneSetTarget((dst_rctx), NULL);                                             \
    ZuneBlit((src_rctx), (dst_rctx), (src_x), (src_y), (dst_x), (dst_y), (w), (h));\
  } while (0)

/* Blit from screen RastPort to DrawingBoard */
#define ZuneBlitScreenToBoard(src_rctx, dst_rctx, src_x, src_y, dst_x, dst_y, w, h)\
  do {                                                                         \
    ZuneSetTarget((src_rctx), NULL);                                             \
    ZuneSetTarget((dst_rctx), (dst_rctx)->target_board);                           \
    ZuneBlit((src_rctx), (dst_rctx), (src_x), (src_y), (dst_x), (dst_y), (w), (h));\
  } while (0)
#define ZuneCreateCircleRegionAt(cx, cy, radius)                               \
  ZuneCreateCircleRegion(ZUNE_POINT_PTR((cx), (cy)), (radius))
#define ZuneCreateRoundedRectRegionRect(x, y, w, h, radius)                    \
  ZuneCreateRoundedRectRegion(ZUNE_RECT_PTR((x), (y), (w), (h)), (radius))

/* Forward declarations */
struct DrawingBoard;
struct RenderContext;
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
struct RenderContext {
  struct Node node; /* For linking in lists */

  /* Window binding (required for OpenGL context) */
  struct Window *window;             /* Window this RenderContext belongs to */

  /* Target surfaces */
  struct RastPort *target_rastport;        /* Target RastPort (screen/window) */
  struct DrawingBoard *target_board; /* Current target DrawingBoard (or NULL for window) */
  struct ColorMap *colormap;         /* Color mapping */
  ULONG pixel_format;                /* Pixel format identifier */

  /* Backend information */
  ULONG backend_type;   /* Active backend */
  APTR backend_context; /* Backend-specific context (GL context, etc.) */
  APTR backend_vtable;  /* Backend function table */
  APTR hidd_bitmap_obj; /* Cached HIDD bitmap object for direct operations */
  struct PenCache *pen_cache; /* Backend-specific pen cache (graphics) */
  struct ColorCache *color_cache;     /* Per-RenderContext color cache */
  struct PenColorCache *pen_color_cache; /* Per-RenderContext pen color cache */

  /* Batching system */
  BOOL batching_enabled; /* Batch operations for performance */
  APTR batch_state;      /* Batching context */

  /* Clipping */
  struct Region *clip_region; /* Clipping region */
  BOOL clipping_enabled;      /* Clipping active */

  /* Text rendering */
  struct TextFont *font;      /* Current font for text operations */

  /* State */
  BOOL valid; /* RenderContext ready for use */
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
/* RenderContext Management */
/*****************************************************************************/

/*
 * ZuneCreateRenderContextForWindow - Create a RenderContext bound to a Window
 *
 * This is the primary way to create a RenderContext. The RenderContext is bound
 * to the window and automatically selects the best backend if backend_type is NULL
 * (OpenGL if available, otherwise CyberGraphics).
 *
 * The window reference is required for OpenGL to create a GL context.
 */
struct RenderContext *ZuneCreateRenderContextForWindow(struct Window *window,
                                             struct ColorMap *colormap,
                                             UWORD backend_type);

/*
 * ZuneCreateDrawingBoardForRenderContext - Create DrawingBoard bound to a RenderContext
 *
 * The DrawingBoard always has a BitMap for legacy compatibility (SetAPen, etc).
 * If OpenGL is active, an FBO is also created for accelerated rendering.
 */
struct DrawingBoard *ZuneCreateDrawingBoardForRenderContext(struct RenderContext *rctx,
                                                     UWORD width, UWORD height, ULONG flags);

void ZuneDestroyRenderContext(struct RenderContext *rctx);
void ZuneClearRenderContext(struct RenderContext *rctx, ULONG color);

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
BOOL ZuneSetTarget(struct RenderContext *rctx, struct DrawingBoard *board);

void ZuneDestroyDrawingBoard(struct RenderContext *rctx, struct DrawingBoard *board);
void ZuneClearDrawingBoard(struct RenderContext *rctx, ULONG color);

/* Note: SyncDrawingBoard has been removed. Use ZunePresent() or ZuneBlit()
 * instead - they automatically sync the backend buffer before blitting. */

/*****************************************************************************/
/* Core Drawing API - Clean and Simple */
/*****************************************************************************/

/* Basic filled shapes */
void ZuneDrawRectangle(struct RenderContext *rctx, struct ZuneRect *rect,
                       const struct ZuneBrush *brush);
void ZuneDrawCircle(struct RenderContext *rctx, struct ZunePoint *center,
                    UBYTE radius, const struct ZuneBrush *brush);

/* Shape outlines */
void ZuneDrawRectangleOutline(struct RenderContext *rctx, struct ZuneRect *rect,
                              ULONG color);
void ZuneDrawCircleOutline(struct RenderContext *rctx, struct ZunePoint *center,
                           UBYTE radius, ULONG color);

/* Styled versions - with line width control */
void ZuneDrawRectangleOutlineStyled(struct RenderContext *rctx,
                                    struct ZuneRect *rect, UBYTE lineWidth,
                                    ULONG color);
void ZuneDrawCircleOutlineStyled(struct RenderContext *rctx,
                                 struct ZunePoint *center, UBYTE radius,
                                 UBYTE lineWidth, ULONG color);

/* Rounded rectangles */
void ZuneDrawRectangleRounded(struct RenderContext *rctx, struct ZuneRect *rect,
                              UBYTE cornerRadius,
                              const struct ZuneBrush *brush);
void ZuneDrawRectangleRoundedOutline(struct RenderContext *rctx,
                                     struct ZuneRect *rect, UBYTE cornerRadius,
                                     ULONG color);
void ZuneDrawRectangleRoundedOutlineStyled(struct RenderContext *rctx,
                                           struct ZuneRect *rect,
                                           UBYTE cornerRadius, UBYTE lineWidth,
                                           ULONG color);

/* Rounded rectangles with fill and border combined */
void ZuneDrawRectangleRoundedStyled(struct RenderContext *rctx,
                                    struct ZuneRect *rect, UBYTE cornerRadius,
                                    UBYTE borderWidth,
                                    const struct ZuneBrush *fillBrush,
                                    ULONG borderColor);

/* Lines */
void ZuneDrawLine(struct RenderContext *rctx, struct ZunePoint *start,
                  struct ZunePoint *end, ULONG color);
void ZuneDrawLineStyled(struct RenderContext *rctx, struct ZunePoint *start,
                        struct ZunePoint *end, UWORD width, ULONG color);

/* Single pixel */
void ZuneDrawPixel(struct RenderContext *rctx, struct ZunePoint *point, ULONG color);

/*****************************************************************************/
/* Antialiased Versions - All shapes support AA */
/*****************************************************************************/

/* Circle AA */
void ZuneDrawCircleAA(struct RenderContext *rctx, struct ZunePoint *center,
                      UBYTE radius, ULONG color);
void ZuneFillCircleAA(struct RenderContext *rctx, struct ZunePoint *center,
                      UWORD radius, const struct ZuneBrush *brush);
void ZuneFillCircleStyledAA(struct RenderContext *rctx, struct ZunePoint *center,
                            UWORD radius, UBYTE borderWidth,
                            const struct ZuneBrush *brush, ULONG borderColor);
void ZuneDrawCircleOutlineStyledAA(struct RenderContext *rctx,
                                   struct ZunePoint *center, UWORD radius,
                                   UBYTE borderWidth, ULONG color);

/* AA Quality */

void ZuneSetAntialiasingQuality(struct RenderContext *rctx, UBYTE quality);
UBYTE ZuneGetAntialiasingQuality(struct RenderContext *rctx);

/* Rounded rectangles AA */
void ZuneFillRectangleRoundedAA(struct RenderContext *rctx, struct ZuneRect *rect,
                                UBYTE cornerRadius,
                                const struct ZuneBrush *brush);
void ZuneFillRectangleRoundedStyledAA(struct RenderContext *rctx,
                                      struct ZuneRect *rect, UBYTE cornerRadius,
                                      UBYTE borderWidth,
                                      const struct ZuneBrush *fillBrush,
                                      ULONG borderColor);
void ZuneDrawRectangleRoundedOutlineStyledAA(struct RenderContext *rctx,
                                             struct ZuneRect *rect,
                                             UBYTE cornerRadius,
                                             UBYTE lineWidth, ULONG color);
void ZuneDrawRectangleRoundedOutlineAA(struct RenderContext *rctx,
                                       struct ZuneRect *rect,
                                       UBYTE cornerRadius, ULONG color);

/* Lines AA */
void ZuneDrawLineAA(struct RenderContext *rctx, struct ZunePoint *start,
                    struct ZunePoint *end, ULONG color);
void ZuneDrawLineStyledAA(struct RenderContext *rctx, struct ZunePoint *start,
                          struct ZunePoint *end, UBYTE width, ULONG color);

/*****************************************************************************/
/* Direct Pixel Access */
/*****************************************************************************/

APTR ZuneLockDrawingBoardPixels(struct RenderContext *rctx, ULONG *pitch);
void ZuneUnlockDrawingBoardPixels(struct RenderContext *rctx);
ULONG ZuneGetPixel(struct RenderContext *rctx, struct ZunePoint *point);
void ZuneSetPixel(struct RenderContext *rctx, struct ZunePoint *point, ULONG color);

/*****************************************************************************/
/* Performance and Batching */
/*****************************************************************************/

void ZuneBeginBatch(struct RenderContext *rctx);
void ZuneEndBatch(struct RenderContext *rctx);
void ZuneFlushBatch(struct RenderContext *rctx);
BOOL ZuneIsBatchingEnabled(struct RenderContext *rctx);
ULONG ZuneGetBatchCount(struct RenderContext *rctx);

/*****************************************************************************/
/* Blitting and Surface Operations */
/*****************************************************************************/

/*
 * ZuneBlit - General-purpose blit between RenderContexts
 *
 * Handles all combinations:
 * - DrawingBoard to DrawingBoard
 * - DrawingBoard to screen RastPort
 * - Screen RastPort to DrawingBoard
 * - Screen RastPort to screen RastPort
 *
 * Automatically syncs OpenGL FBO to bitmap when source is a DrawingBoard.
 * Use the helper macros (ZuneBlitBoards, ZuneBlitBoardToScreen, etc.)
 * for convenience when you need to set targets before blitting.
 */
void ZuneBlit(struct RenderContext *src_rctx, struct RenderContext *dst_rctx,
              WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
              UWORD width, UWORD height);

/*
 * ZunePresent - Present DrawingBoard content to window
 *
 * This is the primary function for double-buffered rendering.
 * Call it to display what has been rendered to the DrawingBoard.
 * Automatically syncs OpenGL FBO to bitmap before blitting.
 */
void ZunePresent(struct RenderContext *rctx,
                 WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                 UWORD width, UWORD height);

/*
 * ZuneCapture - Capture pixels from a RastPort into DrawingBoard
 *
 * Used to read background content for proper alpha blending
 * when drawing antialiased graphics over existing content.
 *
 * Parameters:
 *   rctx - RenderContext with a DrawingBoard target
 *   src_rp - Source RastPort to read from (e.g., window RastPort or double buffer)
 *   src_x, src_y - Source coordinates in the RastPort
 *   dst_x, dst_y - Destination coordinates in the DrawingBoard
 *   width, height - Size of area to capture
 */
void ZuneCapture(struct RenderContext *rctx, struct RastPort *src_rp,
                 WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                 UWORD width, UWORD height);

/**
 * Synchronize backend render buffer to DrawingBoard's bitmap.
 * For OpenGL: copies FBO to bitmap. For CyberGfx: no-op.
 * Call after Zune drawing and before direct bitmap operations.
 */
BOOL ZuneSync(struct RenderContext *rctx);

/**
 * Reload backend render buffer from DrawingBoard's bitmap.
 * For OpenGL: uploads bitmap to FBO. For CyberGfx: no-op.
 * Call after direct bitmap operations and before Zune drawing.
 * This is the inverse of ZuneSync().
 */
BOOL ZuneReload(struct RenderContext *rctx);

/*****************************************************************************/
/* Texture Management */
/*****************************************************************************/

struct ZuneTexture *ZuneCreateTexture(struct RenderContext *rctx, UWORD width,
                                  UWORD height, UBYTE depth, ULONG format,
                                  ULONG flags);
struct ZuneTexture *ZuneCreateTextureFromData(struct RenderContext *rctx, APTR data,
                                          UWORD width, UWORD height,
                                          UBYTE depth, ULONG format,
                                          ULONG pitch, ULONG flags);
struct ZuneTexture *ZuneCreateTextureFromDrawingBoard(struct RenderContext *rctx,
                                                  ULONG flags);
struct ZuneTexture *ZuneCreateTextureFromDatatype(struct RenderContext *rctx,
                                              APTR dt_object, ULONG flags);
struct ZuneTexture *ZuneCreateTextureFromFile(struct RenderContext *rctx,
                                          CONST_STRPTR filename,
                                          struct Screen *screen,
                                          ULONG flags);
void ZuneDestroyTexture(struct RenderContext *rctx, struct ZuneTexture *texture);

/*****************************************************************************/
/* Texture Data Operations */
/*****************************************************************************/

BOOL ZuneUpdateTextureData(struct RenderContext *rctx, struct ZuneTexture *texture,
                       APTR data, struct ZuneRect *rect);
APTR ZuneLockTexturePixels(struct RenderContext *rctx, struct ZuneTexture *texture,
                       ULONG *pitch);
void ZuneUnlockTexturePixels(struct RenderContext *rctx, struct ZuneTexture *texture);
ULONG ZuneGetTexturePixel(struct RenderContext *rctx, struct ZuneTexture *texture,
                      struct ZunePoint *point);
void ZuneSetTexturePixel(struct RenderContext *rctx, struct ZuneTexture *texture,
                     struct ZunePoint *point, ULONG color);

/*****************************************************************************/
/* Texture Rendering */
/*****************************************************************************/

void ZuneDrawTexture(struct RenderContext *rctx, struct ZuneTexture *texture,
                     struct ZunePoint *position);
void ZuneDrawTextureScaled(struct RenderContext *rctx, struct ZuneTexture *texture,
                           struct ZuneRect *dest_rect);
void ZuneDrawTextureRegion(struct RenderContext *rctx, struct ZuneTexture *texture,
                           struct ZuneRect *src_rect,
                           struct ZuneRect *dest_rect);
void ZuneDrawTextureTinted(struct RenderContext *rctx, struct ZuneTexture *texture,
                           struct ZunePoint *position, ULONG tint_color);
void ZuneDrawTextureScaledTinted(struct RenderContext *rctx,
                                 struct ZuneTexture *texture,
                                 struct ZuneRect *dest_rect, ULONG tint_color);
void ZuneDrawTextureRegionTinted(struct RenderContext *rctx,
                                 struct ZuneTexture *texture,
                                 struct ZuneRect *src_rect,
                                 struct ZuneRect *dest_rect, ULONG tint_color);
void ZuneDrawTextureTiled(struct RenderContext *rctx, struct ZuneTexture *texture,
                          struct ZuneRect *dest_rect);
BOOL ZuneIsTextureValid(struct ZuneTexture *texture);

/*****************************************************************************/
/* Clipping */
/*****************************************************************************/

BOOL ZuneSetClipRegion(struct RenderContext *rctx, struct Region *region);
void ZuneClearClipRegion(struct RenderContext *rctx);
struct Region *ZuneCombineRegions(struct Region *r1, struct Region *r2,
                                  UBYTE operation);
struct Region *ZuneCreateCircleRegion(struct ZunePoint *center, WORD radius);
struct Region *ZuneCreateRoundedRectRegion(struct ZuneRect *rect,
                                           WORD corner_radius);

/*****************************************************************************/
/* Text Rendering */
/*****************************************************************************/

/*
 * ZuneSetFont - Set the current font for text operations
 *
 * Accepts any TextFont* including TrueType fonts loaded via OpenDiskFont().
 * The font is stored on the RenderContext and applied when text is drawn.
 */
void ZuneSetFont(struct RenderContext *rctx, struct TextFont *font);

/*
 * ZuneTextLength - Measure the pixel width of a string
 *
 * Returns the pixel width of 'count' characters from 'string' using the
 * current font set via ZuneSetFont(). Accounts for kerning and spacing.
 */
UWORD ZuneTextLength(struct RenderContext *rctx, CONST_STRPTR string, UWORD count);

/*
 * ZuneTextFit - Determine how many characters fit in a pixel width
 *
 * Returns the number of characters from 'string' that fit within 'maxWidth'
 * pixels using the current font.
 */
UWORD ZuneTextFit(struct RenderContext *rctx, CONST_STRPTR string, UWORD count, UWORD maxWidth);

/*
 * ZuneDrawText - Draw text at a position with foreground color
 *
 * Position.y is the TOP of the text line (not baseline).
 * The implementation adds tf_Baseline internally.
 * Draws with transparent background (JAM1 mode).
 */
void ZuneDrawText(struct RenderContext *rctx, struct ZunePoint *position,
                  CONST_STRPTR string, UWORD count, ULONG color);

/*
 * ZuneDrawTextBackground - Draw text with foreground and background colors
 *
 * Same as ZuneDrawText but fills the text background with bgColor (JAM2 mode).
 * Useful for text selections and highlighted text.
 */
void ZuneDrawTextBackground(struct RenderContext *rctx, struct ZunePoint *position,
                            CONST_STRPTR string, UWORD count,
                            ULONG fgColor, ULONG bgColor);

/* Convenience macros */
#define ZuneDrawTextAt(rctx, x, y, str, count, color) \
    ZuneDrawText((rctx), ZUNE_POINT_PTR((x),(y)), (str), (count), (color))
#define ZuneDrawTextBackgroundAt(rctx, x, y, str, count, fg, bg) \
    ZuneDrawTextBackground((rctx), ZUNE_POINT_PTR((x),(y)), (str), (count), (fg), (bg))

/*****************************************************************************/
/* Polyline and Polygon Drawing */
/*****************************************************************************/

/*
 * ZuneDrawPolyline - Draw connected line segments through a series of points
 *
 * Draws lines from points[0]→points[1]→...→points[count-1].
 * Requires count >= 2. Uses 1-pixel line width.
 */
void ZuneDrawPolyline(struct RenderContext *rctx, struct ZunePoint *points,
                      UWORD count, ULONG color);

/*
 * ZuneDrawPolylineStyled - Draw connected line segments with custom width
 */
void ZuneDrawPolylineStyled(struct RenderContext *rctx, struct ZunePoint *points,
                            UWORD count, UWORD lineWidth, ULONG color);

/*
 * ZuneFillPolygon - Fill a closed polygon defined by a series of points
 *
 * The polygon is automatically closed (last point connects to first).
 * Requires count >= 3. Uses scanline fill algorithm.
 */
void ZuneFillPolygon(struct RenderContext *rctx, struct ZunePoint *points,
                     UWORD count, const struct ZuneBrush *brush);

/*****************************************************************************/
/* Color Utilities */
/*****************************************************************************/

ULONG ZuneRGBToColor(UBYTE r, UBYTE g, UBYTE b);
ULONG ZuneARGBToColor(UBYTE a, UBYTE r, UBYTE g, UBYTE b);
ULONG ZuneBlendColors(ULONG color1, ULONG color2, UBYTE alpha);

/*****************************************************************************/
/* Cache Utilities */
/*****************************************************************************/

void ZuneInitPenCache(struct RenderContext *rctx, LONG *pens, UWORD count);

/*****************************************************************************/
/* Tag-Based Creation API                                                    */
/*****************************************************************************/

struct RenderContext *ZuneCreateRenderContextA(struct TagItem *tags);
struct DrawingBoard *ZuneCreateDrawingBoardA(struct TagItem *tags);
struct ZuneTexture *ZuneCreateTextureA(struct TagItem *tags);

#endif /* LIBRARIES_ZUNEGFX_H */
