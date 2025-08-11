/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.
*/

#ifndef _MUIMASTER_RENDER_HAL_H
#define _MUIMASTER_RENDER_HAL_H

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif
#ifndef GRAPHICS_RASTPORT_H
#   include <graphics/rastport.h>
#endif
#ifndef GRAPHICS_GFXMACROS_H
#   include <graphics/gfxmacros.h>
#endif

/* Forward declarations */
struct MUI_RenderInfo;
struct Rectangle;

/* Pixel buffer structure */
typedef struct {
    ULONG *buffer;              /* RGBA32 pixel data */
    ULONG width, height;        /* Buffer dimensions */
    ULONG format;               /* Pixel format (future use) */
    BOOL dirty;                 /* Buffer needs flushing */
    struct Rectangle dirty_rect; /* Dirty area bounds */
    ULONG alloc_size;           /* Allocated buffer size */
} MUI_PixelBuffer;

/* Batch operation structure */
struct MUI_DrawBatch {
    struct Rectangle *rects;    /* Array of rectangles */
    ULONG *colors;             /* Array of RGBA colors */
    UBYTE *operations;         /* Array of operation types */
    UBYTE *alphas;             /* Array of alpha values (optional) */
    LONG count;                /* Number of operations */
    LONG capacity;             /* Allocated capacity */
};

/* Batch operation types */
#define BATCH_OP_FILL    0
#define BATCH_OP_PATTERN 1
#define BATCH_OP_BLEND   2

/* Hardware Abstraction Layer */
struct MUI_RenderHAL {
    /* Capability flags */
    ULONG capabilities;

    /* Traditional rastport operations */
    void (*fill_rect)(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, ULONG pen);
    void (*draw_pattern)(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *pattern, ULONG fg, ULONG bg);
    void (*set_pen)(struct RastPort *rp, ULONG pen);
    void (*set_ab_pen_drmd)(struct RastPort *rp, ULONG apen, ULONG bpen, UBYTE drawmode);

    /* Batch operations */
    void (*batch_fill_rects)(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count);
    void (*batch_blend_rects)(struct RastPort *rp, struct Rectangle *rects, ULONG *colors, UBYTE *alphas, LONG count);

    /* Pixel buffer operations */
    void (*pb_fill_rect)(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color);
    void (*pb_blend_rect)(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color, UBYTE alpha);
    void (*pb_copy_to_rastport)(ULONG *pixels, struct RastPort *rp, LONG width, LONG height, struct Rectangle *area);

    /* Image operations */
    void (*scale_image)(struct RastPort *src, struct RastPort *dst, struct Rectangle *src_rect, struct Rectangle *dst_rect);
    void (*draw_gradient)(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, ULONG start_rgb, ULONG end_rgb, UWORD angle);

    /* Color conversion */
    ULONG (*pen_to_rgba32)(ULONG pen, struct MUI_RenderInfo *mri);
    ULONG (*rgb_to_rgba32)(UBYTE r, UBYTE g, UBYTE b, UBYTE a);
};

/* Capability flags */
#define RENDER_CAP_SIMD         (1<<0)  /* SIMD instructions available */
#define RENDER_CAP_BATCH        (1<<1)  /* Hardware batch operations */
#define RENDER_CAP_BLEND        (1<<2)  /* Hardware alpha blending */
#define RENDER_CAP_SCALE        (1<<3)  /* Hardware scaling */
#define RENDER_CAP_PIXELBUFFER  (1<<4)  /* Pixel buffer support */
#define RENDER_CAP_SSE2         (1<<5)  /* SSE2 instructions */
#define RENDER_CAP_AVX2         (1<<6)  /* AVX2 instructions */
#define RENDER_CAP_NEON         (1<<7)  /* ARM NEON instructions */

/* RastPort wrapper for transparent interception */
struct MUI_RastPortWrapper {
    struct RastPort rp;         /* Must be first for compatibility */
    ULONG magic;                /* Magic number to identify wrapper */
    struct MUI_RenderInfo *mri; /* Back-reference to render info */
    struct MUI_DrawBatch *batch; /* Current batch (if batching) */
    BOOL immediate_mode;        /* TRUE = draw immediately, FALSE = batch */
    BOOL use_pixelbuffer;       /* TRUE = use pixel buffer when beneficial */
    ULONG current_pen;          /* Current foreground pen */
    ULONG current_bpen;         /* Current background pen */
    UBYTE current_drmd;         /* Current draw mode */
};

#define MUI_RASTPORT_MAGIC 0x4D554952  /* "MUIR" */

/* Function prototypes */
struct MUI_RenderHAL *MUI_DetectRenderCapabilities(void);
void MUI_FreeRenderHAL(struct MUI_RenderHAL *hal);

/* Pixel buffer management */
MUI_PixelBuffer *MUI_AcquirePixelBuffer(struct MUI_RenderInfo *mri, ULONG min_width, ULONG min_height);
void MUI_ReleasePixelBuffer(struct MUI_RenderInfo *mri);
void MUI_FlushPixelBuffer(struct MUI_RenderInfo *mri);
BOOL MUI_ShouldUsePixelBuffer(Object *obj, ULONG operation_type, LONG area_size);

/* Batch management */
struct MUI_DrawBatch *MUI_CreateBatch(LONG initial_capacity);
void MUI_AddRectToBatch(struct MUI_DrawBatch *batch, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color, UBYTE operation);
void MUI_FlushBatch(struct MUI_RenderInfo *mri, struct MUI_DrawBatch *batch);
void MUI_FreeBatch(struct MUI_DrawBatch *batch);

/* Wrapped rastport management */
struct MUI_RastPortWrapper *MUI_CreateRastPortWrapper(struct MUI_RenderInfo *mri, struct RastPort *original);
void MUI_FreeRastPortWrapper(struct MUI_RastPortWrapper *wrapper);

/* Transparent drawing functions (intercept graphics.library calls) */
void MUI_RectFill(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2);
void MUI_SetAPen(struct RastPort *rp, ULONG pen);
void MUI_SetBPen(struct RastPort *rp, ULONG pen);
void MUI_SetABPenDrMd(struct RastPort *rp, ULONG apen, ULONG bpen, UBYTE drawmode);
void MUI_SetDrMd(struct RastPort *rp, UBYTE drawmode);

/* Drawing decision logic */
#define OP_FILL     0
#define OP_PATTERN  1
#define OP_BLEND    2
#define OP_GRADIENT 3
#define OP_SCALE    4

/* Helper macros */
#define IS_MUI_RASTPORT(rp) \
    (((struct MUI_RastPortWrapper *)(rp))->magic == MUI_RASTPORT_MAGIC)

#define GET_WRAPPER(rp) \
    ((struct MUI_RastPortWrapper *)(rp))

/* Utility functions */
void MUI_UpdateDirtyRect(struct Rectangle *dirty, LONG x1, LONG y1, LONG x2, LONG y2);
BOOL MUI_RectIntersect(struct Rectangle *a, struct Rectangle *b, struct Rectangle *result);

#endif /* _MUIMASTER_RENDER_HAL_H */
