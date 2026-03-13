/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Texture Internal Functions
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <clib/alib_protos.h>
#include <clib/arossupport_protos.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <aros/cpu.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include <string.h>

#include <proto/datatypes.h>

#define DT_V44_SUPPORT
#include <datatypes/pictureclass.h>

#include "../zunegfx_intern.h"
#include "../drawingboard/drawingboard_intern.h"
#include "texture_intern.h"

#ifndef mskHasAlpha
#define mskHasAlpha 4
#endif

/*****************************************************************************/
/* Internal Texture Helper Functions */
/*****************************************************************************/

struct ZuneTexture *AllocateTexture(void) {
    struct ZuneTexture *texture = AllocVec(sizeof(struct ZuneTexture), MEMF_CLEAR | MEMF_PUBLIC);
    if (texture) {
        texture->valid = FALSE;
        texture->pixels_locked = FALSE;
        texture->hardware_texture = FALSE;
        texture->ref_count = 1;
        texture->backend_type = BACKEND_SOFTWARE;
    }
    return texture;
}

void InitializeTexture(struct ZuneTexture *texture, UWORD width, UWORD height, UBYTE depth, ULONG format, ULONG flags) {
    texture->width = width;
    texture->height = height;
    texture->depth = depth;
    texture->format = format;
    texture->flags = flags;
    texture->pitch = CalculateTexturePitch(width, format);
    texture->data_size = CalculateTextureSize(width, height, format);
    texture->valid = TRUE;
}

/* Get bits per pixel for a texture format */
ULONG ConvertCyberGfxFormatToZuneFormat(ULONG cybergfx_format) {
    /* Convert CyberGraphics pixel format to Zune texture format */
    switch (cybergfx_format) {
    case 11: /* vHidd_StdPixFmt_ARGB32 */
        return ZUNE_TEXTURE_FORMAT_ARGB32;
    case 0: /* RGB24 */
        return ZUNE_TEXTURE_FORMAT_RGB24;
    case 2: /* RGB16 */
        return ZUNE_TEXTURE_FORMAT_RGB16;
    default:
        D(bug("ZuneRenderer: Unknown CyberGraphics format 0x%08x, defaulting to "
              "ARGB32\n",
              cybergfx_format));
        return ZUNE_TEXTURE_FORMAT_ARGB32;
    }
}

ULONG GetTextureFormatBPP(ULONG format) {
    switch (format) {
    case ZUNE_TEXTURE_FORMAT_ARGB32:
        return 32;
    case ZUNE_TEXTURE_FORMAT_RGB24:
        return 24;
    case ZUNE_TEXTURE_FORMAT_ARGB16:
    case ZUNE_TEXTURE_FORMAT_RGB16:
        return 16;
    case ZUNE_TEXTURE_FORMAT_L8:
    case ZUNE_TEXTURE_FORMAT_A8:
        return 8;
    default:
        D(bug("ZuneRenderer: Unknown texture format 0x%08x\n", format));
        return 0;
    }
}

/*****************************************************************************/
/* Internal Texture Management Helper Functions */
/*****************************************************************************/

void AddTextureToList(struct IntZuneGfxBase *base, struct ZuneTexture *texture) {
    if (!base || !texture)
        return;

    ObtainSemaphore(&base->lock);
    AddTail((struct List *)&base->textures, (struct Node *)&texture->node);
    ReleaseSemaphore(&base->lock);
}

void RemoveTextureFromList(struct IntZuneGfxBase *base, struct ZuneTexture *texture) {
    if (!base || !texture)
        return;

    ObtainSemaphore(&base->lock);
    Remove((struct Node *)&texture->node);
    ReleaseSemaphore(&base->lock);
}

BOOL AllocateTextureData(struct ZuneTexture *texture) {
    if (!texture) {
        D(bug("ZuneRenderer: AllocateTextureData - Invalid texture pointer\n"));
        return FALSE;
    }

    if (texture->pixel_data) {
        D(bug("ZuneRenderer: AllocateTextureData - Texture data already "
              "allocated\n"));
        return TRUE; /* Already allocated */
    }

    /* Calculate data size if not already done */
    if (texture->data_size == 0) {
        texture->data_size = CalculateTextureSize(texture->width, texture->height, texture->format);
    }

    if (texture->pitch == 0) {
        texture->pitch = CalculateTexturePitch(texture->width, texture->format);
    }

    if (texture->data_size == 0) {
        D(bug("ZuneRenderer: AllocateTextureData - Invalid texture size "
              "calculation\n"));
        return FALSE;
    }

    /* Allocate pixel data */
    texture->pixel_data = AllocVec(texture->data_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!texture->pixel_data) {
        D(bug("ZuneRenderer: AllocateTextureData - Failed to allocate %u bytes\n", texture->data_size));
        return FALSE;
    }

    D(bug("ZuneRenderer: AllocateTextureData - Allocated %u bytes for %dx%d "
          "texture\n",
          texture->data_size, texture->width, texture->height));

    return TRUE;
}

void FreeTextureData(struct ZuneTexture *texture) {
    if (!texture) {
        return;
    }

    if (texture->pixel_data) {
        D(bug("ZuneRenderer: FreeTextureData - Freeing %u bytes\n", texture->data_size));
        FreeVec(texture->pixel_data);
        texture->pixel_data = NULL;
        texture->data_size = 0;
    }
}

ULONG CalculateTexturePitch(UWORD width, ULONG format) {
    ULONG bytes_per_pixel = GetTextureFormatBPP(format) / 8;
    if (bytes_per_pixel == 0) {
        D(bug("ZuneRenderer: CalculateTexturePitch - Invalid format 0x%08x\n", format));
        return 0;
    }

    ULONG pitch = width * bytes_per_pixel;

    /* Align pitch to 4-byte boundary for better performance */
    pitch = (pitch + 3) & ~3;

    return pitch;
}

ULONG CalculateTextureSize(UWORD width, UWORD height, ULONG format) {
    ULONG pitch = CalculateTexturePitch(width, format);
    if (pitch == 0) {
        return 0;
    }

    return pitch * height;
}

/*
 * Get backend for texture operations.
 * Uses RenderContext's backend if available, otherwise falls back to texture's stored backend.
 */
ZuneBackend *GetTextureBackend(struct RenderContext *rctx, struct ZuneTexture *texture) {
    ZuneBackend *backend = NULL;

    /* Prefer RenderContext's backend - it's already set up */
    if (rctx && rctx->backend_type != BACKEND_SOFTWARE &&
        rctx->backend_type != BACKEND_BEST_AVAILABLE) {
        backend = ZuneFindBackendByType(rctx->backend_type);
        if (backend && BACKEND_HAS_CAP(backend, BACKEND_CAP_TEXTURES))
            return backend;
    }

    /* Fall back to texture's assigned backend */
    if (texture && texture->backend_type &&
        texture->backend_type != BACKEND_SOFTWARE &&
        texture->backend_type != BACKEND_BEST_AVAILABLE) {
        backend = ZuneFindBackendByType(texture->backend_type);
        if (backend && BACKEND_HAS_CAP(backend, BACKEND_CAP_TEXTURES))
            return backend;
    }

    return NULL;
}

BOOL ValidateTexture(struct ZuneTexture *texture) {
    if (!texture) {
        return FALSE;
    }

    /* Check basic texture properties */
    if (texture->width == 0 || texture->height == 0) {
        return FALSE;
    }

    if (!texture->valid) {
        return FALSE;
    }

    /* Check storage presence: either CPU pixels or backend-owned handle */
    if (!texture->pixel_data && !texture->backend_handle && !texture->hardware_texture) {
        return FALSE;
    }

    if (texture->pixel_data) {
        ULONG expected_size = CalculateTextureSize(texture->width, texture->height, texture->format);
        if (expected_size == 0 || texture->data_size != expected_size) {
            return FALSE;
        }
    }

    return TRUE;
}

struct ZuneTexture *CreateTextureFromDatatypeInternal(APTR dt_handle, ULONG flags) {
    Object *dt_obj = (Object *)dt_handle;
    struct BitMapHeader *bmhd = NULL;
    struct BitMap *bm = NULL;
    WORD tex_w = 0, tex_h = 0;
    UBYTE mask = 0;

    if (!dt_obj) {
        D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - missing datatype object (handle=%p)\n", dt_handle));
        return NULL;
    }

    GetDTAttrs(dt_obj, PDTA_BitMapHeader, &bmhd, PDTA_BitMap, &bm, TAG_DONE);
    if (bmhd) {
        tex_w = bmhd->bmh_Width;
        tex_h = bmhd->bmh_Height;
        mask = bmhd->bmh_Masking;
        D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - from bmhd: %dx%d, mask=%d\n", tex_w, tex_h, mask));
    } else if (bm) {
        tex_w = GetBitMapAttr(bm, BMA_WIDTH);
        tex_h = GetBitMapAttr(bm, BMA_HEIGHT);
        D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - from bitmap: %dx%d\n", tex_w, tex_h));
    }

    if (tex_w <= 0 || tex_h <= 0) {
        D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - invalid dimensions %dx%d\n", tex_w, tex_h));
        return NULL;
    }

    ULONG buf_size = (ULONG)tex_w * (ULONG)tex_h * 4;
    ULONG *pixels = AllocVec(buf_size, MEMF_ANY);
    if (!pixels) {
        D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - failed to alloc %lu bytes\n", (ULONG)buf_size));
        return NULL;
    }

    struct pdtBlitPixelArray pa;
    memset(&pa, 0, sizeof(pa));
    pa.MethodID = PDTM_READPIXELARRAY;
    pa.pbpa_PixelData = (UBYTE *)pixels;
    pa.pbpa_PixelFormat = PBPAFMT_ARGB;
    pa.pbpa_PixelArrayMod = tex_w * 4;
    pa.pbpa_Left = 0;
    pa.pbpa_Top = 0;
    pa.pbpa_Width = tex_w;
    pa.pbpa_Height = tex_h;

    DoMethodA(dt_obj, (Msg)&pa);

    D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - after PDTM_READPIXELARRAY:\n"));
    D(bug("  First 4 pixels: %08lx %08lx %08lx %08lx\n",
          (ULONG)pixels[0], (ULONG)pixels[1],
          (ULONG)pixels[2], (ULONG)pixels[3]));

    /*
     * Fix alpha channel for images without alpha.
     * PDTM_READPIXELARRAY with PBPAFMT_ARGB returns 32-bit pixels, but for
     * images without alpha the alpha byte is undefined (often 0x00 = fully
     * transparent). We must set it to 0xFF (fully opaque) to render correctly.
     * This matches the behavior in datatypescache.c GetImageFromFile().
     */
    if (mask != mskHasAlpha)
    {
        ULONG pixel_count = (ULONG)tex_w * (ULONG)tex_h;
        ULONG i;
#if !AROS_BIG_ENDIAN
        for (i = 0; i < pixel_count; i++)
            pixels[i] |= 0x000000ff;  /* Set alpha to 0xFF (little-endian: BGRA in memory) */
#else
        for (i = 0; i < pixel_count; i++)
            pixels[i] |= 0xff000000;  /* Set alpha to 0xFF (big-endian: ARGB in memory) */
#endif
    }

    if (mask == mskHasAlpha)
        flags |= ZUNE_TEXTURE_ALPHA;

    struct ZuneTexture *tex = CreateTextureFromDataInternal(pixels, (UWORD)tex_w, (UWORD)tex_h, 32, ZUNE_TEXTURE_FORMAT_ARGB32, tex_w * 4, flags);

    if (!tex) {
        D(bug("ZuneRenderer: ZuneCreateTextureFromDatatype - CreateTextureFromDataInternal failed\n"));
    }

    FreeVec(pixels);
    return tex;
}

struct ZuneTexture *CreateTextureFromDataInternal(APTR data, UWORD width, UWORD height, UBYTE depth, ULONG format, ULONG pitch, ULONG flags) {
    struct ZuneTexture *texture;
    ULONG row, copy_width;
    UBYTE *src_ptr, *dst_ptr;

    if (!data || width == 0 || height == 0 || pitch == 0) {
        D(bug("ZuneRenderer: CreateTextureFromDataInternal - Invalid parameters\n"));
        return NULL;
    }

    texture = AllocateTexture();
    if (!texture) {
        D(bug("ZuneRenderer: CreateTextureFromDataInternal - Failed to allocate "
              "texture structure\n"));
        return NULL;
    }

    InitializeTexture(texture, width, height, depth, format, flags);

    if (!AllocateTextureData(texture)) {
        D(bug("ZuneRenderer: CreateTextureFromDataInternal - Failed to allocate "
              "texture data\n"));
        FreeVec(texture);
        return NULL;
    }

    /* Copy pixel data row by row with endian conversion if needed */
    src_ptr = (UBYTE *)data;
    dst_ptr = (UBYTE *)texture->pixel_data;
    copy_width = MIN(pitch, texture->pitch);

    for (row = 0; row < height; row++) {
        CopyMem(src_ptr, dst_ptr, copy_width);

        src_ptr += pitch;
        dst_ptr += texture->pitch;
    }

    /*
     * Detect opaque textures for rendering optimization.
     * If all pixels have alpha == 0xFF, we can skip alpha blending entirely
     * in the rendering path, which is significantly faster.
     * 
     * Only check ARGB32 format textures that have the ALPHA flag set,
     * as those are the ones where this optimization matters.
     */
    if (format == ZUNE_TEXTURE_FORMAT_ARGB32 && (flags & ZUNE_TEXTURE_ALPHA)) {
        BOOL is_opaque = TRUE;
        ULONG pixel_count = (ULONG)width * (ULONG)height;
        ULONG *pixels = (ULONG *)texture->pixel_data;
        ULONG i;

#if !AROS_BIG_ENDIAN
        /* Little-endian: alpha is in the low byte (BGRA memory layout) */
        for (i = 0; i < pixel_count; i++) {
            if ((pixels[i] & 0x000000FF) != 0xFF) {
                is_opaque = FALSE;
                break;
            }
        }
#else
        /* Big-endian: alpha is in the high byte (ARGB memory layout) */
        for (i = 0; i < pixel_count; i++) {
            if ((pixels[i] & 0xFF000000) != 0xFF000000) {
                is_opaque = FALSE;
                break;
            }
        }
#endif

        if (is_opaque) {
            texture->flags |= ZUNE_TEXTURE_OPAQUE;
            D(bug("ZuneRenderer: CreateTextureFromDataInternal - Texture is fully opaque\n"));
        }
    } else if (!(flags & ZUNE_TEXTURE_ALPHA)) {
        /* No alpha channel means fully opaque by definition */
        texture->flags |= ZUNE_TEXTURE_OPAQUE;
    }

    D(bug("ZuneRenderer: CreateTextureFromDataInternal - Texture created "
          "successfully (%p), opaque=%d\n",
          texture, (texture->flags & ZUNE_TEXTURE_OPAQUE) ? 1 : 0));

    return texture;
}

struct ZuneTexture *CreateTextureFromDrawingBoardInternal(
    struct IntZuneGfxBase *base, struct RenderContext *rctx, ULONG flags)
{
    struct DrawingBoard *board = rctx->target_board;

    D(bug("ZuneRenderer: CreateTextureFromDrawingBoardInternal(board=%p, flags=0x%08x)\n", board, flags));

    ULONG pitch;
    APTR pixels = LockDrawingBoardPixelsInternal(rctx, &pitch);
    if (!pixels) {
        D(bug("ZuneRenderer: Failed to lock DrawingBoard pixels\n"));
        return NULL;
    }

    /* Convert CyberGraphics pixel format to Zune texture format */
    ULONG zune_format = ConvertCyberGfxFormatToZuneFormat(board->pixel_format);

    struct ZuneTexture *texture = CreateTextureFromDataInternal(pixels, board->width, board->height, board->depth, zune_format, pitch, flags);

    if (texture) {
        ZuneBackend *backend = GetTextureBackend(rctx, texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    UnlockDrawingBoardPixelsInternal(rctx);

    D(bug("ZuneRenderer: Texture created from DrawingBoard %s (%p)\n", texture ? "successfully" : "failed", texture));

    return texture;
}

/* Texture rendering implemented directly in library functions */

/*****************************************************************************/
/* Texture Management Functions */
/*****************************************************************************/

void UnlockTexturePixelsInternal(struct ZuneTexture *texture) {
    if (!texture) {
        D(bug("ZuneRenderer: Invalid texture\n"));
        return;
    }

    if (!texture->pixels_locked) {
        D(bug("ZuneRenderer: Texture pixels not locked\n"));
        return;
    }

    texture->pixels_locked = FALSE;
}
