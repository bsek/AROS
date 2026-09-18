/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Internal Definitions

    Clean internal interface for the unified target system and backend
    architecture. This header defines the structures and functions used
    internally by the library but not exposed in the public API.
*/

#ifndef ZUNEGFX_INTERN_H
#define ZUNEGFX_INTERN_H

#include "clib/graphics_protos.h"
#include "exec/semaphores.h"

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

#ifndef EXEC_LISTS_H
#include <exec/lists.h>
#endif

#ifndef EXEC_SEMAPHORES_H
#include <exec/semaphores.h>
#endif

#ifndef GRAPHICS_RASTPORT_H
#include <graphics/rastport.h>
#endif

#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif

#ifndef CYBERGRAPHX_CYBERGRAPHICS_H
#include <cybergraphx/cybergraphics.h>
#endif

#include "../include/zunegfx.h"

struct InternalColor {
  ULONG original_pixel; /* As provided */
  UBYTE a, r, g, b;     /* Individual components */
  LONG pen;             /* Allocated pen */
  BOOL pen_allocated;   /* TRUE if pen is allocated */
};

/*
 * Scratch a brush needs while it is being rendered. One per RenderContext:
 * callers usually pass a temporary brush, so this cannot live inside it.
 */
struct ZuneBrushCache {
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
  };

#define BRUSHCACHE(b) ((struct ZuneBrushCache *)(b)->internal)

/*****************************************************************************/
/* Object layouts -- private to the library                                  */
/*****************************************************************************/

/* Forward declarations */
struct DrawingBoard;
struct RenderContext;
struct PenCache;
struct ColorCache;
struct PenColorCache;
struct ZuneTexture;

/* Drawing Board - Off-screen rendering surface */
/* Where each component sits inside a locked pixel. Derived from the pixel
 * format the driver reports at lock time, so direct writes land in the
 * order that bitmap actually uses. */
struct ZunePixelLayout {
  UBYTE a_shift, r_shift, g_shift, b_shift; /* ULONG shifts */
  UBYTE bytes_per_pixel;
  BOOL has_alpha;
  BOOL valid; /* FALSE = do not write this buffer directly */
};

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
  struct ZunePixelLayout pixel_layout; /* Component order of the locked pixels */

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
  /* Scratch for whichever brush is currently being rendered */
  struct ZuneBrushCache brush_cache;

  BOOL batching_enabled; /* Batch operations for performance */
  APTR batch_state;      /* Batching context */

  /* Clipping */
  struct Region *clip_region; /* Clipping region */
  struct Region *saved_clip_region; /* Previous Layer clip region (restored on clear) */
  BOOL clipping_enabled;      /* Clipping active */
  BOOL clipping_installed;    /* We installed a region on the Layer */

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
/* Debug and Utility Macros */
/*****************************************************************************/

#ifdef DEBUG
#define ENTER_FUNCTION(name) D(bug("Entering: %s\n", name))
#define EXIT_FUNCTION(name) D(bug("Exiting: %s\n", name))
#else
#define ENTER_FUNCTION(name)
#define EXIT_FUNCTION(name)
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ABS(a) (((a) < 0) ? (-(a)) : (a))
#define CLAMP(x, min, max)                                                     \
  (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))

/* Library base casting macro */
#define ZRB(base) ((struct IntZuneGfxBase *)(base))

/*****************************************************************************/
/* Internal Color Structure */
/*****************************************************************************/

/*****************************************************************************/
/* Library Base Structure */
/*****************************************************************************/

struct IntZuneGfxBase {
  struct Library libnode;
  UWORD pad;

  struct SignalSemaphore lock;

  /* Resource tracking */
  struct List renderports;   /* List of active RenderContexts */
  struct List drawingboards; /* List of active DrawingBoards */
  struct List textures;      /* List of active ZuneTextures */
};

/*****************************************************************************/
/* Batch System Structures */
/*****************************************************************************/

/* Batch command types */
typedef enum {
  BATCH_CMD_FILL_RECT,
  BATCH_CMD_DRAW_RECT,
  BATCH_CMD_DRAW_LINE,
  BATCH_CMD_DRAW_PIXEL,
  BATCH_CMD_DRAW_RECT_STYLED
} BatchCommandType;

/* Batch command structure */
struct BatchCommand {
  BatchCommandType type;
  WORD x, y;           /* Primary coordinates */
  UWORD width, height; /* Dimensions */
  WORD x2, y2;         /* Secondary coordinates (for lines) */
  ULONG color;         /* Color value */
  LONG pen;            /* Allocated pen */
  ULONG sortKey;       /* For sorting optimization */
  UBYTE border_width;  /* Border width for styled outlines */
};

/* Batch constants */
#define IMMEDIATE_BATCH_SIZE 32
#define DEFERRED_BATCH_SIZE 256
#define PIXEL_BATCH_SIZE 128
#define PEN_CACHE_SIZE 64
#define COLOR_CACHE_SIZE 128
#define PEN_COLOR_CACHE_SIZE 64

/* Pen cache for optimizing pen allocation */
struct PenCache {
  struct ColorMap *colormap;
  ULONG entries[PEN_CACHE_SIZE]; /* Cached color->pen mappings */
  LONG pens[PEN_CACHE_SIZE];     /* Corresponding pen numbers */
  UBYTE count;                   /* Number of cached entries */
};

/* Color cache for optimizing ULONG to InternalColor conversion */
struct ColorCache {
  ULONG color_keys[COLOR_CACHE_SIZE];          /* Input ULONG colors */
  struct InternalColor colors[COLOR_CACHE_SIZE]; /* Cached InternalColor results */
  ULONG pixel_formats[COLOR_CACHE_SIZE];       /* Associated pixel formats */
  UBYTE count;                                 /* Number of cached entries */
  UBYTE next_slot;                             /* Next slot to use for replacement */
};

/* Pen color cache for optimizing pen to InternalColor conversion */
struct PenColorCache {
  LONG pen_keys[PEN_COLOR_CACHE_SIZE];         /* Input pen values */
  struct InternalColor colors[PEN_COLOR_CACHE_SIZE]; /* Cached InternalColor results */
  ULONG pixel_formats[PEN_COLOR_CACHE_SIZE];   /* Associated pixel formats */
  struct ColorMap *colormaps[PEN_COLOR_CACHE_SIZE]; /* Associated colormaps */
  UBYTE count;                                 /* Number of cached entries */
  UBYTE next_slot;                             /* Next slot to use for replacement */
};

/* Immediate batch for same-pen operations */
struct ImmediateBatch {
  struct BatchCommand commands[IMMEDIATE_BATCH_SIZE];
  UWORD count;
  LONG currentPen;
  BOOL penValid;
};

/* Deferred batch for mixed operations */
struct DeferredBatch {
  struct BatchCommand commands[DEFERRED_BATCH_SIZE];
  UWORD count;
  BOOL needsSort;
};

/* Pixel batch for direct pixel operations */
struct PixelBatch {
  APTR pixelBuffer;
  UWORD dirtyCount;
  BOOL needsFlush;
};

/* Complete batch state */
struct BatchState {
  struct ImmediateBatch immediate;
  struct DeferredBatch deferred;
  struct PixelBatch pixelBatch;
  struct PenCache penCache;
  struct ColorCache colorCache;
  struct PenColorCache penColorCache;

  BOOL active;
  struct RenderContext *render_port;
};

/*****************************************************************************/
/* Internal Function Prototypes */
/*****************************************************************************/
/* Backend types are defined in zunegfx.h */
typedef UWORD ZuneBackendType;

/* Batch state management */
struct BatchState *CreateBatchState(struct RenderContext *rctx);
void DestroyBatchState(struct BatchState *batch);

/* Pen cache management */
void InitPenCache(struct PenCache *cache, struct ColorMap *cmap);
void CleanupPenCache(struct PenCache *cache);
LONG GetCachedPen(struct PenCache *cache, ULONG color);

/* Color cache management */
void InitColorCache(struct ColorCache *cache);
void CleanupColorCache(struct ColorCache *cache);
BOOL GetCachedInternalColor(struct ColorCache *cache, ULONG color, ULONG pixel_format, struct InternalColor *out_color);
void CacheInternalColor(struct ColorCache *cache, ULONG color, ULONG pixel_format, const struct InternalColor *internal_color);

/* Pen color cache management */
void InitPenColorCache(struct PenColorCache *cache);
void CleanupPenColorCache(struct PenColorCache *cache);
BOOL GetCachedPenInternalColor(struct PenColorCache *cache, LONG pen, struct ColorMap *cmap, ULONG pixel_format, struct InternalColor *out_color);
void CachePenInternalColor(struct PenColorCache *cache, LONG pen, struct ColorMap *cmap, ULONG pixel_format, const struct InternalColor *internal_color);
BOOL ValidateRenderContext(struct RenderContext *rctx);
BOOL ValidateDrawingBoard(struct DrawingBoard *board);

/* Color management */
struct InternalColor ZuneColorToInternal(struct RenderContext *rctx, ULONG color,
                                         ULONG pixel_format);
BOOL ZuneBrushToInternalColor(struct RenderContext *rctx,
                              const struct ZuneBrush *brush,
                              struct InternalColor *out_color);
/* Drawingboard managment */
BOOL AllocateDrawingBoardBitmap(struct DrawingBoard *board,
                                ZuneBackendType backend_type,
                                struct BitMap *window_bitmap);
APTR LockDrawingBoardPixelsInternal(struct RenderContext *rctx, ULONG *pitch);
void UnlockDrawingBoardPixelsInternal(struct RenderContext *rctx);

/* State */
void CleanupZuneGfx(struct IntZuneGfxBase *base);
BOOL InitializeZuneGfx(struct IntZuneGfxBase *base);

/* Utilities */

/* Texture management */
void AddTextureToList(struct IntZuneGfxBase *base,
                      struct ZuneTexture *texture);
void RemoveTextureFromList(struct IntZuneGfxBase *base,
                           struct ZuneTexture *texture);
BOOL AllocateTextureData(struct ZuneTexture *texture);
void FreeTextureData(struct ZuneTexture *texture);
ULONG CalculateTexturePitch(UWORD width, ULONG format);
ULONG CalculateTextureSize(UWORD width, UWORD height, ULONG format);
struct ZuneTexture *
CreateTextureFromDataInternal(APTR data,
                              UWORD width, UWORD height, UBYTE depth,
                              ULONG format, ULONG pitch, ULONG flags);

/*****************************************************************************/
/* Global variables */
/*****************************************************************************/

extern struct List ZuneBackendList;

/* Library bases - declared in zunegfx_init.c or backend files */
extern struct GfxBase *GfxBase;
extern struct Library *CyberGfxBase;
extern struct Library *GLBase;

#endif /* ZUNEGFX_INTERN_H */
