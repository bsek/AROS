#ifndef ZUNERENDERER_BACKEND_INTERFACE_H
#define ZUNERENDERER_BACKEND_INTERFACE_H

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - New Unified Backend Interface

    This header defines the new unified backend interface that works directly
    with ZuneTarget structures. This eliminates the need for dispatch layers
    and provides optimal performance by removing unnecessary branching.

    Key improvements:
    - Single interface for all target types
    - Direct ZuneTarget parameter passing
    - Consolidated drawing operations
    - Unified antialiasing support
    - Performance-optimized function signatures
*/

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"
#include "graphics/gfx.h"

/*****************************************************************************/
/* Backend Type Enumeration */
/*****************************************************************************/

/*****************************************************************************/
/* Backend Capability Flags */
/*****************************************************************************/

#define BACKEND_CAP_BASIC (1L << 0)        /* Basic drawing operations */
#define BACKEND_CAP_CYBERGFX (1L << 1)     /* CyberGraphics operations */
#define BACKEND_CAP_HARDWARE (1L << 2)     /* Hardware acceleration */
#define BACKEND_CAP_DIRECTPIXEL (1L << 3)  /* Direct pixel access */
#define BACKEND_CAP_ANTIALIASING (1L << 4) /* Anti-aliasing support */
#define BACKEND_CAP_OPENGL (1L << 5)       /* OpenGL operations */
#define BACKEND_CAP_BLENDING (1L << 6)     /* Alpha blending */
#define BACKEND_CAP_BATCHING (1L << 7)     /* Command batching */
#define BACKEND_CAP_CIRCLES (1L << 8)      /* Native circle support */
#define BACKEND_CAP_ROUNDED_RECTS                                              \
  (1L << 9)                                      /* Native rounded rectangles  \
                                                  */
#define BACKEND_CAP_TEXTURES (1L << 10)          /* Texture support */
#define BACKEND_CAP_TEXTURE_FILTERING (1L << 11) /* Texture filtering */
#define BACKEND_CAP_TEXTURE_MIPMAPS (1L << 12)   /* Texture mipmaps */
#define BACKEND_CAP_TEXTURE_SCALING (1L << 13)   /* Hardware texture scaling */

/*****************************************************************************/
/* Backend Context Structure */
/*****************************************************************************/

typedef struct ZuneBackendContext {
  ZuneBackendType type; /* Backend type identifier */
  ULONG capabilities;   /* Capability flags */
  CONST_STRPTR name;    /* Human-readable backend name */
  BOOL initialized;     /* Backend initialization status */
  APTR private_data;    /* Backend-specific private data */
  ULONG ref_count;      /* Reference count for cleanup */
} ZuneBackendContext;

/*****************************************************************************/
/* Unified Backend Interface */
/*****************************************************************************/

typedef struct ZuneBackendOps {
  /* Backend identification and lifecycle */
  CONST_STRPTR name;              /* Backend name */
  ZuneBackendType type;           /* Backend type */
  ULONG (*GetCapabilities)(void); /* Get capability flags */

  /* Backend lifecycle management */
  BOOL (*InitBackend)(ZuneBackendContext *ctx);    /* Initialize backend */
  void (*CleanupBackend)(ZuneBackendContext *ctx); /* Cleanup backend */
  BOOL (*IsAvailable)(void); /* Check if backend is available */
  BOOL(*IsCompatible)
  (struct RenderContext
       *rctx); /* Check if RenderContext is compatible with this backend */
  ULONG (*GetPixelFormat)(struct BitMap *bitmap);

  /* RenderContext lifecycle management */
  BOOL(*InitRenderContext)
  (struct RenderContext *rctx); /* Initialize RenderContext for this backend */
  void (*CleanupRenderContext)(
      struct RenderContext *rctx); /* Cleanup RenderContext resources */

  /* Color management */
  BOOL (*PrepareColor)(struct RenderContext *rctx, struct InternalColor *color);
  void (*ReleaseColor)(struct RenderContext *rctx, struct InternalColor *color);

  /* Core drawing primitives - unified interface */
  void (*DrawPixel)(struct RenderContext *rctx, WORD x, WORD y,
                    struct InternalColor *color, BOOL antialias);

  void (*DrawLine)(struct RenderContext *rctx, WORD start_x, WORD start_y,
                   WORD end_x, WORD end_y, UWORD line_width,
                   struct InternalColor *color, BOOL antialias);

  void (*DrawRectangle)(struct RenderContext *rctx, WORD x, WORD y, UWORD width,
                        UWORD height, UBYTE border_width, UBYTE corner_radius,
                        struct ZuneBrush *fill_brush,
                        struct InternalColor *border_color, BOOL filled,
                        BOOL antialias);

  void (*DrawCircle)(struct RenderContext *rctx, WORD center_x, WORD center_y,
                     UWORD radius, UBYTE borderWidth,
                     struct ZuneBrush *fill_brush,
                     struct InternalColor *border_color, BOOL filled,
                     BOOL antialias);

  /* Surface operations */
  void (*ClearRenderContext)(struct RenderContext *rctx, struct InternalColor *color);

  /* Direct pixel access (when RenderContext targets DrawingBoard) */
  APTR (*LockPixels)(struct DrawingBoard *board, ULONG *pitch_out);
  void (*UnlockPixels)(struct DrawingBoard *board);
  ULONG (*GetPixel)(struct DrawingBoard *board, WORD x, WORD y);
  void (*SetPixel)(struct DrawingBoard *board, WORD x, WORD y,
                   struct InternalColor *color);

  /* Performance optimization */
  void (*BeginBatch)(struct RenderContext *rctx); /* Begin batching operations */
  void (*EndBatch)(struct RenderContext *rctx);   /* End and flush batch */
  void (*FlushBatch)(struct RenderContext *rctx); /* Flush current batch */
  BOOL (*IsBatching)(struct RenderContext *rctx); /* Check if batching is active */

  /* Blitting operations */
  void (*BlitRenderContexts)(struct RenderContext *source, struct RenderContext *dest,
                          WORD src_x, WORD src_y, WORD dest_x, WORD dest_y,
                          UWORD width, UWORD height);

  void (*BlitToScreen)(struct RenderContext *source, struct RastPort *screen_rp,
                       WORD src_x, WORD src_y, WORD dest_x, WORD dest_y,
                       UWORD width, UWORD height);

  /* DrawingBoard operations */
  BOOL (*InitDrawingBoard)(struct DrawingBoard *board);
  void (*CleanupDrawingBoard)(struct DrawingBoard *board);

  /* Texture operations */
  BOOL (*InitTexture)(struct ZuneTexture *texture);
  void (*CleanupTexture)(struct ZuneTexture *texture);
  BOOL(*UpdateTexture)
  (struct ZuneTexture *texture, APTR data, UWORD x, UWORD y, UWORD width,
   UWORD height);
  void (*DrawTexture)(struct RenderContext *rctx, struct ZuneTexture *texture,
                      WORD dest_x, WORD dest_y, UWORD dest_width,
                      UWORD dest_height, WORD src_x, WORD src_y,
                      UWORD src_width, UWORD src_height,
                      struct InternalColor *tint);
  APTR (*LockTexturePixels)(struct ZuneTexture *texture, ULONG *pitch);
  void (*UnlockTexturePixels)(struct ZuneTexture *texture);
  ULONG (*GetTexturePixel)(struct ZuneTexture *texture, WORD x, WORD y);
  void (*SetTexturePixel)(struct ZuneTexture *texture, WORD x, WORD y,
                          struct InternalColor *color);

  /* Texture capabilities */
  ULONG (*GetMaxTextureSize)(void);
  BOOL (*SupportsTextureFormat)(ULONG format);

  /* Clipping support */
  BOOL (*SetupClipping)(struct RenderContext *rctx, struct Region *region);
  void (*ClearClipping)(struct RenderContext *rctx); // For hardware-based backends

  /* RastPort copy (Sync RastPort into DrawingBoard) */
  void (*CopyFromRastPort)(struct RenderContext *rctx, struct RastPort *src_rp,
                           WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                           UWORD width, UWORD height);
  BOOL (*CopyFromDrawingBoard)(struct RenderContext *rctx); /* Sync entire backend buffer to bitmap */

  /* Region-based sync (more efficient than full sync) */
  BOOL (*CopyRegionFromDrawingBoard)(struct RenderContext *rctx,
                                     WORD x, WORD y, UWORD width, UWORD height);

  /* Direct present: render DrawingBoard to window via hardware path,
   * bypassing bitmap sync + BltBitMapRastPort. Returns TRUE if handled,
   * FALSE to fall back to sync + blit. */
  BOOL (*PresentDrawingBoard)(struct RenderContext *rctx,
                              WORD src_x, WORD src_y,
                              WORD dst_x, WORD dst_y,
                              UWORD width, UWORD height);

  /* Text rendering */
  void (*DrawText)(struct RenderContext *rctx, WORD x, WORD y,
                   CONST_STRPTR string, UWORD count,
                   struct InternalColor *fg_color, struct InternalColor *bg_color);

  /* Polygon fill */
  void (*FillPolygon)(struct RenderContext *rctx, struct ZunePoint *points,
                      UWORD count, struct ZuneBrush *brush, BOOL antialias);

  /* Future extension slots */
  APTR reserved[0];

} ZuneBackendOps;

/*****************************************************************************/
/* Backend Registration Structure */
/*****************************************************************************/

typedef struct ZuneBackend {
  struct Node node;            /* For linking in backend list */
  ZuneBackendOps *ops;         /* Backend operations table */
  ZuneBackendContext *context; /* Backend context */
  BOOL available;              /* Backend availability status */
  ULONG priority;              /* Backend selection priority */
} ZuneBackend;

/*****************************************************************************/
/* Backend Management Functions */
/*****************************************************************************/

/* Backend registration and discovery */
BOOL ZuneRegisterBackend(ZuneBackend *backend);
void ZuneUnregisterBackend(ZuneBackend *backend);
ZuneBackend *ZuneFindBestBackend(struct RenderContext *rctx);
ZuneBackend *ZuneFindBackendByType(ZuneBackendType type);

/* Backend lifecycle */
BOOL ZuneInitBackends(void);
void ZuneCleanupBackends(void);

/* Backend information */
ULONG ZuneGetBackendCapabilities(ZuneBackend *backend);
CONST_STRPTR ZuneGetBackendName(ZuneBackend *backend);
BOOL ZuneIsBackendAvailable(ZuneBackendType type);

/* Software fallback operations (used when a backend omits an op) */
void ZuneFallback_DrawPixel(struct RenderContext *rctx, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias);
void ZuneFallback_DrawLine(struct RenderContext *rctx, WORD start_x, WORD start_y,
                           WORD end_x, WORD end_y, UWORD line_width,
                           struct InternalColor *color, BOOL antialias);
void ZuneFallback_DrawRectangle(struct RenderContext *rctx, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius,
                                struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias);
void ZuneFallback_DrawCircle(struct RenderContext *rctx, WORD center_x,
                             WORD center_y, UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias);
void ZuneFallback_ClearRenderContext(struct RenderContext *rctx,
                                  struct InternalColor *color);
void ZuneFallback_BlitRenderContexts(struct RenderContext *source,
                                  struct RenderContext *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height);
void ZuneFallback_BlitToScreen(struct RenderContext *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height);
APTR ZuneFallback_LockPixels(struct DrawingBoard *board, ULONG *pitch_out);
void ZuneFallback_UnlockPixels(struct DrawingBoard *board);
ULONG ZuneFallback_GetPixel(struct DrawingBoard *board, WORD x, WORD y);
void ZuneFallback_SetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color);
BOOL ZuneFallback_CopyFromDrawingBoard(struct RenderContext *rctx);
void ZuneFallback_DrawTexture(struct RenderContext *rctx,
                              struct ZuneTexture *texture, WORD dest_x,
                              WORD dest_y, UWORD dest_width, UWORD dest_height,
                              WORD src_x, WORD src_y, UWORD src_width,
                              UWORD src_height, struct InternalColor *tint);
void ZuneFallback_DrawText(struct RenderContext *rctx, WORD x, WORD y,
                           CONST_STRPTR string, UWORD count,
                           struct InternalColor *fg_color,
                           struct InternalColor *bg_color);
void ZuneFallback_FillPolygon(struct RenderContext *rctx,
                              struct ZunePoint *points, UWORD count,
                              struct ZuneBrush *brush, BOOL antialias);

/* RenderContext-backend binding */
ZuneBackend *ZuneGetRenderContextBackend(struct RenderContext *rctx);
BOOL ZuneBindRenderContextToBackend(struct RenderContext *rctx, ZuneBackend *backend);
void ZuneUnbindRenderContextFromBackend(struct RenderContext *rctx);

/*****************************************************************************/
/* Backend Helper Macros */
/*****************************************************************************/

/* Safe backend operation call without extra args */
#define ZUNE_BACKEND_CALL_NO_ARGS(rctx, op)                                      \
  do {                                                                         \
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);                       \
    if (backend && backend->ops && backend->ops->op) {                         \
      backend->ops->op(rctx);                                                    \
    } else {                                                                   \
      ZuneFallback_##op(rctx);                                                   \
    }                                                                          \
  } while (0)

#define ZUNE_BACKEND_CALL_NO_ARGS_RET(rctx, op, default_ret)                     \
  ({                                                                           \
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);                       \
    (backend && backend->ops && backend->ops->op)                              \
        ? backend->ops->op(rctx)                                                 \
        : ZuneFallback_##op(rctx);                                               \
  })

/* Safe backend operation call */
#define ZUNE_BACKEND_CALL(rctx, op, ...)                                         \
  do {                                                                         \
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);                       \
    if (backend && backend->ops && backend->ops->op) {                         \
      backend->ops->op(rctx, __VA_ARGS__);                                       \
    } else {                                                                   \
      ZuneFallback_##op(rctx, __VA_ARGS__);                                      \
    }                                                                          \
  } while (0)

/* Safe backend operation call with return value */
#define ZUNE_BACKEND_CALL_RET(rctx, op, default_ret, ...)                        \
  ({                                                                           \
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);                       \
    (backend && backend->ops && backend->ops->op)                              \
        ? backend->ops->op(rctx, __VA_ARGS__)                                    \
        : ZuneFallback_##op(rctx, __VA_ARGS__);                                  \
  })

/* Check if backend supports capability */
#define BACKEND_HAS_CAP(backend, cap)                                          \
  ((backend) && (backend)->ops && (backend)->ops->GetCapabilities &&           \
   ((backend)->ops->GetCapabilities() & (cap)) != 0)

/* Check if RenderContext's backend supports capability */
#define RENDERCONTEXT_HAS_CAP(rctx, cap)                                            \
  ({                                                                           \
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);                       \
    BACKEND_HAS_CAP(backend, cap);                                             \
  })

/*****************************************************************************/
/* Global Backend List */
/*****************************************************************************/

extern struct List ZuneBackendList;
extern struct SignalSemaphore ZuneBackendSemaphore;

#endif /* ZUNERENDERER_BACKEND_INTERFACE_H */
