/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Texture Management Implementation

    This module provides texture management functionality for the Zune Renderer
    library, including texture creation, data operations, rendering, and
   utilities. All functions use AROS library macros and follow AROS conventions.
*/

#include "backends/backend_interface.h"
#include "include/zunerenderer.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <clib/alib_protos.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include <string.h>

#include <proto/datatypes.h>

#define DT_V44_SUPPORT
#include <datatypes/pictureclass.h>

#include "src/zunerenderer_intern.h"

#ifndef mskHasAlpha
#define mskHasAlpha 4
#endif

/*****************************************************************************/
/* Internal Texture Helper Functions */
/*****************************************************************************/

static struct ZuneTexture *AllocateTexture(void) {
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

static void InitializeTexture(struct ZuneTexture *texture, UWORD width, UWORD height, UBYTE depth, ULONG format, ULONG flags) {
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
static ULONG ConvertCyberGfxFormatToZuneFormat(ULONG cybergfx_format) {
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

static ULONG GetTextureFormatBPP(ULONG format) {
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

void AddTextureToList(struct IntZuneRendererBase *base, struct ZuneTexture *texture) {
    if (!base || !texture)
        return;

    ObtainSemaphore(&base->lock);
    AddTail((struct List *)&base->textures, (struct Node *)&texture->node);
    ReleaseSemaphore(&base->lock);
}

void RemoveTextureFromList(struct IntZuneRendererBase *base, struct ZuneTexture *texture) {
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

static ZuneBackend *SelectTextureBackend(struct ZuneTexture *texture) {
    ZuneBackend *backend = NULL;

    if (texture && texture->backend_type)
        backend = ZuneFindBackendByType(texture->backend_type);

    if (!backend)
        backend = ZuneFindBestBackend(NULL);

    if (backend && BACKEND_HAS_CAP(backend, BACKEND_CAP_TEXTURES))
        return backend;

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

static struct ZuneTexture *CreateTextureFromDatatypeInternal(APTR dt_handle, ULONG flags) {
    Object *dt_obj = (Object *)dt_handle;
    struct BitMapHeader *bmhd = NULL;
    struct BitMap *bm = NULL;
    int tex_w = 0, tex_h = 0;
    UBYTE mask = 0;

    if (!dt_obj) {
        D(bug("ZuneRenderer: CreateTextureFromDatatype - missing datatype object (handle=%p)\n", dt_handle));
        return NULL;
    }

    GetDTAttrs(dt_obj, PDTA_BitMapHeader, &bmhd, PDTA_BitMap, &bm, TAG_DONE);
    if (bmhd) {
        tex_w = bmhd->bmh_Width;
        tex_h = bmhd->bmh_Height;
        mask = bmhd->bmh_Masking;
    } else if (bm) {
        tex_w = GetBitMapAttr(bm, BMA_WIDTH);
        tex_h = GetBitMapAttr(bm, BMA_HEIGHT);
    }

    if (tex_w <= 0 || tex_h <= 0) {
        D(bug("ZuneRenderer: CreateTextureFromDatatype - invalid dimensions %dx%d\n", tex_w, tex_h));
        return NULL;
    }

    size_t buf_size = (size_t)tex_w * (size_t)tex_h * 4;
    ULONG *pixels = AllocVec(buf_size, MEMF_ANY);
    if (!pixels) {
        D(bug("ZuneRenderer: CreateTextureFromDatatype - failed to alloc %lu bytes\n", (ULONG)buf_size));
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
    // if (DoMethodA(dt_obj, (Msg)&pa) == 0) {
    //    D(bug("ZuneRenderer: CreateTextureFromDatatype - PDTM_READPIXELARRAY failed, trying bitmap fallback\n"));

    //   if (bm && tex_w > 0 && tex_h > 0) {
    //     struct RastPort rp;
    //     InitRastPort(&rp);
    //     rp.BitMap = bm;

    //     ULONG rc = ReadPixelArray(pixels, 0, 0, tex_w * 4, &rp, 0, 0, tex_w,
    //                               tex_h, RECTFMT_ARGB);
    //     if (rc == 0) {
    //       D(bug("ZuneRenderer: CreateTextureFromDatatype - ReadPixelArray fallback failed\n"));
    //       FreeVec(pixels);
    //       return NULL;
    //     }
    //   } else {
    //     D(bug("ZuneRenderer: CreateTextureFromDatatype - no valid BitMap fallback\n"));
    //     FreeVec(pixels);
    //     return NULL;
    //   }
    // }

    if (mask == mskHasAlpha)
        flags |= ZUNE_TEXTURE_ALPHA;

    struct ZuneTexture *tex = CreateTextureFromDataInternal(pixels, (UWORD)tex_w, (UWORD)tex_h, 32, ZUNE_TEXTURE_FORMAT_ARGB32, tex_w * 4, flags);

    if (!tex) {
        D(bug("ZuneRenderer: CreateTextureFromDatatype - CreateTextureFromDataInternal failed\n"));
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

    D(bug("ZuneRenderer: CreateTextureFromDataInternal - Texture created "
          "successfully (%p)\n",
          texture));

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

/*****************************************************************************

    NAME */
AROS_LH5(struct ZuneTexture *, CreateTexture,

         /*  SYNOPSIS */
         AROS_LHA(UWORD, width, D0), AROS_LHA(UWORD, height, D1), AROS_LHA(UBYTE, depth, D2), AROS_LHA(ULONG, format, D3), AROS_LHA(ULONG, flags, D4),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 70, zunerenderer)

/*  FUNCTION
    Creates a new texture with the specified dimensions and format.

INPUTS
    width - Texture width in pixels
    height - Texture height in pixels
    depth - Color depth in bits
    format - Pixel format (ZUNE_TEXTURE_FORMAT_*)
    flags - Texture creation flags (ZUNE_TEXTURE_*)

RESULT
    Pointer to new ZuneTexture structure, or NULL if creation failed.

NOTES
    The created texture must be freed with DestroyTexture().
    Texture data is initially undefined.

SEE ALSO
    CreateTextureFromData(), CreateTextureFromDrawingBoard(), DestroyTexture()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
    struct ZuneTexture *texture;

    ENTER_FUNCTION("CreateTexture");

    D(bug("ZuneRenderer: CreateTexture(width=%d, height=%d, depth=%d, "
          "format=0x%08x, flags=0x%08x)\n",
          width, height, depth, format, flags));

    if (width == 0 || height == 0) {
        D(bug("ZuneRenderer: Invalid texture dimensions\n"));
        return NULL;
    }

    texture = AllocateTexture();
    if (!texture) {
        D(bug("ZuneRenderer: Failed to allocate texture structure\n"));
        return NULL;
    }

    InitializeTexture(texture, width, height, depth, format, flags);

    AllocateTextureData(texture); /* best-effort for CPU path */

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->InitTexture) {
        if (backend->ops->InitTexture(texture)) {
            texture->backend_type = backend->ops->type;
        }
    }

    AddTextureToList(base, texture);

    D(bug("ZuneRenderer: Texture created successfully (%p)\n", texture));

    EXIT_FUNCTION("CreateTexture");
    return texture;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH7(struct ZuneTexture *, CreateTextureFromData,

         /*  SYNOPSIS */
         AROS_LHA(APTR, data, A0), AROS_LHA(UWORD, width, D0), AROS_LHA(UWORD, height, D1), AROS_LHA(UBYTE, depth, D2), AROS_LHA(ULONG, format, D3),
         AROS_LHA(ULONG, pitch, D4), AROS_LHA(ULONG, flags, D5),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 71, zunerenderer)

/*  FUNCTION
    Creates a new texture from existing pixel data.

INPUTS
    data - Pointer to source pixel data
    width - Texture width in pixels
    height - Texture height in pixels
    depth - Color depth in bits
    format - Pixel format (ZUNE_TEXTURE_FORMAT_*)
    pitch - Bytes per row in source data
    flags - Texture creation flags (ZUNE_TEXTURE_*)

RESULT
    Pointer to new ZuneTexture structure, or NULL if creation failed.

NOTES
    The source data is copied into the texture's internal buffer.
    The created texture must be freed with DestroyTexture().

SEE ALSO
    CreateTexture(), CreateTextureFromDrawingBoard(), DestroyTexture()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
    struct ZuneTexture *texture = NULL;

    ENTER_FUNCTION("CreateTextureFromData");

    D(bug("ZuneRenderer: CreateTextureFromData(data=%p, width=%d, height=%d, "
          "depth=%d, format=0x%08x, pitch=%d, flags=0x%08x)\n",
          data, width, height, depth, format, pitch, flags));

    texture = CreateTextureFromDataInternal(data, width, height, depth, format, pitch, flags);

    if (texture) {
        ZuneBackend *backend = SelectTextureBackend(texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    EXIT_FUNCTION("CreateTextureFromData");
    return texture;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(struct ZuneTexture *, CreateTextureFromDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(ULONG, flags, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 72, zunerenderer)

/*  FUNCTION
    Creates a new texture from a DrawingBoard's pixel data.

INPUTS
    board - Source DrawingBoard (must not be NULL)
    flags - Texture creation flags (ZUNE_TEXTURE_*)

RESULT
    Pointer to new ZuneTexture structure, or NULL if creation failed.

NOTES
    The DrawingBoard's pixel data is copied into the texture.
    The created texture must be freed with DestroyTexture().

SEE ALSO
    CreateTexture(), CreateTextureFromData(), DestroyTexture()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);

    ENTER_FUNCTION("CreateTextureFromDrawingBoard");

    struct DrawingBoard *board = rp->target_board;

    D(bug("ZuneRenderer: CreateTextureFromDrawingBoard(board=%p, flags=0x%08x)\n", board, flags));

    ULONG pitch;
    APTR pixels = LockDrawingBoardPixelsInternal(rp, &pitch);
    if (!pixels) {
        D(bug("ZuneRenderer: Failed to lock DrawingBoard pixels\n"));
        return NULL;
    }

    /* Convert CyberGraphics pixel format to Zune texture format */
    ULONG zune_format = ConvertCyberGfxFormatToZuneFormat(board->pixel_format);

    struct ZuneTexture *texture = CreateTextureFromDataInternal(pixels, board->width, board->height, board->depth, zune_format, pitch, flags);

    if (texture) {
        ZuneBackend *backend = SelectTextureBackend(texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    UnlockDrawingBoardPixelsInternal(rp);

    D(bug("ZuneRenderer: Texture created from DrawingBoard %s (%p)\n", texture ? "successfully" : "failed", texture));

    EXIT_FUNCTION("CreateTextureFromDrawingBoard");
    return texture;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(struct ZuneTexture *, CreateTextureFromDatatype,

         /*  SYNOPSIS */
         AROS_LHA(APTR, dt_object, A0), AROS_LHA(ULONG, flags, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 73, zunerenderer)

/*  FUNCTION
    Creates a new texture from a datatype object.

INPUTS
    dt_object - Pointer to a datatype object (e.g. picture.datatype instance)
    flags - Texture creation flags (ZUNE_TEXTURE_*)

RESULT
    Pointer to new ZuneTexture structure, or NULL if creation failed.

SEE ALSO
    CreateTexture(), CreateTextureFromData(), DestroyTexture()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
    struct ZuneTexture *texture = CreateTextureFromDatatypeInternal(dt_object, flags);

    if (texture) {
        ZuneBackend *backend = SelectTextureBackend(texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    return texture;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, DestroyTexture,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 74, zunerenderer)

/*  FUNCTION
    Destroys a texture and frees all associated resources.

INPUTS
    texture - Texture to destroy (may be NULL)

RESULT
    None

NOTES
    After calling this function, the texture pointer is no longer valid.
    It is safe to pass NULL to this function.

SEE ALSO
    CreateTexture(), CreateTextureFromData(), CreateTextureFromDrawingBoard()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);

    ENTER_FUNCTION("DestroyTexture");

    D(bug("ZuneRenderer: DestroyTexture(texture=%p)\n", texture));

    if (!texture) {
        D(bug("ZuneRenderer: NULL texture, nothing to destroy\n"));
        return;
    }

    /* Decrement reference count */
    texture->ref_count--;
    if (texture->ref_count > 0) {
        D(bug("ZuneRenderer: Texture still has %d references\n", texture->ref_count));
        return;
    }

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->CleanupTexture) {
        backend->ops->CleanupTexture(texture);
    }

    /* Mark as invalid */
    texture->valid = FALSE;

    /* Unlock pixels if locked */
    if (texture->pixels_locked) {
        UnlockTexturePixelsInternal(texture);
    }

    /* Remove from tracking list */
    RemoveTextureFromList(base, texture);

    /* Free texture data */
    FreeTextureData(texture);

    /* Free the structure */
    FreeVec(texture);

    D(bug("ZuneRenderer: Texture destroyed\n"));

    EXIT_FUNCTION("DestroyTexture");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Texture Data Operations */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH3(BOOL, UpdateTextureData,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0), AROS_LHA(APTR, data, A1), AROS_LHA(struct ZuneRect *, rect, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 76, zunerenderer)

/*  FUNCTION
    Updates a rectangular region of texture data.

INPUTS
    texture - Target texture (must not be NULL)
    data - Source pixel data
    rect - Update region rectangle

RESULT
    TRUE if update succeeded, FALSE otherwise

NOTES
    The update region must be within texture bounds.

SEE ALSO
    LockTexturePixels(), UnlockTexturePixels(), SetTexturePixel()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ULONG row, bytes_per_pixel;
    UBYTE *src_ptr, *dst_ptr;

    ENTER_FUNCTION("UpdateTextureData");

    D(bug("ZuneRenderer: UpdateTextureData(texture=%p, data=%p, rect=%p)\n", texture, data, rect));

    if (!texture || !data || !rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return FALSE;
    }

    if (rect->x + rect->width > texture->width || rect->y + rect->height > texture->height) {
        D(bug("ZuneRenderer: Update region out of bounds\n"));
        return FALSE;
    }

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->UpdateTexture) {
        if (backend->ops->UpdateTexture(texture, data, rect->x, rect->y, rect->width, rect->height)) {
            EXIT_FUNCTION("UpdateTextureData");
            return TRUE;
        }
    }

    if (!texture->pixel_data) {
        D(bug("ZuneRenderer: No pixel_data for CPU update path\n"));
        return FALSE;
    }

    if (texture->pixels_locked) {
        D(bug("ZuneRenderer: Texture pixels are locked\n"));
        return FALSE;
    }

    bytes_per_pixel = GetTextureFormatBPP(texture->format) / 8;
    src_ptr = (UBYTE *)data;
    dst_ptr = (UBYTE *)texture->pixel_data + ((IPTR)rect->y * texture->pitch) + ((IPTR)rect->x * bytes_per_pixel);

    for (row = 0; row < rect->height; row++) {
        CopyMem(src_ptr, dst_ptr, (IPTR)rect->width * bytes_per_pixel);
        src_ptr += (IPTR)rect->width * bytes_per_pixel;
        dst_ptr += texture->pitch;
    }

    D(bug("ZuneRenderer: Texture data updated successfully (CPU fallback)\n"));

    EXIT_FUNCTION("UpdateTextureData");
    return TRUE;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(APTR, LockTexturePixels,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0), AROS_LHA(ULONG *, pitch, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 77, zunerenderer)

/*  FUNCTION
    Locks texture pixels for direct access.

INPUTS
    texture - Texture to lock (must not be NULL)
    pitch - Pointer to store pitch value (may be NULL)

RESULT
    Pointer to pixel data, or NULL if locking failed

NOTES
    Always call UnlockTexturePixels() when finished with direct access.
    Do not call other texture functions while pixels are locked.

SEE ALSO
    UnlockTexturePixels(), UpdateTextureData()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("LockTexturePixels");

    D(bug("ZuneRenderer: LockTexturePixels(texture=%p, pitch=%p)\n", texture, pitch));

    if (!texture) {
        D(bug("ZuneRenderer: Invalid texture\n"));
        return NULL;
    }

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->LockTexturePixels) {
        APTR ptr = backend->ops->LockTexturePixels(texture, pitch);
        if (ptr)
            return ptr;
    }

    if (texture->pixels_locked) {
        D(bug("ZuneRenderer: Texture pixels already locked\n"));
        return NULL;
    }

    if (!texture->pixel_data) {
        D(bug("ZuneRenderer: No pixel data available to lock\n"));
        return NULL;
    }

    texture->pixels_locked = TRUE;

    if (pitch) {
        *pitch = texture->pitch;
    }

    D(bug("ZuneRenderer: Texture pixels locked (pitch=%d)\n", texture->pitch));

    EXIT_FUNCTION("LockTexturePixels");
    return texture->pixel_data;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH1(void, UnlockTexturePixels,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 78, zunerenderer)

/*  FUNCTION
    Unlocks texture pixels previously locked with LockTexturePixels().

INPUTS
    texture - Texture to unlock (must not be NULL)

RESULT
    None

NOTES
    Must be called for every successful LockTexturePixels() call.

SEE ALSO
    LockTexturePixels(), UpdateTextureData()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("UnlockTexturePixels");

    D(bug("ZuneRenderer: UnlockTexturePixels(texture=%p)\n", texture));

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->UnlockTexturePixels) {
        backend->ops->UnlockTexturePixels(texture);
        D(bug("ZuneRenderer: Texture pixels unlocked (backend)\n"));
        EXIT_FUNCTION("UnlockTexturePixels");
        return;
    }

    UnlockTexturePixelsInternal(texture);

    D(bug("ZuneRenderer: Texture pixels unlocked\n"));

    EXIT_FUNCTION("UnlockTexturePixels");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(ULONG, GetTexturePixel,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0), AROS_LHA(struct ZunePoint *, point, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 79, zunerenderer)

/*  FUNCTION
    Gets the color value of a pixel in the texture.

INPUTS
    texture - Source texture (must not be NULL)
    point - Pixel coordinates

RESULT
    Color value in ARGB format, or 0 if coordinates are invalid

NOTES
    Coordinates must be within texture bounds.

SEE ALSO
    SetTexturePixel(), LockTexturePixels()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("GetTexturePixel");

    if (!texture || !point) {
        return 0;
    }

    if (point->x < 0 || point->y < 0 || point->x >= texture->width || point->y >= texture->height) {
        return 0;
    }

    if (texture->pixels_locked) {
        return 0;
    }

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->GetTexturePixel) {
        return backend->ops->GetTexturePixel(texture, point->x, point->y);
    }

    if (!texture->pixel_data) {
        return 0;
    }

    UBYTE *pixel_ptr;
    ULONG color = 0;
    ULONG bytes_per_pixel;

    bytes_per_pixel = GetTextureFormatBPP(texture->format) / 8;
    pixel_ptr = (UBYTE *)texture->pixel_data + ((IPTR)point->y * texture->pitch) + ((IPTR)point->x * bytes_per_pixel);

    switch (texture->format) {
    case ZUNE_TEXTURE_FORMAT_ARGB32:
        color = *(ULONG *)pixel_ptr;
        break;
    case ZUNE_TEXTURE_FORMAT_RGB24:
        color = 0xFF000000 | (pixel_ptr[0] << 16) | (pixel_ptr[1] << 8) | pixel_ptr[2];
        break;
    case ZUNE_TEXTURE_FORMAT_ARGB16: {
        UWORD pixel = *(UWORD *)pixel_ptr;
        UBYTE a = (pixel >> 12) & 0x0F;
        UBYTE r = (pixel >> 8) & 0x0F;
        UBYTE g = (pixel >> 4) & 0x0F;
        UBYTE b = pixel & 0x0F;
        color = (a << 28) | (r << 20) | (g << 12) | (b << 4);
    } break;
    case ZUNE_TEXTURE_FORMAT_RGB16: {
        UWORD pixel = *(UWORD *)pixel_ptr;
        UBYTE r = (pixel >> 11) & 0x1F;
        UBYTE g = (pixel >> 5) & 0x3F;
        UBYTE b = pixel & 0x1F;
        color = 0xFF000000 | (r << 19) | (g << 10) | (b << 3);
    } break;
    }

    EXIT_FUNCTION("GetTexturePixel");
    return color;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, SetTexturePixel,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0), AROS_LHA(struct ZunePoint *, point, A1), AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 80, zunerenderer)

/*  FUNCTION
    Sets the color value of a pixel in the texture.

INPUTS
    texture - Target texture (must not be NULL)
    point - Pixel coordinates
    color - Color value in ARGB format

RESULT
    None

NOTES
    Coordinates must be within texture bounds.

SEE ALSO
    GetTexturePixel(), UpdateTextureData()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("SetTexturePixel");

    if (!texture || !point) {
        return;
    }

    if (point->x < 0 || point->y < 0 || point->x >= texture->width || point->y >= texture->height) {
        return;
    }

    if (texture->pixels_locked) {
        return;
    }

    ZuneBackend *backend = SelectTextureBackend(texture);
    if (backend && backend->ops && backend->ops->SetTexturePixel) {
        backend->ops->SetTexturePixel(texture, point->x, point->y,
                                      &((struct InternalColor){
                                          .a = (color >> 24) & 0xFF,
                                          .r = (color >> 16) & 0xFF,
                                          .g = (color >> 8) & 0xFF,
                                          .b = color & 0xFF,
                                      }));
        EXIT_FUNCTION("SetTexturePixel");
        return;
    }

    if (!texture->pixel_data) {
        return;
    }

    UBYTE *pixel_ptr;
    ULONG bytes_per_pixel;

    bytes_per_pixel = GetTextureFormatBPP(texture->format) / 8;
    pixel_ptr = (UBYTE *)texture->pixel_data + ((IPTR)point->y * texture->pitch) + ((IPTR)point->x * bytes_per_pixel);

    switch (texture->format) {
    case ZUNE_TEXTURE_FORMAT_ARGB32:
        *(ULONG *)pixel_ptr = color;
        break;
    case ZUNE_TEXTURE_FORMAT_RGB24:
        pixel_ptr[0] = (color >> 16) & 0xFF;
        pixel_ptr[1] = (color >> 8) & 0xFF;
        pixel_ptr[2] = color & 0xFF;
        break;
    case ZUNE_TEXTURE_FORMAT_ARGB16: {
        UBYTE a = (color >> 28) & 0x0F;
        UBYTE r = (color >> 20) & 0x0F;
        UBYTE g = (color >> 12) & 0x0F;
        UBYTE b = (color >> 4) & 0x0F;
        *(UWORD *)pixel_ptr = (a << 12) | (r << 8) | (g << 4) | b;
    } break;
    case ZUNE_TEXTURE_FORMAT_RGB16: {
        UBYTE r = (color >> 19) & 0x1F;
        UBYTE g = (color >> 10) & 0x3F;
        UBYTE b = (color >> 3) & 0x1F;
        *(UWORD *)pixel_ptr = (r << 11) | (g << 5) | b;
    } break;
    }

    EXIT_FUNCTION("SetTexturePixel");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Texture Rendering Functions */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneDrawTexture,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZunePoint *, position, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 82, zunerenderer)

/*  FUNCTION
    Draws a texture at the specified position.

INPUTS
    rp - Target RenderPort (must not be NULL)
    texture - Source texture (must not be NULL)
    position - Destination coordinates

RESULT
    None

NOTES
    The texture is drawn at its original size.

SEE ALSO
    ZuneDrawTextureScaled(), ZuneDrawTextureRegion()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTexture");

    D(bug("ZuneRenderer: ZuneDrawTexture(rp=%p, texture=%p, position=%p)\n", rp, texture, position));

    if (!ValidateRenderPort(rp) || !texture || !position) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    UWORD width = texture->width;
    UWORD height = texture->height;
    UWORD x = position->x;
    UWORD y = position->y;

    ZUNE_BACKEND_CALL(rp, DrawTexture, texture, x, y, width, height, 0, 0, width, height, NULL);

    EXIT_FUNCTION("ZuneDrawTexture");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneDrawTextureScaled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZuneRect *, dest_rect, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 83, zunerenderer)

/*  FUNCTION
    Draws a texture scaled to the specified dimensions.

INPUTS
    rp - Target RenderPort (must not be NULL)
    texture - Source texture (must not be NULL)
    dest_rect - Destination rectangle

RESULT
    None

NOTES
    The texture is scaled to fit the specified dimensions.

SEE ALSO
    ZuneDrawTexture(), ZuneDrawTextureRegion()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ZuneRect src_rect;

    ENTER_FUNCTION("ZuneDrawTextureScaled");

    D(bug("ZuneRenderer: ZuneDrawTextureScaled(rp=%p, texture=%p, dest_rect=%p)\n", rp, texture, dest_rect));

    if (!ValidateRenderPort(rp) || !texture || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    ZUNE_BACKEND_CALL(rp, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, 0, 0, texture->width,
                      texture->height, NULL);

    EXIT_FUNCTION("ZuneDrawTextureScaled");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawTextureRegion,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 84, zunerenderer)

/*  FUNCTION
    Draws a region of a texture with scaling.

INPUTS
    rp - Target RenderPort (must not be NULL)
    texture - Source texture (must not be NULL)
    src_rect - Source region rectangle
    dest_rect - Destination rectangle

RESULT
    None

NOTES
    The source region is scaled to fit the destination dimensions.

SEE ALSO
    ZuneDrawTexture(), ZuneDrawTextureScaled()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureRegion");

    D(bug("ZuneRenderer: ZuneDrawTextureRegion(rp=88%p, texture=%p, src_rect=%p, "
          "dest_rect=%p)\n",
          rp, texture, src_rect, dest_rect));

    if (!ValidateRenderPort(rp) || !texture || !src_rect || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    if (src_rect->x + src_rect->width > texture->width || src_rect->y + src_rect->height > texture->height) {
        D(bug("ZuneRenderer: Source region out of bounds\n"));
        return;
    }

    if (dest_rect->width == 0 || dest_rect->height == 0) {
        D(bug("ZuneRenderer: Invalid destination dimensions\n"));
        return;
    }

    ZUNE_BACKEND_CALL(rp, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, src_rect->x, src_rect->y,
                      src_rect->width, src_rect->height, NULL);

    EXIT_FUNCTION("ZuneDrawTextureRegion");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawTextureTinted,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZunePoint *, position, A2),
         AROS_LHA(ULONG, tint_color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 85, zunerenderer)

/*  FUNCTION
    Draws a texture with color tinting at the specified position.

INPUTS
    rp - Target RenderPort (must not be NULL)
    texture - Source texture (must not be NULL)
    position - Destination coordinates
    tint_color - Tint color in ARGB format

RESULT
    None

NOTES
    The texture is drawn at its original size with color tinting applied.

SEE ALSO
    ZuneDrawTextureScaledTinted(), ZuneDrawTextureRegionTinted()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureTinted");

    D(bug("ZuneRenderer: ZuneDrawTextureTinted(rp=%p, texture=%p, position=%p, "
          "tint=0x%08x)\n",
          rp, texture, position, tint_color));

    if (!ValidateRenderPort(rp) || !texture || !position) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    struct InternalColor color = ZuneColorToInternal(rp, tint_color, rp->pixel_format);
    ZUNE_BACKEND_CALL(rp, DrawTexture, texture, position->x, position->y, texture->width, texture->height, 0, 0, texture->width, texture->height,
                      &color);

    EXIT_FUNCTION("ZuneDrawTextureTinted");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH4(void, ZuneDrawTextureScaledTinted,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZuneRect *, dest_rect, A2),
         AROS_LHA(ULONG, tint_color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 86, zunerenderer)

/*  FUNCTION
    Draws a texture scaled with color tinting.

INPUTS
    rp - Target RenderPort (must not be NULL)
    texture - Source texture (must not be NULL)
    dest_rect - Destination rectangle
    tint_color - Tint color in ARGB format

RESULT
    None

NOTES
    The texture is scaled to fit the specified dimensions with tinting.

SEE ALSO
    ZuneDrawTextureTinted(), ZuneDrawTextureRegionTinted()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ZuneRect src_rect;

    ENTER_FUNCTION("ZuneDrawTextureScaledTinted");

    D(bug("ZuneRenderer: ZuneDrawTextureScaledTinted(rp=%p, texture=%p, "
          "dest_rect=%p, tint=0x%08x)\n",
          rp, texture, dest_rect, tint_color));

    if (!ValidateRenderPort(rp) || !texture || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    struct InternalColor color = ZuneColorToInternal(rp, tint_color, rp->pixel_format);
    ZUNE_BACKEND_CALL(rp, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, 0, 0, texture->width,
                      texture->height, &color);

    EXIT_FUNCTION("ZuneDrawTextureScaledTinted");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH5(void, ZuneDrawTextureRegionTinted,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZuneRect *, src_rect, A2),
         AROS_LHA(struct ZuneRect *, dest_rect, A3), AROS_LHA(ULONG, tint_color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 87, zunerenderer)

/*  FUNCTION
    Draws a region of a texture with scaling and color tinting.

INPUTS
    rp - Target RenderPort (must not be NULL)
    texture - Source texture (must not be NULL)
    src_rect - Source region rectangle
    dest_rect - Destination rectangle
    tint_color - Tint color in ARGB format

RESULT
    None

NOTES
    The source region is scaled to fit the destination dimensions with tinting.

SEE ALSO
    ZuneDrawTextureTinted(), ZuneDrawTextureScaledTinted()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);

    ENTER_FUNCTION("ZuneDrawTextureRegionTinted");

    D(bug("ZuneRenderer: ZuneDrawTextureRegionTinted(rp=%p, texture=%p, "
          "src_rect=%p, dest_rect=%p, tint=0x%08x)\n",
          rp, texture, src_rect, dest_rect, tint_color));

    if (!ValidateRenderPort(rp) || !texture || !src_rect || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    if (src_rect->x + src_rect->width > texture->width || src_rect->y + src_rect->height > texture->height) {
        D(bug("ZuneRenderer: Source region out of bounds\n"));
        return;
    }

    if (dest_rect->width == 0 || dest_rect->height == 0) {
        D(bug("ZuneRenderer: Invalid destination dimensions\n"));
        return;
    }

    struct InternalColor color = ZuneColorToInternal(rp, tint_color, rp->pixel_format);
    ZUNE_BACKEND_CALL(rp, DrawTexture, texture, dest_rect->x, dest_rect->y, dest_rect->width, dest_rect->height, src_rect->x, src_rect->y,
                      src_rect->width, src_rect->height, &color);

    EXIT_FUNCTION("ZuneDrawTextureRegionTinted");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH3(void, ZuneDrawTextureTiled,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(struct ZuneTexture *, texture, A1), AROS_LHA(struct ZuneRect *, dest_rect, A2),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 88, zunerenderer)

/*  FUNCTION
    Draws a texture tiled across the specified destination rectangle.
    The texture will be repeated horizontally and vertically to fill
    the entire destination area.

INPUTS
    rp - Pointer to the RenderPort to draw on
    texture - Pointer to the ZuneTexture to tile
    dest_rect - Destination rectangle to fill with tiled texture

RESULT
    None

EXAMPLE

BUGS
    None

SEE ALSO
    ZuneDrawTexture(), ZuneDrawTextureScaled()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureTiled");

    D(bug("ZuneRenderer: ZuneDrawTextureTiled(rp=%p, texture=%p, dest_rect=%p)\n", rp, texture, dest_rect));

    if (!ValidateRenderPort(rp) || !texture || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    if (!ValidateTexture(texture)) {
        D(bug("ZuneRenderer: Invalid texture\n"));
        return;
    }

    UWORD texture_width = texture->width;
    UWORD texture_height = texture->height;

    if (texture_width == 0 || texture_height == 0) {
        D(bug("ZuneRenderer: Texture has zero dimensions\n"));
        return;
    }

    WORD dest_x = dest_rect->x;
    WORD dest_y = dest_rect->y;
    UWORD dest_width = dest_rect->width;
    UWORD dest_height = dest_rect->height;

    /* Calculate how many complete tiles we need */
    UWORD tiles_x = (dest_width + texture_width - 1) / texture_width;
    UWORD tiles_y = (dest_height + texture_height - 1) / texture_height;

    D(bug("ZuneRenderer: Tiling %dx%d texture across %dx%d area (%d x %d tiles)\n", texture_width, texture_height, dest_width, dest_height, tiles_x,
          tiles_y));

    /* Draw tiles row by row */
    UWORD ty, tx;
    for (ty = 0; ty < tiles_y; ty++) {
        for (tx = 0; tx < tiles_x; tx++) {
            WORD tile_x = dest_x + (tx * texture_width);
            WORD tile_y = dest_y + (ty * texture_height);

            /* Calculate the actual size of this tile (may be clipped at edges) */
            UWORD tile_width = texture_width;
            UWORD tile_height = texture_height;

            /* Clip tile dimensions if we're at the edge */
            if (tile_x + tile_width > dest_x + dest_width) {
                tile_width = (dest_x + dest_width) - tile_x;
            }
            if (tile_y + tile_height > dest_y + dest_height) {
                tile_height = (dest_y + dest_height) - tile_y;
            }

            /* Only draw if the tile has positive dimensions */
            if (tile_width > 0 && tile_height > 0) {
                /* Use backend to draw the tile (may be clipped) */
                ZUNE_BACKEND_CALL(rp, DrawTexture, texture, tile_x, tile_y, tile_width, tile_height, 0, 0, tile_width, tile_height, NULL);
            }
        }
    }

    EXIT_FUNCTION("ZuneDrawTextureTiled");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Texture Utility Functions */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH1(BOOL, ZuneIsTextureValid,

         /*  SYNOPSIS */
         AROS_LHA(struct ZuneTexture *, texture, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 90, zunerenderer)

/*  FUNCTION
    Validates that a texture is properly initialized and ready for use.

INPUTS
    texture - Pointer to the ZuneTexture structure to validate

RESULT
    TRUE if the texture is valid and can be used for rendering operations,
    FALSE otherwise.

EXAMPLE

BUGS
    None

SEE ALSO

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneIsTextureValid");

    BOOL result = ValidateTexture(texture);

    EXIT_FUNCTION("ZuneIsTextureValid");
    return result;

    AROS_LIBFUNC_EXIT
}
