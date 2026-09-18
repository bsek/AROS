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
struct DrawingBoard;
struct RenderContext;

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

struct ZuneGradientStop {
  float position; /* 0.0 - 1.0 */
  ULONG color;
};

struct ZuneBrush {
  enum ZuneBrushType type;
  ULONG flags;

  /* Library-owned scratch set by the drawing calls. Opaque here: the brush
   * is usually a temporary, so the cache itself lives in the RenderContext. */
  APTR internal;

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
      .internal = NULL,                                                         \
      .data = {.solid = {.color = (ULONG)(color_value)}}})

#define ZUNE_BRUSH_SOLID(color_value) (&ZUNE_BRUSH_LITERAL_SOLID(color_value))

#define ZUNE_BRUSH_LITERAL_PEN_EX(pen_value, should_release)                   \
  ((struct ZuneBrush){.type = ZUNE_BRUSH_TYPE_PEN,                       \
                            .flags = 0,                                        \
                            .internal = NULL,                                   \
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
#define ZuneDrawLinePoints(rctx, startX, startY, endX, endY, style)              \
  ZuneDrawLine((rctx), ZUNE_POINT_PTR((startX), (startY)),                       \
               ZUNE_POINT_PTR((endX), (endY)), (style))
#define ZuneDrawPixelAt(rctx, x, y, color)                                       \
  ZuneDrawPixel((rctx), ZUNE_POINT_PTR((x), (y)), (color))
#define ZuneGetPixelAt(rctx, x, y) ZuneGetPixel((rctx), ZUNE_POINT_PTR((x), (y)))
#define ZuneSetPixelAt(rctx, x, y, color)                                            \
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

/*****************************************************************************/
/* Opaque objects                                                            */
/*****************************************************************************/
/* Shapes and textures                                                       */
/*****************************************************************************/

/*
 * How a shape is painted. A NULL fill leaves the interior alone; a zero
 * stroke_width leaves the edge alone; corner_radius applies to rectangles.
 *
 * Lines and polylines use the same struct: they are pure strokes, so they
 * read stroke_color, stroke_width and the flags and ignore the rest.
 */
#define ZUNE_SHAPE_ANTIALIAS  (1 << 0)

struct ZuneShapeStyle {
  const struct ZuneBrush *fill; /* NULL = no fill */
  ULONG stroke_color;           /* only read when stroke_width > 0 */
  UWORD stroke_width;           /* 0 = no stroke */
  UBYTE corner_radius;          /* rectangles only; 0 = square */
  ULONG flags;                  /* ZUNE_SHAPE_* */
};

/*
 * How a texture is painted. A NULL source takes the whole texture, a zero
 * tint means untinted, and a dest with zero width or height takes the
 * source's size.
 */
#define ZUNE_TEXTURE_TILE     (1 << 0)

struct ZuneTextureStyle {
  const struct ZuneRect *source; /* NULL = whole texture */
  ULONG tint;                    /* 0 = untinted */
  ULONG flags;                   /* ZUNE_TEXTURE_TILE */
};

/*
 * Style literals. One macro covers every combination, so the axes stay
 * parameters instead of turning into more entry points:
 *
 *   ZuneDrawRect(rctx, ZUNE_RECT_PTR(10, 10, 80, 40),
 *                ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_RED),
 *                           .corner_radius = 6,
 *                           .flags = ZUNE_SHAPE_ANTIALIAS));
 */
/*
 * How text is painted. A zero background means none, the same way a zero
 * tint means untinted.
 */
struct ZuneTextStyle {
  ULONG color;      /* glyph colour */
  ULONG background; /* 0 = draw no background */
  ULONG flags;      /* reserved */
};

#define ZUNE_STYLE(...)     (&(struct ZuneShapeStyle){__VA_ARGS__})
#define ZUNE_TEXSTYLE(...)  (&(struct ZuneTextureStyle){__VA_ARGS__})
#define ZUNE_TEXTSTYLE(...) (&(struct ZuneTextStyle){__VA_ARGS__})

void ZuneDrawRect(struct RenderContext *rctx, const struct ZuneRect *rect,
                  const struct ZuneShapeStyle *style);
void ZuneDrawCircle(struct RenderContext *rctx, const struct ZunePoint *center,
                    UWORD radius, const struct ZuneShapeStyle *style);
void ZuneDrawTextureStyled(struct RenderContext *rctx,
                           struct ZuneTexture *texture,
                           const struct ZuneRect *dest,
                           const struct ZuneTextureStyle *style);

/*****************************************************************************/

/*
 * RenderContext, DrawingBoard and ZuneTexture are owned by the library and
 * their layout is private. Use the accessors below to reach the few fields
 * that are useful from outside.
 */
struct RastPort *ZuneGetBoardRastPort(struct DrawingBoard *board);
struct BitMap *ZuneGetBoardBitMap(struct DrawingBoard *board);
void ZuneGetBoardSize(struct DrawingBoard *board, UWORD *width, UWORD *height);
struct RastPort *ZuneGetTargetRastPort(struct RenderContext *rctx);
struct DrawingBoard *ZuneGetTargetBoard(struct RenderContext *rctx);
ULONG ZuneGetBackendType(struct RenderContext *rctx);
void ZuneGetTextureSize(struct ZuneTexture *texture, UWORD *width,
                        UWORD *height);
ULONG ZuneGetTextureFlags(struct ZuneTexture *texture);
void ZuneRetainTexture(struct ZuneTexture *texture);

/*****************************************************************************/
/* RenderContext Management */
/*****************************************************************************/

/*
 * ZuneCreateRenderContextForWindow - Create a RenderContext bound to a Window
 *
 * This is the primary way to create a RenderContext. The RenderContext is bound
 * to the window and picks the best backend when backend_type is
 * BACKEND_BEST_AVAILABLE (OpenGL if present, otherwise CyberGraphics).
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

/*****************************************************************************/
/* Lines and pixels */
/*****************************************************************************/

/* Rectangles and circles go through ZuneDrawRect()/ZuneDrawCircle() above. */

/*
 * A line is a pure stroke: it reads stroke_color, stroke_width and the flags
 * from the style, so width and antialiasing are fields rather than separate
 * entry points.
 */
void ZuneDrawLine(struct RenderContext *rctx, struct ZunePoint *start,
                  struct ZunePoint *end, const struct ZuneShapeStyle *style);

/* Single pixel */
void ZuneDrawPixel(struct RenderContext *rctx, struct ZunePoint *point, ULONG color);

/*****************************************************************************/
/* Antialiasing */
/*****************************************************************************/

/* Everything that can be antialiased takes ZUNE_SHAPE_ANTIALIAS in its
 * style. The quality setting applies to all of them. */

void ZuneSetAntialiasingQuality(struct RenderContext *rctx, UBYTE quality);
UBYTE ZuneGetAntialiasingQuality(struct RenderContext *rctx);

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
/* Texture queries */
/*****************************************************************************/

/* Drawing goes through ZuneDrawTextureStyled() above. */

BOOL ZuneIsTextureValid(struct ZuneTexture *texture);

/*****************************************************************************/
/* Clipping */
/*****************************************************************************/

BOOL ZuneSetClipRegion(struct RenderContext *rctx, struct Region *region);
void ZuneClearClipRegion(struct RenderContext *rctx);
BOOL ZuneCombineRegions(struct RenderContext *rctx, struct Region *region);
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
 * ZuneDrawText - Draw text at a position
 *
 * Takes the glyph colour and an optional background from the style, so
 * background filling is a field rather than a second entry point.
 */
void ZuneDrawText(struct RenderContext *rctx, struct ZunePoint *position,
                  CONST_STRPTR string, UWORD count,
                  const struct ZuneTextStyle *style);

/* Convenience macro */
#define ZuneDrawTextAt(rctx, x, y, str, count, style) \
    ZuneDrawText((rctx), ZUNE_POINT_PTR((x),(y)), (str), (count), (style))

/*****************************************************************************/
/* Polyline and Polygon Drawing */
/*****************************************************************************/

/*
 * ZuneDrawPolyline - Connected line segments through a series of points
 *
 * Draws points[0] to points[1] to ... to points[count-1], count >= 2.
 * Strokes, so it reads stroke_color, stroke_width and the flags.
 */
void ZuneDrawPolyline(struct RenderContext *rctx, struct ZunePoint *points,
                      UWORD count, const struct ZuneShapeStyle *style);

/*
 * ZuneFillPolygon - Fill a closed polygon defined by a series of points
 *
 * The polygon closes itself, count >= 3. Takes the style's fill brush and
 * ZUNE_SHAPE_ANTIALIAS; a style without a fill draws nothing.
 */
void ZuneFillPolygon(struct RenderContext *rctx, struct ZunePoint *points,
                     UWORD count, const struct ZuneShapeStyle *style);

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

/* OpenGL interop */
APTR ZuneGetMasterGLContext(void);

struct RenderContext *ZuneCreateRenderContextA(struct TagItem *tags);
struct DrawingBoard *ZuneCreateDrawingBoardA(struct TagItem *tags);
struct ZuneTexture *ZuneCreateTextureTagList(struct TagItem *tags);

#endif /* LIBRARIES_ZUNEGFX_H */
