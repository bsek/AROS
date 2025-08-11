/*
    Copyright (C) 2023, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>

#include "render_hal.h"
#include "muimaster_intern.h"
#include "support.h"

#ifdef __AROS__
#include <aros/cpu.h>
#endif

/* Forward declarations for implementation functions */
static void amiga_fill_rect(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, ULONG pen);
static void amiga_draw_pattern(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *pattern, ULONG fg, ULONG bg);
static void amiga_set_pen(struct RastPort *rp, ULONG pen);
static void amiga_set_ab_pen_drmd(struct RastPort *rp, ULONG apen, ULONG bpen, UBYTE drawmode);
static void amiga_batch_fill_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count);
static void amiga_batch_blend_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *colors, UBYTE *alphas, LONG count);
static void amiga_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color);
static void amiga_pb_blend_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color, UBYTE alpha);
static void amiga_pb_copy_to_rastport(ULONG *pixels, struct RastPort *rp, LONG width, LONG height, struct Rectangle *area);
static ULONG amiga_pen_to_rgba32(ULONG pen, struct MUI_RenderInfo *mri);
static ULONG amiga_rgb_to_rgba32(UBYTE r, UBYTE g, UBYTE b, UBYTE a);

#ifdef __x86_64__
/* SSE2/AVX2 implementations */
static void sse2_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color);
static void avx2_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color);
static void sse2_batch_fill_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count);
#endif

#ifdef __arm__
/* ARM NEON implementations */
static void neon_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color);
static void neon_batch_fill_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count);
#endif

/* CPU capability detection */
static BOOL detect_sse2(void)
{
#ifdef __x86_64__
    /* On x86_64, SSE2 is always available */
    return TRUE;
#elif defined(__i386__)
    /* TODO: Implement CPUID check for i386 */
    return FALSE;
#else
    return FALSE;
#endif
}

static BOOL detect_avx2(void)
{
#ifdef __x86_64__
    /* TODO: Implement proper AVX2 detection via CPUID */
    /* For now, assume it's available on modern x86_64 */
    return TRUE;
#else
    return FALSE;
#endif
}

static BOOL detect_neon(void)
{
#ifdef __arm__
    /* TODO: Implement proper NEON detection */
    /* For now, assume it's available on modern ARM */
    return TRUE;
#else
    return FALSE;
#endif
}

/* Main capability detection function */
struct MUI_RenderHAL *MUI_DetectRenderCapabilities(void)
{
    struct MUI_RenderHAL *hal;

    hal = AllocVec(sizeof(struct MUI_RenderHAL), MEMF_CLEAR);
    if (!hal)
        return NULL;

    /* Initialize with Amiga-compatible defaults */
    hal->capabilities = 0;
    hal->fill_rect = amiga_fill_rect;
    hal->draw_pattern = amiga_draw_pattern;
    hal->set_pen = amiga_set_pen;
    hal->set_ab_pen_drmd = amiga_set_ab_pen_drmd;
    hal->batch_fill_rects = amiga_batch_fill_rects;
    hal->batch_blend_rects = amiga_batch_blend_rects;
    hal->pb_fill_rect = amiga_pb_fill_rect;
    hal->pb_blend_rect = amiga_pb_blend_rect;
    hal->pb_copy_to_rastport = amiga_pb_copy_to_rastport;
    hal->pen_to_rgba32 = amiga_pen_to_rgba32;
    hal->rgb_to_rgba32 = amiga_rgb_to_rgba32;

#ifdef __AROS__
    /* Check for CyberGraphX for hardware acceleration */
    if (CyberGfxBase) {
        hal->capabilities |= RENDER_CAP_BLEND;
        /* TODO: Use CGX functions for better blending */
    }

    /* Always enable pixel buffer support on AROS */
    hal->capabilities |= RENDER_CAP_PIXELBUFFER;

    /* CPU-specific optimizations */
#ifdef __x86_64__
    if (detect_sse2()) {
        hal->capabilities |= RENDER_CAP_SIMD | RENDER_CAP_SSE2;
        hal->pb_fill_rect = sse2_pb_fill_rect;
        hal->batch_fill_rects = sse2_batch_fill_rects;
    }

    if (detect_avx2()) {
        hal->capabilities |= RENDER_CAP_AVX2;
        hal->pb_fill_rect = avx2_pb_fill_rect;
    }
#endif

#ifdef __arm__
    if (detect_neon()) {
        hal->capabilities |= RENDER_CAP_SIMD | RENDER_CAP_NEON;
        hal->pb_fill_rect = neon_pb_fill_rect;
        hal->batch_fill_rects = neon_batch_fill_rects;
    }
#endif

    /* Enable batching if we have SIMD or pixel buffer support */
    if (hal->capabilities & (RENDER_CAP_SIMD | RENDER_CAP_PIXELBUFFER)) {
        hal->capabilities |= RENDER_CAP_BATCH;
    }
#endif

    return hal;
}

void MUI_FreeRenderHAL(struct MUI_RenderHAL *hal)
{
    if (hal) {
        FreeVec(hal);
    }
}

/* Amiga-compatible implementations */
static void amiga_fill_rect(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, ULONG pen)
{
    SetAPen(rp, pen);
    RectFill(rp, x1, y1, x2, y2);
}

static void amiga_draw_pattern(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *pattern, ULONG fg, ULONG bg)
{
    SetAPen(rp, fg);
    SetBPen(rp, bg);
    SetAfPt(rp, pattern, 1);
    RectFill(rp, x1, y1, x2, y2);
    SetAfPt(rp, NULL, 0);
}

static void amiga_set_pen(struct RastPort *rp, ULONG pen)
{
    SetAPen(rp, pen);
}

static void amiga_set_ab_pen_drmd(struct RastPort *rp, ULONG apen, ULONG bpen, UBYTE drawmode)
{
    SetABPenDrMd(rp, apen, bpen, drawmode);
}

static void amiga_batch_fill_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count)
{
    LONG i;
    for (i = 0; i < count; i++) {
        SetAPen(rp, pens[i]);
        RectFill(rp, rects[i].MinX, rects[i].MinY, rects[i].MaxX, rects[i].MaxY);
    }
}

static void amiga_batch_blend_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *colors, UBYTE *alphas, LONG count)
{
    /* Fallback: just draw opaque rectangles */
    LONG i;
    for (i = 0; i < count; i++) {
        /* Extract RGB from RGBA and find closest pen */
        UBYTE r = (colors[i] >> 24) & 0xFF;
        UBYTE g = (colors[i] >> 16) & 0xFF;
        UBYTE b = (colors[i] >> 8) & 0xFF;

        /* TODO: Find closest pen for RGB values */
        SetAPen(rp, 1); /* Temporary fallback */
        RectFill(rp, rects[i].MinX, rects[i].MinY, rects[i].MaxX, rects[i].MaxY);
    }
}

static void amiga_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color)
{
    LONG x, y;

    for (y = y1; y <= y2; y++) {
        ULONG *line = pixels + y * width;
        for (x = x1; x <= x2; x++) {
            line[x] = color;
        }
    }
}

static void amiga_pb_blend_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color, UBYTE alpha)
{
    LONG x, y;
    UBYTE src_r = (color >> 24) & 0xFF;
    UBYTE src_g = (color >> 16) & 0xFF;
    UBYTE src_b = (color >> 8) & 0xFF;
    UBYTE inv_alpha = 255 - alpha;

    for (y = y1; y <= y2; y++) {
        ULONG *line = pixels + y * width;
        for (x = x1; x <= x2; x++) {
            ULONG dst = line[x];
            UBYTE dst_r = (dst >> 24) & 0xFF;
            UBYTE dst_g = (dst >> 16) & 0xFF;
            UBYTE dst_b = (dst >> 8) & 0xFF;

            UBYTE new_r = (src_r * alpha + dst_r * inv_alpha) / 255;
            UBYTE new_g = (src_g * alpha + dst_g * inv_alpha) / 255;
            UBYTE new_b = (src_b * alpha + dst_b * inv_alpha) / 255;

            line[x] = (new_r << 24) | (new_g << 16) | (new_b << 8) | 0xFF;
        }
    }
}

static void amiga_pb_copy_to_rastport(ULONG *pixels, struct RastPort *rp, LONG width, LONG height, struct Rectangle *area)
{
    /* TODO: Implement conversion from RGBA32 to rastport format */
    /* For now, this is a placeholder */
    /* This would need to:
     * 1. Convert RGBA32 to the rastport's native format
     * 2. Handle color mapping for indexed modes
     * 3. Use WritePixelArray or similar for direct modes
     */
}

static ULONG amiga_pen_to_rgba32(ULONG pen, struct MUI_RenderInfo *mri)
{
    /* Convert pen number to RGBA32 color */
    if (mri && mri->mri_Colormap) {
        ULONG rgb[3];
        GetRGB32(mri->mri_Colormap, pen, 1, rgb);

        UBYTE r = (rgb[0] >> 24) & 0xFF;
        UBYTE g = (rgb[1] >> 24) & 0xFF;
        UBYTE b = (rgb[2] >> 24) & 0xFF;

        return (r << 24) | (g << 16) | (b << 8) | 0xFF;
    }

    /* Fallback for common pens */
    switch (pen) {
        case 0: return 0x000000FF; /* Black */
        case 1: return 0xFFFFFFFF; /* White */
        case 2: return 0x666666FF; /* Gray */
        default: return 0x888888FF; /* Default gray */
    }
}

static ULONG amiga_rgb_to_rgba32(UBYTE r, UBYTE g, UBYTE b, UBYTE a)
{
    return (r << 24) | (g << 16) | (b << 8) | a;
}

#ifdef __x86_64__
/* SSE2 optimized implementations would go here */
static void sse2_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color)
{
    /* TODO: Implement SSE2 optimized version */
    /* For now, fall back to generic implementation */
    amiga_pb_fill_rect(pixels, width, x1, y1, x2, y2, color);
}

static void avx2_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color)
{
    /* TODO: Implement AVX2 optimized version */
    /* For now, fall back to SSE2 implementation */
    sse2_pb_fill_rect(pixels, width, x1, y1, x2, y2, color);
}

static void sse2_batch_fill_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count)
{
    /* TODO: Implement SSE2 optimized batch operations */
    /* For now, fall back to generic implementation */
    amiga_batch_fill_rects(rp, rects, pens, count);
}
#endif

#ifdef __arm__
/* ARM NEON optimized implementations would go here */
static void neon_pb_fill_rect(ULONG *pixels, ULONG width, LONG x1, LONG y1, LONG x2, LONG y2, ULONG color)
{
    /* TODO: Implement NEON optimized version */
    /* For now, fall back to generic implementation */
    amiga_pb_fill_rect(pixels, width, x1, y1, x2, y2, color);
}

static void neon_batch_fill_rects(struct RastPort *rp, struct Rectangle *rects, ULONG *pens, LONG count)
{
    /* TODO: Implement NEON optimized batch operations */
    /* For now, fall back to generic implementation */
    amiga_batch_fill_rects(rp, rects, pens, count);
}
#endif
