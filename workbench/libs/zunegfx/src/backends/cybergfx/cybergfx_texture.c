/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Texture Implementation

    This file implements texture operations for the CyberGraphics backend,
    providing hardware-accelerated texture rendering when possible.
*/

#include <cybergraphx/cybergraphics.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <hidd/gfx.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include <proto/oop.h>

#include "../../zunegfx_intern.h"
#include "../backend_interface.h"
#include "cybergfx_backend.h"
#include "cybergfx_pixel_format.h"
#include "cybergfx_simd.h"
#include "libraries/zunegfx.h"
#include <stdbool.h>
#include <string.h>

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************/
/* Constants and Limits */
/*****************************************************************************/

#define CYBERGFX_MAX_TEXTURE_SIZE 4096
#define CYBERGFX_MIN_TEXTURE_SIZE 1

/* Pre-tiled cache dimensions.
 * This matches the legacy BackFillInfo system in datatypescache.c.
 * 256x256 is a good balance between memory usage and tiling efficiency. */
#define TILED_CACHE_MAX_WIDTH 256
#define TILED_CACHE_MAX_HEIGHT 256

/*****************************************************************************/
/* Forward Declarations */
/*****************************************************************************/

static void FreeTiledCache(struct ZuneTexture *texture);

/*****************************************************************************/
/* Texture Helper Functions */
/*****************************************************************************/

static ULONG GetCyberPixelFormat(ULONG zune_format) {
  switch (zune_format) {
  case ZUNE_TEXTURE_FORMAT_ARGB32:
    return PIXFMT_ARGB32;
  case ZUNE_TEXTURE_FORMAT_RGB24:
    return PIXFMT_RGB24;
  case ZUNE_TEXTURE_FORMAT_RGB16:
    return PIXFMT_RGB16;
  case ZUNE_TEXTURE_FORMAT_L8:
  case ZUNE_TEXTURE_FORMAT_A8:
    return PIXFMT_LUT8;
  default:
    return PIXFMT_ARGB32; /* Default fallback */
  }
}

static ULONG GetBytesPerPixel(ULONG format) {
  switch (format) {
  case ZUNE_TEXTURE_FORMAT_ARGB32:
    return 4;
  case ZUNE_TEXTURE_FORMAT_RGB24:
    return 3;
  case ZUNE_TEXTURE_FORMAT_RGB16:
    return 2;
  case ZUNE_TEXTURE_FORMAT_L8:
  case ZUNE_TEXTURE_FORMAT_A8:
    return 1;
  default:
    return 4;
  }
}

static ULONG ConvertPixelWithTint(ULONG pixel, ULONG format,
                                  struct InternalColor *tint) {
  if (!tint)
    return pixel;

  UBYTE r, g, b, a;
  UBYTE tr = tint->r, tg = tint->g, tb = tint->b, ta = tint->a;

  /* Extract components based on format */
  switch (format) {
  case ZUNE_TEXTURE_FORMAT_ARGB32:
    unpack_argb32(pixel, &a, &r, &g, &b);
    break;
  case ZUNE_TEXTURE_FORMAT_RGB24:
    a = 0xFF;
    r = (pixel >> 16) & 0xFF;
    g = (pixel >> 8) & 0xFF;
    b = pixel & 0xFF;
    break;
  case ZUNE_TEXTURE_FORMAT_RGB16:
    a = 0xFF;
    r = ((pixel >> 11) & 0x1F) << 3;
    g = ((pixel >> 5) & 0x3F) << 2;
    b = (pixel & 0x1F) << 3;
    break;
  default:
    return pixel;
  }

  /* Apply tint */
  r = (r * tr) >> 8;
  g = (g * tg) >> 8;
  b = (b * tb) >> 8;
  a = (a * ta) >> 8;

  /* Reconstruct pixel in native format */
  switch (format) {
  case ZUNE_TEXTURE_FORMAT_ARGB32:
    return pack_argb32(a, r, g, b);
  case ZUNE_TEXTURE_FORMAT_RGB24:
    return (r << 16) | (g << 8) | b;
  case ZUNE_TEXTURE_FORMAT_RGB16:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  default:
    return pixel;
  }
}

/*****************************************************************************/
/* Texture Operations Implementation */
/*****************************************************************************/

BOOL CybergfxInitTexture(struct ZuneTexture *texture) {
  ENTER_FUNCTION("CybergfxInitTexture");

  if (!texture) {
    D(bug("CybergfxInitTexture: Invalid texture pointer\n"));
    return FALSE;
  }

  /* Validate texture dimensions */
  if (texture->width == 0 || texture->height == 0 ||
      texture->width > CYBERGFX_MAX_TEXTURE_SIZE ||
      texture->height > CYBERGFX_MAX_TEXTURE_SIZE) {
    D(bug("CybergfxInitTexture: Invalid texture dimensions %dx%d\n",
          texture->width, texture->height));
    return FALSE;
  }

  /* Ensure pixel data is allocated */
  if (!texture->pixel_data) {
    ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
    texture->pitch = texture->width * bytes_per_pixel;
    texture->data_size = texture->pitch * texture->height;

    texture->pixel_data =
        AllocMem(texture->data_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!texture->pixel_data) {
      D(bug("CybergfxInitTexture: Failed to allocate pixel data\n"));
      return FALSE;
    }
  }

  /* Initialize backend-specific data if needed */
  texture->backend_handle = NULL;
  texture->hardware_texture = FALSE;
  texture->backend_type = BACKEND_CYBERGFX;

  D(bug("CybergfxInitTexture: Initialized %dx%d texture (format=%d)\n",
        texture->width, texture->height, texture->format));

  EXIT_FUNCTION("CybergfxInitTexture");
  return TRUE;
}

void CybergfxCleanupTexture(struct ZuneTexture *texture) {
  ENTER_FUNCTION("CybergfxCleanupTexture");

  if (!texture) {
    D(bug("CybergfxCleanupTexture: Invalid texture pointer\n"));
    return;
  }

  /* Free pre-tiled cache if present */
  FreeTiledCache(texture);

  /* Free backend-specific resources */
  if (texture->backend_handle) {
    /* Free any CyberGraphics-specific handles here */
    texture->backend_handle = NULL;
  }

  /* Note: pixel_data cleanup is handled by the core library */

  D(bug("CybergfxCleanupTexture: Cleaned up texture\n"));

  EXIT_FUNCTION("CybergfxCleanupTexture");
}

BOOL CybergfxUpdateTexture(struct ZuneTexture *texture, APTR data, UWORD x,
                           UWORD y, UWORD width, UWORD height) {
  ENTER_FUNCTION("CybergfxUpdateTexture");

  if (!texture || !data || !texture->pixel_data) {
    D(bug("CybergfxUpdateTexture: Invalid parameters\n"));
    return FALSE;
  }

  /* Bounds checking */
  if (x + width > texture->width || y + height > texture->height) {
    D(bug("CybergfxUpdateTexture: Update region out of bounds\n"));
    return FALSE;
  }

  ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
  UBYTE *src = (UBYTE *)data;
  UBYTE *dst = (UBYTE *)texture->pixel_data + (y * texture->pitch) +
               (x * bytes_per_pixel);

  /* Copy data row by row */
  for (UWORD row = 0; row < height; row++) {
    CopyMem(src, dst, width * bytes_per_pixel);
    src += width * bytes_per_pixel;
    dst += texture->pitch;
  }

  D(bug("CybergfxUpdateTexture: Updated region (%d,%d) %dx%d\n", x, y, width,
        height));

  EXIT_FUNCTION("CybergfxUpdateTexture");
  return TRUE;
}

/**
 * CybergfxDrawTextureToRastPortFast
 *
 * Fast path for drawing ARGB32 textures without scaling or tinting.
 * Uses WritePixelArray/WritePixelArrayAlpha for bulk operations.
 *
 * Returns TRUE if fast path was used, FALSE if fallback needed.
 */
static BOOL CybergfxDrawTextureToRastPortFast(struct RenderPort *rp,
                                              struct ZuneTexture *texture,
                                              WORD dest_x, WORD dest_y,
                                              UWORD width, UWORD height,
                                              WORD src_x, WORD src_y,
                                              BOOL needs_alpha_blend) {
  struct RastPort *rastport = rp->target_rp;

  if (!rastport || !rastport->BitMap)
    return FALSE;

  /* Only handle ARGB32 format in fast path */
  if (texture->format != ZUNE_TEXTURE_FORMAT_ARGB32)
    return FALSE;

  /* Calculate source pointer offset */
  UBYTE *src_pixels = (UBYTE *)texture->pixel_data +
                      (src_y * texture->pitch) + (src_x * 4);

  D(bug("CybergfxDrawTextureToRastPortFast: Using bulk %s for %dx%d texture\n",
        needs_alpha_blend ? "WritePixelArrayAlpha" : "WritePixelArray", width, height));

  if (needs_alpha_blend) {
    /* Use WritePixelArrayAlpha for textures that need alpha blending.
     * This is the same highly optimized function used by the legacy code. */
    WritePixelArrayAlpha(src_pixels, 0, 0, texture->pitch,
                         rastport, dest_x, dest_y, width, height,
                         0xffffffff);
  } else {
    /* Use WritePixelArray for fully opaque textures.
     * Even faster than alpha version since no blending required. */
    WritePixelArray(src_pixels, 0, 0, texture->pitch,
                    rastport, dest_x, dest_y, width, height,
                    RECTFMT_ARGB);
  }

  return TRUE;
}

void CybergfxDrawTextureToRastPort(struct RenderPort *rp,
                                   struct ZuneTexture *texture, WORD dest_x,
                                   WORD dest_y, UWORD dest_width,
                                   UWORD dest_height, WORD src_x, WORD src_y,
                                   UWORD src_width, UWORD src_height,
                                   struct InternalColor *tint, ULONG scale_x,
                                   ULONG scale_y) {

  /* Get target rastport and cached HIDD bitmap object */
  struct RastPort *rastport =
      rp->target_board ? rp->target_board->rastport : rp->target_rp;
  OOP_Object *bitmap_obj =
      (rp->target_rp && rp->hidd_bitmap_obj)
          ? (OOP_Object *)rp->hidd_bitmap_obj
          : NULL;

  /* Get window border offset for correct pixel positioning */
  WORD window_offset_x = 0, window_offset_y = 0;
  if (rp->target_rp && rp->target_rp->Layer) {
    window_offset_x = rp->target_rp->Layer->bounds.MinX;
    window_offset_y = rp->target_rp->Layer->bounds.MinY;
  }

  ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
  UBYTE *src_pixels = (UBYTE *)texture->pixel_data;

  D(bug("CybergfxDrawTextureToRastPort: texture->pitch=%d, bytes_per_pixel=%d, "
        "texture dimensions=%dx%d\n",
        texture->pitch, bytes_per_pixel, texture->width, texture->height));
  D(bug("CybergfxDrawTextureToRastPort: scale_x=0x%08X, scale_y=0x%08X, "
        "cached_hidd_obj=%p\n",
        scale_x, scale_y, bitmap_obj));

  /*
   * FAST PATH: Use bulk CyberGraphics operations when possible.
   *
   * Conditions for fast path:
   * 1. No scaling (unity scale: scale_x == 0x10000 && scale_y == 0x10000)
   * 2. No tinting
   * 3. ARGB32 format
   * 4. Rendering directly to RastPort (not DrawingBoard)
   * 5. No clipping needed (or simple rectangular clip)
   *
   * This matches the performance of the legacy dt_put_on_rastport() function.
   */
  BOOL unity_scale = (scale_x == 0x10000 && scale_y == 0x10000 &&
                      dest_width == src_width && dest_height == src_height);

  /* Determine if we need alpha blending:
   * - If ZUNE_TEXTURE_OPAQUE is set, texture is fully opaque - no blending needed
   * - If ZUNE_TEXTURE_ALPHA is set but not OPAQUE, need alpha blending
   * - If neither ALPHA flag is set, texture is opaque - no blending needed */
  BOOL needs_alpha_blend = (texture->flags & ZUNE_TEXTURE_ALPHA) != 0 &&
                           (texture->flags & ZUNE_TEXTURE_OPAQUE) == 0;

  if (unity_scale && !tint && rp->target_rp && !rp->target_board) {
    /* Check if we can use the fast path */
    if (CybergfxDrawTextureToRastPortFast(rp, texture, dest_x, dest_y,
                                          dest_width, dest_height,
                                          src_x, src_y, needs_alpha_blend)) {
      /* Fast path succeeded, we're done */
      return;
    }
    /* Fast path failed, fall through to slow path */
    D(bug("CybergfxDrawTextureToRastPort: Fast path unavailable, using slow path\n"));
  }

  /*
   * SLOW PATH: Pixel-by-pixel rendering.
   * Used when scaling, tinting, or non-ARGB32 format is needed.
   */
  for (UWORD dy = 0; dy < dest_height; dy++) {
    UWORD sy = src_y + ((dy * scale_y) >> 16);
    if (sy >= texture->height)
      sy = texture->height - 1;

    for (UWORD dx = 0; dx < dest_width; dx++) {
      UWORD sx = src_x + ((dx * scale_x) >> 16);
      if (sx >= texture->width)
        sx = texture->width - 1;

      /* Get source pixel */
      UBYTE *pixel_ptr =
          src_pixels + (sy * texture->pitch) + (sx * bytes_per_pixel);
      ULONG pixel = 0;

      switch (bytes_per_pixel) {
      case 4:
        pixel = *(ULONG *)pixel_ptr;
        break;
      case 3:
        pixel = (pixel_ptr[0] << 16) | (pixel_ptr[1] << 8) | pixel_ptr[2];
        break;
      case 2:
        pixel = *(UWORD *)pixel_ptr;
        break;
      case 1:
        pixel = *pixel_ptr;
        break;
      }

      /* Apply tinting if specified */
      if (tint) {
        pixel = ConvertPixelWithTint(pixel, texture->format, tint);
      }

      /* Use direct HIDD operations to bypass WritePixelArray format conversion
       */
      HIDDT_Color col;
      UBYTE a, r, g, b;

      switch (texture->format) {
      case ZUNE_TEXTURE_FORMAT_ARGB32:
        /* Texture data is in native ARGB32 format - use unpack macro */
        unpack_argb32(pixel, &a, &r, &g, &b);
        break;
      case ZUNE_TEXTURE_FORMAT_RGB24:
        a = 0xFF;
        r = (pixel >> 16) & 0xFF;
        g = (pixel >> 8) & 0xFF;
        b = pixel & 0xFF;
        break;
      case ZUNE_TEXTURE_FORMAT_RGB16:
        a = 0xFF;
        r = ((pixel >> 11) & 0x1F) << 3;
        g = ((pixel >> 5) & 0x3F) << 2;
        b = (pixel & 0x1F) << 3;
        break;
      default:
        /* Default assumes ARGB32 format */
        unpack_argb32(pixel, &a, &r, &g, &b);
        break;
      }

      /* Skip fully transparent pixels */
      if (a == 0)
        continue;

      /* Apply clipping before drawing pixel */
      WORD pixel_x = dest_x + dx;
      WORD pixel_y = dest_y + dy;

      if (!CybergfxClipPixel(rp, pixel_x, pixel_y))
        continue;

      /* For partially transparent pixels (0 < a < 255), we need alpha blending.
       * For fully opaque pixels (a == 255), we can just write directly.
       */
      if (a < 255 && bitmap_obj) {
        /* Read destination pixel for alpha blending */
        HIDDT_Pixel dest_native = HIDD_BM_GetPixel(bitmap_obj,
                                                    pixel_x + window_offset_x,
                                                    pixel_y + window_offset_y);
        HIDDT_Color dest_col;
        HIDD_BM_UnmapPixel(bitmap_obj, dest_native, &dest_col);

        /* Use blend function with unpacked components */
        ULONG dest_pixel = pack_argb32(dest_col.alpha >> 8, dest_col.red >> 8,
                                       dest_col.green >> 8, dest_col.blue >> 8);
        ULONG blended = blend_argb32_alpha(dest_pixel, a, r, g, b);
        unpack_argb32(blended, &a, &r, &g, &b);
      }

      /* Convert to HIDD color format (16-bit components) */
      col.alpha = a << 8;
      col.red = r << 8;
      col.green = g << 8;
      col.blue = b << 8;

      if (bitmap_obj) {
        /* Use cached HIDD bitmap object for efficient operations */
        HIDDT_Pixel native_pixel = HIDD_BM_MapColor(bitmap_obj, &col);
        HIDD_BM_PutPixel(bitmap_obj, pixel_x + window_offset_x,
                         pixel_y + window_offset_y, native_pixel);
      } else {
        /* Fallback to standard RastPort write if no HIDD object is available */
        ULONG fallback_pixel = pack_argb32_logical(a, r, g, b);
        WriteRGBPixel(rastport, pixel_x, pixel_y, fallback_pixel);
      }
    }
  }

  /* Update the display region to make the changes visible only when we're
   * drawing directly to a screen RastPort. Off-screen DrawingBoards will be
   * blitted later, so avoid poking the windowing system to prevent crashes. */
  if (rp->target_rp && bitmap_obj) {
    HIDD_BM_UpdateRect(bitmap_obj, dest_x + window_offset_x,
                       dest_y + window_offset_y, dest_width + 1,
                       dest_height + 1);
  }
}

void CybergfxDrawTextureToDrawingBoard(
    struct RenderPort *rp, struct ZuneTexture *texture, WORD dest_x,
    WORD dest_y, UWORD dest_width, UWORD dest_height, WORD src_x, WORD src_y,
    UWORD src_width, UWORD src_height, struct InternalColor *tint,
    ULONG scale_x, ULONG scale_y) {
  ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
  UBYTE *src_pixels = (UBYTE *)texture->pixel_data;
  /* Rendering to DrawingBoard */
  struct DrawingBoard *board = rp->target_board;
  D(bug("CybergfxDrawTexture: Using DrawingBoard, pixels_locked = %s\n",
        board->pixels_locked ? "TRUE" : "FALSE"));

  if (board->pixels_locked) {
    /* Use direct pixel manipulation for locked DrawingBoard */
    D(bug("CybergfxDrawTexture: Locked board detected, pixel_format = %u\n",
          board->pixel_format));

    if (board->pixel_format == PIXFMT_ARGB32 ||
        board->pixel_format == PIXFMT_RGBA32) {
      ULONG *dest_pixels = (ULONG *)board->pixels;
      ULONG pitch_pixels = board->pitch / 4;

      bool unity_scale = (scale_x == 0x10000 && scale_y == 0x10000 &&
                          dest_width == src_width && dest_height == src_height);

      /* Fast path: ARGB32 copy/tint with unity scale */
      if (unity_scale && bytes_per_pixel == 4) {
        for (UWORD dy = 0; dy < dest_height; dy++) {
          const UBYTE *src_row =
              src_pixels + ((src_y + dy) * texture->pitch) + src_x * 4;
          ULONG *dst_row =
              dest_pixels + ((dest_y + dy) * pitch_pixels) + dest_x;

          cybergfx_blit_argb32_unity(src_row, dst_row, dest_width, tint);
        }
        return;
      }

      /* Render pixel by pixel using direct memory access */
      for (UWORD dy = 0; dy < dest_height; dy++) {
        UWORD sy = src_y + ((dy * scale_y) >> 16);
        if (sy >= texture->height)
          sy = texture->height - 1;

        for (UWORD dx = 0; dx < dest_width; dx++) {
          UWORD sx = src_x + ((dx * scale_x) >> 16);
          if (sx >= texture->width)
            sx = texture->width - 1;

          WORD px = dest_x + dx;
          WORD py = dest_y + dy;

          /* Bounds check for destination */
          if (px >= 0 && py >= 0 && px < board->width && py < board->height) {
            /* Get source pixel */
            UBYTE *pixel_ptr =
                src_pixels + (sy * texture->pitch) + (sx * bytes_per_pixel);
            ULONG pixel = 0;

            switch (bytes_per_pixel) {
            case 4:
              pixel = *(ULONG *)pixel_ptr;
              break;
            case 3:
              pixel = 0xFF000000 | (pixel_ptr[0] << 16) | (pixel_ptr[1] << 8) |
                      pixel_ptr[2];
              break;
            case 2: {
              UWORD rgb16 = *(UWORD *)pixel_ptr;
              UBYTE r = ((rgb16 >> 11) & 0x1F) << 3;
              UBYTE g = ((rgb16 >> 5) & 0x3F) << 2;
              UBYTE b = (rgb16 & 0x1F) << 3;
              pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
            } break;
            case 1:
              pixel = 0xFF000000 | (*pixel_ptr << 16) | (*pixel_ptr << 8) |
                      *pixel_ptr;
              break;
            }

            /* Apply tinting if specified */
            if (tint) {
              pixel = ConvertPixelWithTint(pixel, texture->format, tint);
            }

            /* Alpha blend source pixel over destination */
            ULONG dest_pixel = dest_pixels[py * pitch_pixels + px];
            ULONG blended = blend_argb32(dest_pixel, pixel);

            /* Skip if pixel unchanged (fully transparent source) */
            if (blended == dest_pixel)
              continue;

            CybergfxWritePixelClamped(dest_pixels, pitch_pixels, board->width,
                                      board->height, px, py, blended);
          }
        }
      }
    } else {
      D(bug("CybergfxDrawTexture: Unsupported pixel format for locked "
            "DrawingBoard: %u\n",
            board->pixel_format));
    }
  } else {
    /* Unlocked pixels, draw to rastport */
    D(bug("CybergfxDrawTexture: Using unlocked DrawingBoard path\n"));
    struct RastPort *rastport = board->rastport;
    if (!rastport) {
      D(bug("CybergfxDrawTexture: No rastport in DrawingBoard\n"));
      return;
    }

    CybergfxDrawTextureToRastPort(rp, texture, dest_x, dest_y, dest_width,
                                  dest_height, src_x, src_y, src_width,
                                  src_height, tint, scale_x, scale_y);
  }
}

void CybergfxDrawTexture(struct RenderPort *rp, struct ZuneTexture *texture,
                         WORD dest_x, WORD dest_y, UWORD dest_width,
                         UWORD dest_height, WORD src_x, WORD src_y,
                         UWORD src_width, UWORD src_height,
                         struct InternalColor *tint) {
  ENTER_FUNCTION("CybergfxDrawTexture");

  if (!rp || !texture || !texture->pixel_data) {
    D(bug("CybergfxDrawTexture: Invalid parameters\n"));
    return;
  }

  /* Bounds checking */
  if (src_x + src_width > texture->width ||
      src_y + src_height > texture->height) {
    D(bug("CybergfxDrawTexture: Source region out of bounds\n"));
    return;
  }

  if (dest_width == 0 || dest_height == 0 || src_width == 0 ||
      src_height == 0) {
    D(bug("CybergfxDrawTexture: Invalid dimensions\n"));
    return;
  }

  /* Simple nearest-neighbor scaling with optional tinting */

  /* Calculate scaling factors (fixed-point 16.16) */
  ULONG scale_x = (src_width << 16) / dest_width;
  ULONG scale_y = (src_height << 16) / dest_height;

  D(bug("CybergfxDrawTexture: src=(%d,%d %dx%d) dest=(%d,%d %dx%d) "
        "scale=(0x%08X,0x%08X)\n",
        src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width,
        dest_height, scale_x, scale_y));

  if (rp->target_board) {
    CybergfxDrawTextureToDrawingBoard(rp, texture, dest_x, dest_y, dest_width,
                                      dest_height, src_x, src_y, src_width,
                                      src_height, tint, scale_x, scale_y);
  } else if (rp->target_rp) {
    /* Rendering directly to RastPort */
    D(bug("CybergfxDrawTexture: Using direct RastPort path\n"));
    struct RastPort *rastport = rp->target_rp;
    CybergfxDrawTextureToRastPort(rp, texture, dest_x, dest_y, dest_width,
                                  dest_height, src_x, src_y, src_width,
                                  src_height, tint, scale_x, scale_y);
  } else {
    D(bug("CybergfxDrawTexture: No valid target found!\n"));
    return;
  }

  D(bug("CybergfxDrawTexture: Drew texture region (%d,%d %dx%d) to (%d,%d "
        "%dx%d)\n",
        src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width,
        dest_height));

  EXIT_FUNCTION("CybergfxDrawTexture");
}

APTR CybergfxLockTexturePixels(struct ZuneTexture *texture, ULONG *pitch) {
  ENTER_FUNCTION("CybergfxLockTexturePixels");

  if (!texture || !texture->pixel_data) {
    D(bug("CybergfxLockTexturePixels: Invalid texture\n"));
    return NULL;
  }

  if (texture->pixels_locked) {
    D(bug("CybergfxLockTexturePixels: Texture already locked\n"));
    return NULL;
  }

  texture->pixels_locked = TRUE;
  if (pitch) {
    *pitch = texture->pitch;
  }

  D(bug("CybergfxLockTexturePixels: Locked texture pixels\n"));

  EXIT_FUNCTION("CybergfxLockTexturePixels");
  return texture->pixel_data;
}

void CybergfxUnlockTexturePixels(struct ZuneTexture *texture) {
  ENTER_FUNCTION("CybergfxUnlockTexturePixels");

  if (!texture) {
    D(bug("CybergfxUnlockTexturePixels: Invalid texture\n"));
    return;
  }

  if (!texture->pixels_locked) {
    D(bug("CybergfxUnlockTexturePixels: Texture not locked\n"));
    return;
  }

  texture->pixels_locked = FALSE;

  D(bug("CybergfxUnlockTexturePixels: Unlocked texture pixels\n"));

  EXIT_FUNCTION("CybergfxUnlockTexturePixels");
}

ULONG CybergfxGetTexturePixel(struct ZuneTexture *texture, WORD x, WORD y) {
  ENTER_FUNCTION("CybergfxGetTexturePixel");

  if (!texture || !texture->pixel_data) {
    D(bug("CybergfxGetTexturePixel: Invalid texture\n"));
    return 0;
  }

  if (x < 0 || y < 0 || x >= texture->width || y >= texture->height) {
    D(bug("CybergfxGetTexturePixel: Coordinates out of bounds\n"));
    return 0;
  }

  ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
  UBYTE *pixel_ptr = (UBYTE *)texture->pixel_data + (y * texture->pitch) +
                     (x * bytes_per_pixel);
  ULONG pixel = 0;

  switch (bytes_per_pixel) {
  case 4:
    pixel = *(ULONG *)pixel_ptr;
    break;
  case 3:
    pixel = (pixel_ptr[0] << 16) | (pixel_ptr[1] << 8) | pixel_ptr[2];
    break;
  case 2:
    pixel = *(UWORD *)pixel_ptr;
    break;
  case 1:
    pixel = *pixel_ptr;
    break;
  }

  D(bug("CybergfxGetTexturePixel: Got pixel 0x%08x at (%d,%d)\n", pixel, x, y));

  EXIT_FUNCTION("CybergfxGetTexturePixel");
  return pixel;
}

void CybergfxSetTexturePixel(struct ZuneTexture *texture, WORD x, WORD y,
                             struct InternalColor *color) {
  ENTER_FUNCTION("CybergfxSetTexturePixel");

  if (!texture || !texture->pixel_data || !color) {
    D(bug("CybergfxSetTexturePixel: Invalid parameters\n"));
    return;
  }

  if (x < 0 || y < 0 || x >= texture->width || y >= texture->height) {
    D(bug("CybergfxSetTexturePixel: Coordinates out of bounds\n"));
    return;
  }

  ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
  UBYTE *pixel_ptr = (UBYTE *)texture->pixel_data + (y * texture->pitch) +
                     (x * bytes_per_pixel);
  ULONG pixel;

  /* Convert color to texture format */
  switch (texture->format) {
  case ZUNE_TEXTURE_FORMAT_ARGB32:
    pixel = (color->a << 24) | (color->r << 16) | (color->g << 8) | color->b;
    *(ULONG *)pixel_ptr = pixel;
    break;
  case ZUNE_TEXTURE_FORMAT_RGB24:
    pixel_ptr[0] = color->r;
    pixel_ptr[1] = color->g;
    pixel_ptr[2] = color->b;
    break;
  case ZUNE_TEXTURE_FORMAT_RGB16:
    pixel = ((color->r >> 3) << 11) | ((color->g >> 2) << 5) | (color->b >> 3);
    *(UWORD *)pixel_ptr = pixel;
    break;
  case ZUNE_TEXTURE_FORMAT_L8:
  case ZUNE_TEXTURE_FORMAT_A8:
    /* For palette mode, use the red component as index */
    *pixel_ptr = color->r;
    break;
  default:
    D(bug("CybergfxSetTexturePixel: Unsupported texture format\n"));
    break;
  }

  D(bug("CybergfxSetTexturePixel: Set pixel at (%d,%d) to ARGB(%d,%d,%d,%d)\n",
        x, y, color->a, color->r, color->g, color->b));

  EXIT_FUNCTION("CybergfxSetTexturePixel");
}

ULONG CybergfxGetMaxTextureSize(void) {
  ENTER_FUNCTION("CybergfxGetMaxTextureSize");
  EXIT_FUNCTION("CybergfxGetMaxTextureSize");
  return CYBERGFX_MAX_TEXTURE_SIZE;
}

BOOL CybergfxSupportsTextureFormat(ULONG format) {
  ENTER_FUNCTION("CybergfxSupportsTextureFormat");

  BOOL supported = FALSE;

  switch (format) {
  case ZUNE_TEXTURE_FORMAT_ARGB32:
  case ZUNE_TEXTURE_FORMAT_RGB24:
  case ZUNE_TEXTURE_FORMAT_RGB16:
  case ZUNE_TEXTURE_FORMAT_L8:
  case ZUNE_TEXTURE_FORMAT_A8:
    supported = TRUE;
    break;
  default:
    supported = FALSE;
    break;
  }

  D(bug("CybergfxSupportsTextureFormat: Format %d is %ssupported\n", format,
        supported ? "" : "not "));

  EXIT_FUNCTION("CybergfxSupportsTextureFormat");
  return supported;
}

/*****************************************************************************/
/* Pre-tiled Cache Management */
/*****************************************************************************/

/**
 * CalculateTiledCacheSize
 *
 * Calculates the optimal pre-tiled cache dimensions for a given texture.
 * The cache size is the smallest multiple of texture dimensions that is
 * >= the maximum cache size (256x256), ensuring we can tile efficiently.
 */
static void CalculateTiledCacheSize(UWORD tex_width, UWORD tex_height,
                                    UWORD *cache_width, UWORD *cache_height) {
  /* For textures larger than cache max, just use the texture size */
  if (tex_width >= TILED_CACHE_MAX_WIDTH) {
    *cache_width = tex_width;
  } else {
    /* Round up to nearest multiple of texture width that's >= max */
    *cache_width = TILED_CACHE_MAX_WIDTH - (TILED_CACHE_MAX_WIDTH % tex_width);
    if (*cache_width < TILED_CACHE_MAX_WIDTH)
      *cache_width += tex_width;
  }

  if (tex_height >= TILED_CACHE_MAX_HEIGHT) {
    *cache_height = tex_height;
  } else {
    /* Round up to nearest multiple of texture height that's >= max */
    *cache_height = TILED_CACHE_MAX_HEIGHT - (TILED_CACHE_MAX_HEIGHT % tex_height);
    if (*cache_height < TILED_CACHE_MAX_HEIGHT)
      *cache_height += tex_height;
  }
}

/**
 * CreateTiledCache
 *
 * Creates a pre-tiled cache for a texture using the exponential doubling
 * algorithm. This is the same approach used by the legacy CopyTiledBitMap()
 * function in datatypescache.c.
 *
 * We create both:
 * 1. A native BitMap for hardware-accelerated BltBitMap operations
 * 2. An ARGB32 pixel buffer as fallback for DrawingBoard rendering
 *
 * The BitMap path is significantly faster as it uses hardware blitting.
 */
static BOOL CreateTiledCache(struct ZuneTexture *texture, struct RastPort *friend_rp) {
  UWORD tex_width = texture->width;
  UWORD tex_height = texture->height;
  UWORD cache_width, cache_height;
  ULONG *cache_pixels;
  ULONG *src_pixels;
  ULONG src_pitch_pixels;
  ULONG cache_pitch_pixels;
  struct BitMap *cache_bitmap = NULL;

  /* Don't create cache if texture format is not ARGB32 */
  if (texture->format != ZUNE_TEXTURE_FORMAT_ARGB32)
    return FALSE;

  /* Don't create cache if texture is already large enough */
  if (tex_width >= TILED_CACHE_MAX_WIDTH && tex_height >= TILED_CACHE_MAX_HEIGHT)
    return FALSE;

  /* Calculate optimal cache size */
  CalculateTiledCacheSize(tex_width, tex_height, &cache_width, &cache_height);

  /* Allocate ARGB32 pixel buffer for software path and initial tiling */
  cache_pixels = AllocVec(cache_width * cache_height * sizeof(ULONG),
                          MEMF_PUBLIC | MEMF_CLEAR);
  if (!cache_pixels) {
    D(bug("CreateTiledCache: Failed to allocate %dx%d pixel cache\n",
          cache_width, cache_height));
    return FALSE;
  }

  src_pixels = (ULONG *)texture->pixel_data;
  src_pitch_pixels = texture->pitch / 4;
  cache_pitch_pixels = cache_width;

  D(bug("CreateTiledCache: Creating %dx%d cache from %dx%d texture\n",
        cache_width, cache_height, tex_width, tex_height));

  /*
   * Step 1: Copy the first tile (texture) into the cache at position (0,0)
   */
  for (UWORD y = 0; y < tex_height; y++) {
    CopyMem(src_pixels + (y * src_pitch_pixels),
            cache_pixels + (y * cache_pitch_pixels),
            tex_width * sizeof(ULONG));
  }

  /*
   * Step 2: Exponential doubling horizontally
   * Start with one tile width, then double until we fill the cache width.
   * This is much faster than copying tile-by-tile.
   */
  UWORD filled_width = tex_width;
  while (filled_width < cache_width) {
    UWORD copy_width = filled_width;
    if (filled_width + copy_width > cache_width)
      copy_width = cache_width - filled_width;

    /* Copy the filled portion to double the width */
    for (UWORD y = 0; y < tex_height; y++) {
      CopyMem(cache_pixels + (y * cache_pitch_pixels),
              cache_pixels + (y * cache_pitch_pixels) + filled_width,
              copy_width * sizeof(ULONG));
    }
    filled_width += copy_width;
  }

  /*
   * Step 3: Exponential doubling vertically
   * Now we have one full row of tiles. Double the rows until we fill
   * the cache height.
   */
  UWORD filled_height = tex_height;
  while (filled_height < cache_height) {
    UWORD copy_height = filled_height;
    if (filled_height + copy_height > cache_height)
      copy_height = cache_height - filled_height;

    /* Copy filled rows to double the height */
    CopyMem(cache_pixels,
            cache_pixels + (filled_height * cache_pitch_pixels),
            cache_width * copy_height * sizeof(ULONG));
    filled_height += copy_height;
  }

  /*
   * Step 4: Create native BitMap and copy pre-tiled data to it.
   * This enables hardware-accelerated BltBitMap operations.
   * We use a friend bitmap from the target RastPort for optimal format.
   */
  if (friend_rp && friend_rp->BitMap) {
    ULONG depth = GetBitMapAttr(friend_rp->BitMap, BMA_DEPTH);

    /* Allocate bitmap compatible with screen for hardware blitting */
    cache_bitmap = AllocBitMap(cache_width, cache_height, depth,
                               BMF_MINPLANES, friend_rp->BitMap);

    if (cache_bitmap) {
      /* Write pre-tiled ARGB32 pixels to the native bitmap */
      WritePixelArray(cache_pixels, 0, 0, cache_width * sizeof(ULONG),
                      friend_rp, 0, 0, 0, 0, RECTFMT_ARGB);

      /* Actually write to the cache bitmap via a temporary RastPort */
      struct RastPort temp_rp;
      InitRastPort(&temp_rp);
      temp_rp.BitMap = cache_bitmap;

      WritePixelArray(cache_pixels, 0, 0, cache_width * sizeof(ULONG),
                      &temp_rp, 0, 0, cache_width, cache_height, RECTFMT_ARGB);

      D(bug("CreateTiledCache: Created %dx%d native BitMap cache (depth=%ld)\n",
            cache_width, cache_height, depth));
    } else {
      D(bug("CreateTiledCache: Failed to allocate native BitMap, using software path\n"));
    }
  }

  /* Store cache in texture */
  texture->tiled_cache_bitmap = cache_bitmap;
  texture->tiled_cache_pixels = cache_pixels;
  texture->tiled_cache_width = cache_width;
  texture->tiled_cache_height = cache_height;
  texture->tiled_cache_pitch = cache_width * sizeof(ULONG);

  D(bug("CreateTiledCache: Successfully created %dx%d pre-tiled cache (bitmap=%p, pixels=%p)\n",
        cache_width, cache_height, cache_bitmap, cache_pixels));

  return TRUE;
}

/**
 * FreeTiledCache
 *
 * Frees the pre-tiled cache associated with a texture.
 */
static void FreeTiledCache(struct ZuneTexture *texture) {
  if (!texture)
    return;

  if (texture->tiled_cache_bitmap) {
    FreeBitMap(texture->tiled_cache_bitmap);
    texture->tiled_cache_bitmap = NULL;
  }

  if (texture->tiled_cache_pixels) {
    FreeVec(texture->tiled_cache_pixels);
    texture->tiled_cache_pixels = NULL;
  }

  texture->tiled_cache_width = 0;
  texture->tiled_cache_height = 0;
  texture->tiled_cache_pitch = 0;
}

/**
 * EnsureTiledCache
 *
 * Ensures that the texture has a valid pre-tiled cache.
 * Creates the cache if it doesn't exist.
 *
 * @param texture The texture to ensure cache for
 * @param friend_rp RastPort to use as friend for BitMap allocation (can be NULL)
 *
 * Returns TRUE if cache is available, FALSE otherwise.
 */
static BOOL EnsureTiledCache(struct ZuneTexture *texture, struct RastPort *friend_rp) {
  /* Already have a cache? */
  if (texture->tiled_cache_pixels || texture->tiled_cache_bitmap)
    return TRUE;

  /* Try to create one */
  return CreateTiledCache(texture, friend_rp);
}

/*****************************************************************************/
/* Tiled Rendering with Pre-tiled Cache */
/*****************************************************************************/

/**
 * RenderTiledCacheToRastPort
 *
 * Renders using the pre-tiled cache. Uses hardware BltBitMap when available,
 * falls back to WritePixelArray otherwise.
 *
 * When using BltBitMap, we use the exponential doubling algorithm from
 * the legacy CopyTiledBitMap() for maximum efficiency:
 * 1. Blit the first cache-sized chunk
 * 2. Exponentially double horizontally
 * 3. Exponentially double vertically
 */
static BOOL RenderTiledCacheToRastPort(struct RenderPort *rp,
                                       struct ZuneTexture *texture,
                                       WORD dest_x, WORD dest_y,
                                       UWORD dest_width, UWORD dest_height) {
  struct RastPort *rastport = rp->target_rp;
  struct BitMap *cache_bitmap = texture->tiled_cache_bitmap;
  ULONG *cache_pixels = (ULONG *)texture->tiled_cache_pixels;
  UWORD cache_width = texture->tiled_cache_width;
  UWORD cache_height = texture->tiled_cache_height;
  ULONG cache_pitch = texture->tiled_cache_pitch;

  /* Determine if we need alpha blending */
  BOOL needs_alpha_blend = (texture->flags & ZUNE_TEXTURE_ALPHA) != 0 &&
                           (texture->flags & ZUNE_TEXTURE_OPAQUE) == 0;

  D(bug("RenderTiledCacheToRastPort: dest=(%d,%d) %dx%d, cache=%dx%d, bitmap=%p, alpha=%d\n",
        dest_x, dest_y, dest_width, dest_height, cache_width, cache_height,
        cache_bitmap, needs_alpha_blend));

  /*
   * FAST PATH: Use hardware BltBitMap with exponential doubling.
   * This is only possible when:
   * 1. We have a native BitMap cache
   * 2. The texture is fully opaque (no alpha blending needed)
   *
   * This matches the legacy CopyTiledBitMap() performance.
   */
  if (cache_bitmap && !needs_alpha_blend && rastport->BitMap) {
    struct BitMap *dest_bitmap = rastport->BitMap;
    UWORD first_width, first_height;
    WORD pos, size;

    /* Calculate first tile dimensions (may be clipped) */
    first_width = (cache_width > dest_width) ? dest_width : cache_width;
    first_height = (cache_height > dest_height) ? dest_height : cache_height;

    /* Step 1: Blit the first cache chunk to destination */
    BltBitMap(cache_bitmap, 0, 0, dest_bitmap, dest_x, dest_y,
              first_width, first_height, 0xC0, -1, NULL);

    /* Step 2: Exponential doubling horizontally */
    for (pos = dest_x + cache_width, size = MIN(cache_width, dest_x + dest_width - pos);
         pos < dest_x + dest_width;
         ) {
      BltBitMap(dest_bitmap, dest_x, dest_y, dest_bitmap, pos, dest_y,
                size, MIN(cache_height, dest_height), 0xC0, -1, NULL);
      pos += size;
      size = MIN(size << 1, dest_x + dest_width - pos);
    }

    /* Step 3: Exponential doubling vertically */
    for (pos = dest_y + cache_height, size = MIN(cache_height, dest_y + dest_height - pos);
         pos < dest_y + dest_height;
         ) {
      BltBitMap(dest_bitmap, dest_x, dest_y, dest_bitmap, dest_x, pos,
                dest_width, size, 0xC0, -1, NULL);
      pos += size;
      size = MIN(size << 1, dest_y + dest_height - pos);
    }

    D(bug("RenderTiledCacheToRastPort: Used BltBitMap fast path\n"));
    return TRUE;
  }

  /*
   * FALLBACK: Use WritePixelArray for alpha blending or when no BitMap cache.
   * Tile the cache across the destination with simple iteration.
   */
  if (!cache_pixels) {
    D(bug("RenderTiledCacheToRastPort: No cache available!\n"));
    return FALSE;
  }

  for (WORD cy = 0; cy < dest_height; cy += cache_height) {
    UWORD tile_height = cache_height;
    if (cy + tile_height > dest_height)
      tile_height = dest_height - cy;

    for (WORD cx = 0; cx < dest_width; cx += cache_width) {
      UWORD tile_width = cache_width;
      if (cx + tile_width > dest_width)
        tile_width = dest_width - cx;

      if (needs_alpha_blend) {
        WritePixelArrayAlpha(cache_pixels, 0, 0, cache_pitch,
                             rastport, dest_x + cx, dest_y + cy,
                             tile_width, tile_height, 0xffffffff);
      } else {
        WritePixelArray(cache_pixels, 0, 0, cache_pitch,
                        rastport, dest_x + cx, dest_y + cy,
                        tile_width, tile_height, RECTFMT_ARGB);
      }
    }
  }

  D(bug("RenderTiledCacheToRastPort: Used WritePixelArray fallback\n"));
  return TRUE;
}

/**
 * RenderTiledCacheToDrawingBoard
 *
 * Renders using the pre-tiled cache directly to a locked DrawingBoard.
 * Uses memory copy operations for maximum speed.
 */
static BOOL RenderTiledCacheToDrawingBoard(struct RenderPort *rp,
                                           struct ZuneTexture *texture,
                                           WORD dest_x, WORD dest_y,
                                           UWORD dest_width, UWORD dest_height) {
  struct DrawingBoard *board = rp->target_board;
  ULONG *cache_pixels = (ULONG *)texture->tiled_cache_pixels;
  UWORD cache_width = texture->tiled_cache_width;
  UWORD cache_height = texture->tiled_cache_height;
  ULONG *dest_pixels = (ULONG *)board->pixels;
  ULONG dest_pitch_pixels = board->pitch / 4;

  if (!cache_pixels) {
    D(bug("RenderTiledCacheToDrawingBoard: No pixel cache available!\n"));
    return FALSE;
  }

  /* Determine if we need alpha blending */
  BOOL needs_alpha_blend = (texture->flags & ZUNE_TEXTURE_ALPHA) != 0 &&
                           (texture->flags & ZUNE_TEXTURE_OPAQUE) == 0;

  /* Clip destination to board bounds */
  if (dest_x < 0) { dest_width += dest_x; dest_x = 0; }
  if (dest_y < 0) { dest_height += dest_y; dest_y = 0; }
  if (dest_x + dest_width > board->width) dest_width = board->width - dest_x;
  if (dest_y + dest_height > board->height) dest_height = board->height - dest_y;

  if (dest_width <= 0 || dest_height <= 0)
    return TRUE;

  D(bug("RenderTiledCacheToDrawingBoard: dest=(%d,%d) %dx%d, cache=%dx%d\n",
        dest_x, dest_y, dest_width, dest_height, cache_width, cache_height));

  /* Process each destination row */
  for (UWORD dy = 0; dy < dest_height; dy++) {
    UWORD cache_y = dy % cache_height;
    ULONG *cache_row = cache_pixels + (cache_y * cache_width);
    ULONG *dest_row = dest_pixels + ((dest_y + dy) * dest_pitch_pixels) + dest_x;

    if (needs_alpha_blend) {
      /* Alpha blending path - use SIMD-accelerated blending */
      UWORD dx = 0;
      while (dx < dest_width) {
        UWORD cache_x = dx % cache_width;
        UWORD copy_width = cache_width - cache_x;
        if (dx + copy_width > dest_width)
          copy_width = dest_width - dx;

        /* Use SIMD-accelerated blending for the cache segment */
        cybergfx_blend_argb32_row(dest_row + dx, cache_row + cache_x, copy_width);
        dx += copy_width;
      }
    } else {
      /* Opaque path - direct memory copy */
      UWORD dx = 0;
      while (dx < dest_width) {
        UWORD cache_x = dx % cache_width;
        UWORD copy_width = cache_width - cache_x;
        if (dx + copy_width > dest_width)
          copy_width = dest_width - dx;

        CopyMem(cache_row + cache_x, dest_row + dx, copy_width * sizeof(ULONG));
        dx += copy_width;
      }
    }
  }

  return TRUE;
}

/*****************************************************************************/
/* Legacy Tiled Rendering (fallback without cache) */
/*****************************************************************************/

/**
 * CybergfxDrawTextureTiledToDrawingBoard
 *
 * Optimized tiled texture rendering directly to a locked DrawingBoard.
 * This function tiles an ARGB32 texture using direct memory access,
 * which is the fastest possible approach.
 *
 * Returns TRUE if successful, FALSE if fallback needed.
 */
static BOOL CybergfxDrawTextureTiledToDrawingBoard(struct RenderPort *rp,
                                                   struct ZuneTexture *texture,
                                                   WORD dest_x, WORD dest_y,
                                                   UWORD dest_width,
                                                   UWORD dest_height) {
  struct DrawingBoard *board = rp->target_board;

  if (!board || !board->pixels_locked)
    return FALSE;

  /* Only handle ARGB32 pixel formats */
  if (board->pixel_format != PIXFMT_ARGB32 &&
      board->pixel_format != PIXFMT_RGBA32)
    return FALSE;

  /* Only handle ARGB32 textures */
  if (texture->format != ZUNE_TEXTURE_FORMAT_ARGB32)
    return FALSE;

  UWORD tex_width = texture->width;
  UWORD tex_height = texture->height;

  /* Determine if we need alpha blending:
   * - If ZUNE_TEXTURE_OPAQUE is set, texture is fully opaque - use fast copy
   * - If ZUNE_TEXTURE_ALPHA is set but not OPAQUE, need alpha blending
   * - If neither ALPHA flag is set, texture is opaque - use fast copy */
  BOOL needs_alpha_blend = (texture->flags & ZUNE_TEXTURE_ALPHA) != 0 &&
                           (texture->flags & ZUNE_TEXTURE_OPAQUE) == 0;

  ULONG *src_pixels = (ULONG *)texture->pixel_data;
  ULONG src_pitch_pixels = texture->pitch / 4;
  ULONG *dest_pixels = (ULONG *)board->pixels;
  ULONG dest_pitch_pixels = board->pitch / 4;

  D(bug("CybergfxDrawTextureTiledToDrawingBoard: Tiling %dx%d to %dx%d, needs_alpha=%d (flags=0x%x)\n",
        tex_width, tex_height, dest_width, dest_height, needs_alpha_blend, texture->flags));

  /* Clip destination to board bounds */
  if (dest_x < 0) { dest_width += dest_x; dest_x = 0; }
  if (dest_y < 0) { dest_height += dest_y; dest_y = 0; }
  if (dest_x + dest_width > board->width) dest_width = board->width - dest_x;
  if (dest_y + dest_height > board->height) dest_height = board->height - dest_y;

  if (dest_width <= 0 || dest_height <= 0)
    return TRUE; /* Nothing to draw, but not an error */

  /* Process each destination row */
  for (UWORD dy = 0; dy < dest_height; dy++) {
    UWORD src_y = dy % tex_height;
    ULONG *src_row = src_pixels + (src_y * src_pitch_pixels);
    ULONG *dest_row = dest_pixels + ((dest_y + dy) * dest_pitch_pixels) + dest_x;

    if (needs_alpha_blend) {
      /* Alpha blending path - use SIMD-accelerated row blending */
      UWORD dx = 0;
      while (dx < dest_width) {
        UWORD src_x = dx % tex_width;
        UWORD copy_width = tex_width - src_x;
        if (dx + copy_width > dest_width)
          copy_width = dest_width - dx;

        /* Use SIMD-accelerated blending for the tile segment */
        cybergfx_blend_argb32_row(dest_row + dx, src_row + src_x, copy_width);
        dx += copy_width;
      }
    } else {
      /* Opaque path - direct memory copy, much faster */
      UWORD dx = 0;
      while (dx < dest_width) {
        UWORD src_x = dx % tex_width;
        UWORD copy_width = tex_width - src_x;
        if (dx + copy_width > dest_width)
          copy_width = dest_width - dx;

        CopyMem(src_row + src_x, dest_row + dx, copy_width * sizeof(ULONG));
        dx += copy_width;
      }
    }
  }

  D(bug("CybergfxDrawTextureTiledToDrawingBoard: Completed\n"));
  return TRUE;
}

/**
 * CybergfxDrawTextureTiledToRastPort
 *
 * Optimized tiled texture rendering using bulk WritePixelArray operations.
 * This function tiles an ARGB32 texture across the destination area using
 * row-by-row WritePixelArray calls, which is much faster than drawing
 * individual tiles through the generic DrawTexture path.
 *
 * Returns TRUE if successful, FALSE if fallback to slow path needed.
 */
static BOOL CybergfxDrawTextureTiledToRastPort(struct RenderPort *rp,
                                               struct ZuneTexture *texture,
                                               WORD dest_x, WORD dest_y,
                                               UWORD dest_width,
                                               UWORD dest_height) {
  struct RastPort *rastport = rp->target_rp;

  if (!rastport || !rastport->BitMap)
    return FALSE;

  /* Only handle ARGB32 format */
  if (texture->format != ZUNE_TEXTURE_FORMAT_ARGB32)
    return FALSE;

  UWORD tex_width = texture->width;
  UWORD tex_height = texture->height;

  /* Determine if we need alpha blending:
   * - If ZUNE_TEXTURE_OPAQUE is set, texture is fully opaque - use WritePixelArray
   * - If ZUNE_TEXTURE_ALPHA is set but not OPAQUE, need WritePixelArrayAlpha
   * - If neither ALPHA flag is set, texture is opaque - use WritePixelArray */
  BOOL needs_alpha_blend = (texture->flags & ZUNE_TEXTURE_ALPHA) != 0 &&
                           (texture->flags & ZUNE_TEXTURE_OPAQUE) == 0;

  ULONG *src_pixels = (ULONG *)texture->pixel_data;
  ULONG src_pitch_pixels = texture->pitch / 4;

  D(bug("CybergfxDrawTextureTiledToRastPort: Tiling %dx%d texture to %dx%d area, needs_alpha=%d (flags=0x%x)\n",
        tex_width, tex_height, dest_width, dest_height, needs_alpha_blend, texture->flags));

  /* Allocate a temporary row buffer for the tiled scanline */
  ULONG *row_buffer = AllocVec(dest_width * sizeof(ULONG), MEMF_PUBLIC);
  if (!row_buffer) {
    D(bug("CybergfxDrawTextureTiledToRastPort: Failed to allocate row buffer\n"));
    return FALSE;
  }

  /* Process each destination row */
  for (UWORD dy = 0; dy < dest_height; dy++) {
    /* Calculate which texture row to sample (with wrapping) */
    UWORD src_y = dy % tex_height;
    ULONG *src_row = src_pixels + (src_y * src_pitch_pixels);

    /* Build the tiled row in the buffer */
    UWORD dx = 0;
    while (dx < dest_width) {
      /* Calculate how many pixels to copy from the texture row */
      UWORD src_x = dx % tex_width;
      UWORD copy_width = tex_width - src_x;

      /* Don't copy beyond destination width */
      if (dx + copy_width > dest_width)
        copy_width = dest_width - dx;

      /* Copy pixels from texture row to buffer */
      CopyMem(src_row + src_x, row_buffer + dx, copy_width * sizeof(ULONG));
      dx += copy_width;
    }

    /* Write the entire row to the RastPort */
    if (needs_alpha_blend) {
      WritePixelArrayAlpha(row_buffer, 0, 0, dest_width * 4,
                           rastport, dest_x, dest_y + dy, dest_width, 1,
                           0xffffffff);
    } else {
      WritePixelArray(row_buffer, 0, 0, dest_width * 4,
                      rastport, dest_x, dest_y + dy, dest_width, 1,
                      RECTFMT_ARGB);
    }
  }

  FreeVec(row_buffer);

  D(bug("CybergfxDrawTextureTiledToRastPort: Completed tiling\n"));
  return TRUE;
}

/**
 * CybergfxDrawTextureTiledFast
 *
 * Optimized tiled texture rendering using per-texture pre-tiled cache.
 *
 * This function implements the same caching strategy as the legacy
 * dt_put_on_rastport_tiled() function in datatypescache.c:
 *
 * 1. On first call, creates a 256x256 pre-tiled cache for the texture
 * 2. Uses the cache for all subsequent tiled renders
 * 3. The cache persists for the lifetime of the texture
 *
 * This is much more efficient than per-window caching because:
 * - Cache is created once per texture, not once per window
 * - Cache is smaller (256x256 vs full window size)
 * - Cache is reused across all windows using the same texture
 * - No cache invalidation needed when drawing different areas
 *
 * Returns TRUE if successful, FALSE if fallback to slow path needed.
 */
BOOL CybergfxDrawTextureTiledFast(struct RenderPort *rp,
                                  struct ZuneTexture *texture,
                                  WORD dest_x, WORD dest_y,
                                  UWORD dest_width, UWORD dest_height) {
  D(bug("CybergfxDrawTextureTiledFast: ENTER rp=%p target_board=%p target_rp=%p\n",
        rp, rp ? rp->target_board : NULL, rp ? rp->target_rp : NULL));

  /* Validate basic parameters */
  if (!rp || !texture || !texture->pixel_data) {
    D(bug("CybergfxDrawTextureTiledFast: Invalid params - returning FALSE\n"));
    return FALSE;
  }

  /* Only handle ARGB32 format */
  if (texture->format != ZUNE_TEXTURE_FORMAT_ARGB32)
    return FALSE;

  /* Don't bother with tiny textures or destinations */
  if (texture->width == 0 || texture->height == 0 ||
      dest_width == 0 || dest_height == 0)
    return FALSE;

  /* Get the target RastPort for friend bitmap allocation */
  struct RastPort *friend_rp = rp->target_rp;
  if (!friend_rp && rp->target_board)
    friend_rp = rp->target_board->rastport;

  /*
   * FAST PATH: Use pre-tiled cache if available or can be created.
   *
   * The cache includes:
   * 1. A native BitMap (for hardware BltBitMap - fastest)
   * 2. An ARGB32 pixel buffer (for software fallback)
   *
   * The BitMap is allocated as a friend of the target RastPort's bitmap
   * for optimal hardware compatibility.
   */
  if (EnsureTiledCache(texture, friend_rp)) {
    /* We have a pre-tiled cache - use it */

    /* Try DrawingBoard path first (fastest when locked) */
    if (rp->target_board && rp->target_board->pixels_locked) {
      D(bug("CybergfxDrawTextureTiledFast: Trying locked DrawingBoard path\n"));
      if (RenderTiledCacheToDrawingBoard(rp, texture, dest_x, dest_y,
                                         dest_width, dest_height)) {
        D(bug("CybergfxDrawTextureTiledFast: Locked DrawingBoard path SUCCESS\n"));
        return TRUE;
      }
    }

    /* Try RastPort path */
    if (rp->target_rp && !rp->target_board) {
      D(bug("CybergfxDrawTextureTiledFast: Trying direct RastPort path\n"));
      if (RenderTiledCacheToRastPort(rp, texture, dest_x, dest_y,
                                     dest_width, dest_height)) {
        D(bug("CybergfxDrawTextureTiledFast: Direct RastPort path SUCCESS\n"));
        return TRUE;
      }
    }

    /* Fall back to unlocked DrawingBoard's rastport if available */
    if (rp->target_board && rp->target_board->rastport) {
      D(bug("CybergfxDrawTextureTiledFast: Trying unlocked DrawingBoard rastport path (board->rastport=%p, bitmap=%p)\n",
            rp->target_board->rastport, rp->target_board->rastport ? rp->target_board->rastport->BitMap : NULL));
      struct RenderPort temp_rp = *rp;
      temp_rp.target_rp = rp->target_board->rastport;
      temp_rp.target_board = NULL;
      if (RenderTiledCacheToRastPort(&temp_rp, texture, dest_x, dest_y,
                                     dest_width, dest_height)) {
        D(bug("CybergfxDrawTextureTiledFast: Unlocked DrawingBoard rastport path SUCCESS\n"));
        return TRUE;
      }
    }
  }

  /*
   * FALLBACK: No cache available (texture too large or non-ARGB32).
   * Use direct tiling without cache.
   */

  /* Try DrawingBoard path first (fastest when locked) */
  if (rp->target_board && rp->target_board->pixels_locked) {
    if (CybergfxDrawTextureTiledToDrawingBoard(rp, texture, dest_x, dest_y,
                                                dest_width, dest_height)) {
      return TRUE;
    }
  }

  /* Try RastPort path */
  if (rp->target_rp && !rp->target_board) {
    if (CybergfxDrawTextureTiledToRastPort(rp, texture, dest_x, dest_y,
                                           dest_width, dest_height)) {
      return TRUE;
    }
  }

  /* Fall back to unlocked DrawingBoard's rastport if available */
  if (rp->target_board && rp->target_board->rastport) {
    struct RenderPort temp_rp = *rp;
    temp_rp.target_rp = rp->target_board->rastport;
    temp_rp.target_board = NULL;
    if (CybergfxDrawTextureTiledToRastPort(&temp_rp, texture, dest_x, dest_y,
                                           dest_width, dest_height)) {
      return TRUE;
    }
  }

  return FALSE;
}
