/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Internal Definitions

    Clean internal interface for the unified target system and backend
    architecture. This header defines the structures and functions used
    internally by the library but not exposed in the public API.
*/

#ifndef ZUNEGFX_INTERN_H
#define ZUNEGFX_INTERN_H

#include "clib/graphics_protos.h"
#include "exec/semaphores.h"
#include <stdint.h>

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
  struct List renderports;   /* List of active RenderPorts */
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
  BATCH_CMD_DRAW_PIXEL
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
  struct RenderPort *render_port;
};

/*****************************************************************************/
/* Internal Function Prototypes */
/*****************************************************************************/
/* Backend types are defined in zunegfx.h */
typedef UWORD ZuneBackendType;

/* Batch state management */
struct BatchState *CreateBatchState(struct RenderPort *rp);
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
BOOL ValidateRenderPort(struct RenderPort *rp);
BOOL ValidateDrawingBoard(struct DrawingBoard *board);

/* Color management */
struct InternalColor ZuneColorToInternal(struct RenderPort *rp, ULONG color,
                                         ULONG pixel_format);
BOOL ZuneBrushToInternalColor(struct RenderPort *rp,
                              const struct ZuneBrush *brush,
                              struct InternalColor *out_color);
/* Drawingboard managment */
BOOL AllocateDrawingBoardBitmap(struct DrawingBoard *board,
                                ZuneBackendType backend_type,
                                struct BitMap *friend_bitmap);
APTR LockDrawingBoardPixelsInternal(struct RenderPort *rp, ULONG *pitch);
void UnlockDrawingBoardPixelsInternal(struct RenderPort *rp);

/* State */
void CleanupZuneRenderer(struct IntZuneGfxBase *base);
BOOL InitializeZuneRenderer(struct IntZuneGfxBase *base);

/* Utilities */
ULONG BlendColorsInternal(ULONG color1, ULONG color2, ULONG alpha);

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
