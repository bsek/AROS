/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.

    Extensions to MUI_RenderInfo structure to support Hardware Abstraction Layer,
    pixel buffers, and batch drawing while maintaining backward compatibility.
*/

#ifndef _MUIMASTER_RENDER_EXTENSIONS_H
#define _MUIMASTER_RENDER_EXTENSIONS_H

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif
#ifndef GRAPHICS_RASTPORT_H
#   include <graphics/rastport.h>
#endif
#ifndef GRAPHICS_REGIONS_H
#   include <graphics/regions.h>
#endif
#ifndef INTUITION_SCREENS_H
#   include <intuition/screens.h>
#endif

#include "render_hal.h"
#include "render_batch.h"

/* Extended MUI_RenderInfo structure */
/* This extends the existing structure without breaking binary compatibility */
struct MUI_RenderInfo {
    /* Original MUI_RenderInfo fields (must remain unchanged for compatibility) */
    struct Screen *mri_Screen;              /* Screen for rendering */
    struct DrawInfo *mri_DrawInfo;          /* Screen DrawInfo */
    struct ColorMap *mri_Colormap;          /* Screen colormap */
    struct RastPort *mri_RastPort;          /* Current rastport */
    struct Rectangle mri_ClipRect;          /* Current clipping rectangle */
    struct Window *mri_Window;              /* Associated window */
    Object *mri_WindowObject;               /* Window object */

    /* Pen management */
    ULONG *mri_Pens;                        /* Pointer to pen array */
    ULONG mri_PensStorage[8];               /* Storage for MUI pens */

    /* Font management */
    struct TextFont *mri_Fonts[16];         /* Font array for different sizes */

    /* Screen properties */
    UWORD mri_ScreenWidth, mri_ScreenHeight;
    ULONG mri_Flags;                        /* Various flags */

    /* Border sizes */
    WORD mri_BorderLeft, mri_BorderRight;
    WORD mri_BorderTop, mri_BorderBottom;

    /* Custom frames */
    APTR mri_FrameImage[16];                /* Custom frame images */

    /* System images for scrollers */
    Object *mri_UpImage, *mri_DownImage;
    Object *mri_LeftImage, *mri_RightImage;
    Object *mri_SizeImage;

    /* Scroller gadgets */
    Object *mri_VertProp, *mri_HorizProp;

    /* Double buffering (original - currently disabled) */
    struct BitMap *mri_BufferBM;            /* Buffer bitmap */
    struct RastPort mri_BufferRP;           /* Buffer rastport */

    /* NEW EXTENSIONS - Added at end for compatibility */

    /* Hardware Abstraction Layer */
    struct MUI_RenderHAL *mri_HAL;          /* HAL for optimized rendering */

    /* Pixel buffer support */
    MUI_PixelBuffer mri_PixelBuffer;        /* Pixel buffer for modern rendering */

    /* Batch rendering */
    struct MUI_DrawBatch *mri_CurrentBatch; /* Current drawing batch */
    BOOL mri_BatchMode;                     /* Are we in batch mode? */

    /* Optimization tracking */
    ULONG mri_OptimizationFlags;            /* Current optimization settings */
    ULONG mri_FrameCounter;                 /* Frame counter for performance tracking */
    ULONG mri_LastFlushTime;                /* Last time buffers were flushed */

    /* Performance statistics */
    struct {
        ULONG total_draws;                  /* Total draw operations */
        ULONG batched_draws;                /* Operations that were batched */
        ULONG pixelbuffer_draws;            /* Operations using pixel buffer */
        ULONG simd_operations;              /* SIMD-accelerated operations */
        ULONG cache_hits;                   /* Resource cache hits */
    } mri_Stats;

    /* Resource caching */
    struct MinList mri_FontCache;           /* Cached fonts */
    struct MinList mri_ImageCache;          /* Cached images */
    struct MinList mri_PatternCache;        /* Cached patterns */

    /* Multi-threading support (future) */
    struct SignalSemaphore mri_RenderSemaphore; /* Rendering synchronization */
    APTR mri_RenderThread;                  /* Background rendering thread */

    /* GPU acceleration hooks (future) */
    APTR mri_GPUContext;                    /* GPU rendering context */
    APTR mri_GPUBuffers;                    /* GPU buffer objects */

    /* Extension version for future compatibility */
    ULONG mri_ExtensionVersion;             /* Version of extensions */
    APTR mri_Reserved[4];                   /* Reserved for future use */
};

/* Optimization flags */
#define MUIOPT_BATCH_RENDERING      (1<<0)  /* Enable batch rendering */
#define MUIOPT_PIXEL_BUFFER         (1<<1)  /* Enable pixel buffer */
#define MUIOPT_SIMD_ACCELERATION    (1<<2)  /* Enable SIMD acceleration */
#define MUIOPT_GPU_ACCELERATION     (1<<3)  /* Enable GPU acceleration (future) */
#define MUIOPT_AGGRESSIVE_CACHE     (1<<4)  /* Aggressive resource caching */
#define MUIOPT_BACKGROUND_RENDER    (1<<5)  /* Background rendering (future) */
#define MUIOPT_DIRTY_TRACKING       (1<<6)  /* Track dirty regions */
#define MUIOPT_VSYNC_RENDERING      (1<<7)  /* VSync-aligned rendering */

/* Extension version */
#define MUI_RENDER_EXTENSION_VERSION    1

/* Convenience macros for accessing extended features */
#define muiRenderHAL(obj) \
    (muiRenderInfo(obj) ? muiRenderInfo(obj)->mri_HAL : NULL)

#define muiPixelBuffer(obj) \
    (muiRenderInfo(obj) ? &muiRenderInfo(obj)->mri_PixelBuffer : NULL)

#define muiCurrentBatch(obj) \
    (muiRenderInfo(obj) ? muiRenderInfo(obj)->mri_CurrentBatch : NULL)

#define muiOptFlags(obj) \
    (muiRenderInfo(obj) ? muiRenderInfo(obj)->mri_OptimizationFlags : 0)

/* Initialization and cleanup functions */
BOOL MUI_InitRenderExtensions(struct MUI_RenderInfo *mri);
void MUI_CleanupRenderExtensions(struct MUI_RenderInfo *mri);

/* Optimization control */
void MUI_SetOptimizationFlags(struct MUI_RenderInfo *mri, ULONG flags);
ULONG MUI_GetOptimizationFlags(struct MUI_RenderInfo *mri);
void MUI_EnableOptimization(struct MUI_RenderInfo *mri, ULONG optimization);
void MUI_DisableOptimization(struct MUI_RenderInfo *mri, ULONG optimization);

/* Performance monitoring */
void MUI_ResetRenderStats(struct MUI_RenderInfo *mri);
void MUI_UpdateRenderStats(struct MUI_RenderInfo *mri, ULONG operation_type, ULONG count);
void MUI_GetRenderStats(struct MUI_RenderInfo *mri, APTR stats_buffer);

/* Resource cache management */
BOOL MUI_InitResourceCaches(struct MUI_RenderInfo *mri);
void MUI_CleanupResourceCaches(struct MUI_RenderInfo *mri);
void MUI_FlushResourceCaches(struct MUI_RenderInfo *mri);

/* Advanced rendering functions */
void MUI_BeginAdvancedRendering(struct MUI_RenderInfo *mri);
void MUI_EndAdvancedRendering(struct MUI_RenderInfo *mri);
void MUI_FlushAllBuffers(struct MUI_RenderInfo *mri);

/* Compatibility checks */
BOOL MUI_HasRenderExtensions(struct MUI_RenderInfo *mri);
ULONG MUI_GetRenderExtensionVersion(struct MUI_RenderInfo *mri);

/* Future GPU acceleration hooks */
typedef void (*MUI_GPUDrawFunc)(APTR gpu_context, APTR draw_data);
typedef BOOL (*MUI_GPUInitFunc)(APTR *gpu_context);
typedef void (*MUI_GPUCleanupFunc)(APTR gpu_context);

struct MUI_GPUHooks {
    MUI_GPUInitFunc init;
    MUI_GPUCleanupFunc cleanup;
    MUI_GPUDrawFunc draw_rect;
    MUI_GPUDrawFunc draw_image;
    MUI_GPUDrawFunc draw_text;
    MUI_GPUDrawFunc draw_gradient;
};

/* Register GPU hooks (future use) */
BOOL MUI_RegisterGPUHooks(struct MUI_RenderInfo *mri, struct MUI_GPUHooks *hooks);

/* Utility macros for backward compatibility */
#ifndef MUI_ENHANCED_RENDERING
/* If enhanced rendering is disabled, these become no-ops */
#define MUI_Enhanced_RectFill(obj, x1, y1, x2, y2, pen) \
    do { SetAPen(_rp(obj), pen); RectFill(_rp(obj), x1, y1, x2, y2); } while(0)

#define MUI_Enhanced_BlendRect(obj, x1, y1, x2, y2, r, g, b, alpha) \
    do { SetAPen(_rp(obj), 1); RectFill(_rp(obj), x1, y1, x2, y2); } while(0)

#define MUI_Enhanced_RectFillPattern(obj, x1, y1, x2, y2, pattern, fg, bg) \
    do { SetABPenDrMd(_rp(obj), fg, bg, JAM2); SetAfPt(_rp(obj), pattern, 1); \
         RectFill(_rp(obj), x1, y1, x2, y2); SetAfPt(_rp(obj), NULL, 0); } while(0)
#endif

#endif /* _MUIMASTER_RENDER_EXTENSIONS_H */
