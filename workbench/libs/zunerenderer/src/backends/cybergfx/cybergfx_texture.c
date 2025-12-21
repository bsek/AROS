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

#include "../../zunerenderer_intern.h"
#include "../backend_interface.h"
#include "cybergfx_backend.h"
#include "cybergfx_pixel_format.h"
#include "cybergfx_simd.h"
#include "libraries/zunerenderer.h"
#include <stdbool.h>
#include <string.h>
#include <immintrin.h>

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************/
/* Constants and Limits */
/*****************************************************************************/

/*****************************************************************************/
/* Constants and Limits */
/*****************************************************************************/

#define CYBERGFX_MAX_TEXTURE_SIZE 4096
#define CYBERGFX_MIN_TEXTURE_SIZE 1

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

  ULONG dest_pixfmt = GetCyberMapAttr(rastport->BitMap, CYBRMATTR_PIXFMT);
  ULONG bytes_per_pixel = GetBytesPerPixel(texture->format);
  UBYTE *src_pixels = (UBYTE *)texture->pixel_data;

  D(bug("CybergfxDrawTextureToRastPort: texture->pitch=%d, bytes_per_pixel=%d, "
        "texture dimensions=%dx%d\n",
        texture->pitch, bytes_per_pixel, texture->width, texture->height));
  D(bug("CybergfxDrawTextureToRastPort: scale_x=0x%08X, scale_y=0x%08X, "
        "cached_hidd_obj=%p\n",
        scale_x, scale_y, bitmap_obj));

  /* Render pixel by pixel */
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
