/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Fast Pixel Operations Implementation

    This module provides fast pixel manipulation functions for locked
   DrawingBoards. These operations work directly with pixel buffers to provide
   maximum performance for graphics operations, especially useful for
   CyberGraphics backends.

    Key features:
    - Direct pixel buffer manipulation
    - Support for multiple pixel formats
    - Fast blitting operations
    - Color conversion utilities
    - Surface operations for DrawingBoards
    - Optimized inner loops for performance

    The functions in this module require locked DrawingBoards and provide
    the fastest possible pixel access for graphics operations.
*/

#include "aros/macros.h"
#define DEBUG 0
#include <aros/debug.h>
#include <aros/libcall.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "backends/backend_interface.h"
#include "include/zunegfx.h"
#include "zunegfx_intern.h"

/*****************************************************************************/
/* Color Conversion Caching System */
/*****************************************************************************/

void InitPenCache(struct PenCache *cache, struct ColorMap *cmap) {
  if (!cache)
    return;
  cache->colormap = cmap;
  cache->count = 0;
}

void CleanupPenCache(struct PenCache *cache) {
  if (!cache)
    return;
  /* Release any allocated pens */
  if (cache->colormap) {
    for (int i = 0; i < cache->count; i++) {
      ReleasePen(cache->colormap, cache->pens[i]);
    }
  }
  cache->count = 0;
}

LONG GetCachedPen(struct PenCache *cache, ULONG color) {
  if (!cache || !cache->colormap)
    return -1;

  /* Check if color is already cached */
  for (int i = 0; i < cache->count; i++) {
    if (cache->entries[i] == color) {
      return cache->pens[i];
    }
  }

  /* Try to allocate a new pen */
  LONG pen = ObtainBestPen(cache->colormap, ((color >> 16) & 0xFF) << 24,
                           ((color >> 8) & 0xFF) << 24, (color & 0xFF) << 24,
                           TAG_DONE);

  if (pen != -1 && cache->count < PEN_CACHE_SIZE) {
    cache->entries[cache->count] = color;
    cache->pens[cache->count] = pen;
    cache->count++;
  }

  return pen;
}

/*****************************************************************************/
/* Color cache implementation */
/*****************************************************************************/

void InitColorCache(struct ColorCache *cache) {
  if (!cache)
    return;
  cache->count = 0;
  cache->next_slot = 0;
}

void CleanupColorCache(struct ColorCache *cache) {
  if (!cache)
    return;
  /* No special cleanup needed for color cache entries */
  cache->count = 0;
  cache->next_slot = 0;
}

BOOL GetCachedInternalColor(struct ColorCache *cache, ULONG color, ULONG pixel_format, struct InternalColor *out_color) {
  if (!cache)
    return FALSE;

  /* Check if color and pixel format combination is already cached */
  for (int i = 0; i < cache->count; i++) {
    if (cache->color_keys[i] == color && cache->pixel_formats[i] == pixel_format) {
      *out_color = cache->colors[i];
      D(bug("ColorCache: HIT - color=0x%08X, format=%d, slot=%d\n", color, pixel_format, i));
      return TRUE;
    }
  }

  D(bug("ColorCache: MISS - color=0x%08X, format=%d, entries=%d\n", color, pixel_format, cache->count));
  return FALSE;
}

void CacheInternalColor(struct ColorCache *cache, ULONG color, ULONG pixel_format, const struct InternalColor *internal_color) {
  if (!cache || !internal_color)
    return;

  int slot;

  /* If cache is not full, use next available slot */
  if (cache->count < COLOR_CACHE_SIZE) {
    slot = cache->count;
    cache->count++;
  } else {
    /* Cache is full, use round-robin replacement */
    slot = cache->next_slot;
    cache->next_slot = (cache->next_slot + 1) % COLOR_CACHE_SIZE;
  }

  cache->color_keys[slot] = color;
  cache->pixel_formats[slot] = pixel_format;
  cache->colors[slot] = *internal_color;

  D(bug("ColorCache: STORE - color=0x%08X, format=%d, slot=%d/%d\n",
        color, pixel_format, slot, COLOR_CACHE_SIZE));
}

/*****************************************************************************/
/* Pen color cache implementation */
/*****************************************************************************/

void InitPenColorCache(struct PenColorCache *cache) {
  if (!cache)
    return;
  cache->count = 0;
  cache->next_slot = 0;
}

void CleanupPenColorCache(struct PenColorCache *cache) {
  if (!cache)
    return;
  /* No special cleanup needed for pen color cache entries */
  cache->count = 0;
  cache->next_slot = 0;
}

BOOL GetCachedPenInternalColor(struct PenColorCache *cache, LONG pen, struct ColorMap *cmap, ULONG pixel_format, struct InternalColor *out_color) {
  if (!cache || !out_color || !cmap)
    return FALSE;

  /* Check if pen, colormap, and pixel format combination is already cached */
  for (int i = 0; i < cache->count; i++) {
    if (cache->pen_keys[i] == pen &&
        cache->colormaps[i] == cmap &&
        cache->pixel_formats[i] == pixel_format) {
      *out_color = cache->colors[i];
      D(bug("PenColorCache: HIT - pen=%ld, cmap=%p, format=%d, slot=%d\n", pen, cmap, pixel_format, i));
      return TRUE;
    }
  }

  D(bug("PenColorCache: MISS - pen=%ld, cmap=%p, format=%d, entries=%d\n", pen, cmap, pixel_format, cache->count));
  return FALSE;
}

void CachePenInternalColor(struct PenColorCache *cache, LONG pen, struct ColorMap *cmap, ULONG pixel_format, const struct InternalColor *internal_color) {
  if (!cache || !internal_color || !cmap)
    return;

  int slot;

  /* If cache is not full, use next available slot */
  if (cache->count < PEN_COLOR_CACHE_SIZE) {
    slot = cache->count;
    cache->count++;
  } else {
    /* Cache is full, use round-robin replacement */
    slot = cache->next_slot;
    cache->next_slot = (cache->next_slot + 1) % PEN_COLOR_CACHE_SIZE;
  }

  cache->pen_keys[slot] = pen;
  cache->colormaps[slot] = cmap;
  cache->pixel_formats[slot] = pixel_format;
  cache->colors[slot] = *internal_color;

  D(bug("PenColorCache: STORE - pen=%ld, cmap=%p, format=%d, slot=%d/%d\n",
        pen, cmap, pixel_format, slot, PEN_COLOR_CACHE_SIZE));
}

ULONG BlendColorsInternal(ULONG color1, ULONG color2, ULONG alpha) {
  if (alpha == 0)
    return color1;
  if (alpha == 255)
    return color2;

  UBYTE r1 = ZUNE_GET_RED(color1);
  UBYTE g1 = ZUNE_GET_GREEN(color1);
  UBYTE b1 = ZUNE_GET_BLUE(color1);
  UBYTE a1 = ZUNE_GET_ALPHA(color1);

  UBYTE r2 = ZUNE_GET_RED(color2);
  UBYTE g2 = ZUNE_GET_GREEN(color2);
  UBYTE b2 = ZUNE_GET_BLUE(color2);
  UBYTE a2 = ZUNE_GET_ALPHA(color2);

  UBYTE inv_alpha = 255 - alpha;

  UBYTE r = (r1 * inv_alpha + r2 * alpha) / 255;
  UBYTE g = (g1 * inv_alpha + g2 * alpha) / 255;
  UBYTE b = (b1 * inv_alpha + b2 * alpha) / 255;
  UBYTE a = (a1 * inv_alpha + a2 * alpha) / 255;

  return ZUNE_COLOR_ARGB32(a, r, g, b);
}

static struct InternalColor BuildInternalColor(struct RenderContext *rctx,
                                               ULONG color,
                                               BOOL prepare_backend) {
  struct InternalColor ic;

  ic.a = ZUNE_GET_ALPHA(color);
  ic.r = ZUNE_GET_RED(color);
  ic.g = ZUNE_GET_GREEN(color);
  ic.b = ZUNE_GET_BLUE(color);
  ic.pen = -1;
  ic.pen_allocated = FALSE;
  ic.original_pixel = color;

  if (prepare_backend) {
    ZuneBackend *backend = ZuneGetRenderContextBackend(rctx);
    if (backend && backend->ops && backend->ops->PrepareColor) {
      backend->ops->PrepareColor(rctx, &ic);
    }
  }

  return ic;
}

static BOOL PenIndexToARGB(struct RenderContext *rctx, LONG pen, ULONG *out_color) {
  ULONG rgb[3];

  if (!rctx || !out_color)
    return FALSE;

  struct ColorMap *cm = rctx->colormap;
  if (!cm)
    return FALSE;

  if (pen < 0 || (cm->Count > 0 && pen >= cm->Count))
    return FALSE;

  GetRGB32(cm, pen, 1, rgb);
  *out_color =
      ZUNE_COLOR_ARGB32(0xFF, rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
  return TRUE;
}

BOOL ZuneBrushToInternalColor(struct RenderContext *rctx,
                              const struct ZuneBrush *brush,
                              struct InternalColor *out_color) {
  if (!rctx || !brush || !out_color) {
    D(bug(
        "ZuneBrushToInternalColor: Invalid parameters rctx=%p brush=%p out=%p\n",
        rctx, brush, out_color));
    return FALSE;
  }

  switch (brush->type) {
  case ZUNE_BRUSH_TYPE_SOLID:
    /* Try cache first for solid color */
    if (rctx->color_cache &&
        GetCachedInternalColor(rctx->color_cache, brush->data.solid.color,
                               rctx->pixel_format, out_color)) {
      D(bug("ZuneBrushToInternalColor: Cache hit for solid color=0x%08X\n",
            brush->data.solid.color));
      return TRUE;
    }
    /* Not in cache, compute and cache it */
    *out_color = BuildInternalColor(rctx, brush->data.solid.color, TRUE);
    if (rctx->color_cache) {
      CacheInternalColor(rctx->color_cache, brush->data.solid.color,
                         rctx->pixel_format, out_color);
    }
    return TRUE;
  case ZUNE_BRUSH_TYPE_PEN: {
    LONG pen = brush->data.pen.pen;

    /* Try pen color cache first */
    if (rctx->pen_color_cache && rctx->colormap) {
      if (GetCachedPenInternalColor(rctx->pen_color_cache, pen, rctx->colormap,
                                    rctx->pixel_format, out_color)) {
        D(bug("ZuneBrushToInternalColor: Pen cache hit for pen=%ld\n", pen));
        /* Update pen allocation info from brush */
        out_color->pen = pen;
        out_color->pen_allocated = brush->data.pen.release_pen;
        return TRUE;
      }
    }

    /* Not in cache, convert pen to ARGB first */
    ULONG argb_color;
    if (!PenIndexToARGB(rctx, pen, &argb_color)) {
      D(bug("ZuneBrushToInternalColor: Invalid pen %ld for rctx=%p\n", pen, rctx));
      return FALSE;
    }

    /* Build internal color */
    *out_color = BuildInternalColor(rctx, argb_color, FALSE);
    out_color->pen = pen;
    out_color->pen_allocated = brush->data.pen.release_pen;

    /* Cache the result */
    if (rctx->pen_color_cache && rctx->colormap) {
      CachePenInternalColor(rctx->pen_color_cache, pen, rctx->colormap,
                            rctx->pixel_format, out_color);
    }

    return TRUE;
  }
  default:
    D(bug("ZuneBrushToInternalColor: Unsupported brush type %d\n",
          brush->type));
    return FALSE;
  }
}

/*****************************************************************************/
/* Color Utility Functions */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH3(ULONG, ZuneRGBToColor,

         /*  SYNOPSIS */
         AROS_LHA(UBYTE, r, D0), AROS_LHA(UBYTE, g, D1), AROS_LHA(UBYTE, b, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 32, zunegfx)

/*  FUNCTION
    Converts RGB components to ARGB color value with full alpha.

INPUTS
    r - Red component (0-255)
    g - Green component (0-255)
    b - Blue component (0-255)

RESULT
    ARGB color value with alpha = 255

SEE ALSO
    ZuneARGBToColor(), ZuneBlendColors()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  return ZUNE_COLOR_RGB24(r, g, b);

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(ULONG, ZuneARGBToColor,

         /*  SYNOPSIS */
         AROS_LHA(UBYTE, a, D0), AROS_LHA(UBYTE, r, D1), AROS_LHA(UBYTE, g, D2),
         AROS_LHA(UBYTE, b, D3),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 33, zunegfx)

/*  FUNCTION
    Converts ARGB components to ARGB color value.

INPUTS
    a - Alpha component (0-255)
    r - Red component (0-255)
    g - Green component (0-255)
    b - Blue component (0-255)

RESULT
    ARGB color value

SEE ALSO
    ZuneRGBToColor(), ZuneBlendColors()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  return ZUNE_COLOR_ARGB32(a, r, g, b);

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(ULONG, ZuneBlendColors,

         /*  SYNOPSIS */
         AROS_LHA(ULONG, color1, D0), AROS_LHA(ULONG, color2, D1),
         AROS_LHA(UBYTE, alpha, D2),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 34, zunegfx)

/*  FUNCTION
    Blends two colors using alpha blending.

INPUTS
    color1 - First color (background)
    color2 - Second color (foreground)
    alpha - Blend factor (0-255, 0=color1, 255=color2)

RESULT
    Blended color

NOTES
    This performs standard alpha blending: result = color1 * (1-alpha) + color2
* alpha

SEE ALSO
    ZuneRGBToColor(), ZuneARGBToColor()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  return BlendColorsInternal(color1, color2, alpha);

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Internal Color Conversion */
/*****************************************************************************/

/* Convert ARGB color to internal color structure with caching */
struct InternalColor ZuneColorToInternal(struct RenderContext *rctx, ULONG color,
                                         ULONG pixel_format) {
  struct InternalColor ic;

  /* Try to get from cache first */
  if (rctx && rctx->color_cache) {
    if (GetCachedInternalColor(rctx->color_cache, color, pixel_format, &ic)) {
      D(bug("ZuneColorToInternal: Cache hit for color=0x%08X, pixel_format=%d\n",
            color, pixel_format));
      return ic;
    }
  }

  /* Not in cache, compute it */
  ic = BuildInternalColor(rctx, color, TRUE);

  /* Cache the result */
  if (rctx && rctx->color_cache) {
    CacheInternalColor(rctx->color_cache, color, pixel_format, &ic);
  }

  return ic;
}

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneInitPenCache,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(LONG *, pens, A1),
         AROS_LHA(UWORD, count, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 96, zunegfx)

/*  FUNCTION
    Pre-initializes the pen color cache with a list of pen indices.
    This allows frequently used pens to be cached upfront for better
    performance during rendering.

INPUTS
    rctx    - RenderContext to initialize cache for
    pens  - Array of pen indices to cache
    count - Number of pens in the array

RESULT
    None

SEE ALSO
    CreateRenderContext()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  if (!rctx || !pens || count == 0)
    return;

  if (!rctx->pen_color_cache || !rctx->colormap)
    return;

  for (UWORD i = 0; i < count; i++) {
    LONG pen = pens[i];
    ULONG argb_color;

    /* Convert pen to ARGB */
    if (!PenIndexToARGB(rctx, pen, &argb_color))
      continue;

    /* Build and cache the internal color */
    struct InternalColor ic = BuildInternalColor(rctx, argb_color, FALSE);
    ic.pen = pen;
    ic.pen_allocated = FALSE;

    CachePenInternalColor(rctx->pen_color_cache, pen, rctx->colormap,
                          rctx->pixel_format, &ic);
  }

  AROS_LIBFUNC_EXIT
}
