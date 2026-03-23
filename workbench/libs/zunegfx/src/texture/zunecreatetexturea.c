/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateTextureA
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <clib/arossupport_protos.h>

#include <proto/datatypes.h>

#define DT_V44_SUPPORT
#include <datatypes/pictureclass.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH1(struct ZuneTexture *, ZuneCreateTextureA,
         AROS_LHA(struct TagItem *, tags, A0),
         struct Library *, ZuneGfxBase, 108, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
    struct ZuneTexture *texture = NULL;
    struct RenderContext *rctx;
    UWORD width, height;
    UBYTE depth;
    ULONG format, flags, pitch;
    APTR data;
    CONST_STRPTR filename;
    APTR dt_object;
    struct Screen *screen;
    BOOL from_board;

    ENTER_FUNCTION("ZuneCreateTextureA");

    if (!tags) {
        D(bug("ZuneRenderer: ZuneCreateTextureA - NULL tags\n"));
        EXIT_FUNCTION("ZuneCreateTextureA");
        return NULL;
    }

    /* Parse common tags */
    rctx       = (struct RenderContext *)LibGetTagData(ZUNE_Texture_RenderContext, 0, tags);
    width      = (UWORD)LibGetTagData(ZUNE_Texture_Width, 0, tags);
    height     = (UWORD)LibGetTagData(ZUNE_Texture_Height, 0, tags);
    depth      = (UBYTE)LibGetTagData(ZUNE_Texture_Depth, 32, tags);
    format     = (ULONG)LibGetTagData(ZUNE_Texture_Format, ZUNE_TEXTURE_FORMAT_ARGB32, tags);
    flags      = (ULONG)LibGetTagData(ZUNE_Texture_Flags, 0, tags);

    /* Parse source-specific tags */
    filename   = (CONST_STRPTR)LibGetTagData(ZUNE_Texture_SourceFile, 0, tags);
    dt_object  = (APTR)LibGetTagData(ZUNE_Texture_SourceDatatype, 0, tags);
    from_board = (BOOL)LibGetTagData(ZUNE_Texture_SourceDrawingBoard, FALSE, tags);
    data       = (APTR)LibGetTagData(ZUNE_Texture_Data, 0, tags);
    pitch      = (ULONG)LibGetTagData(ZUNE_Texture_Pitch, 0, tags);
    screen     = (struct Screen *)LibGetTagData(ZUNE_Texture_Screen, 0, tags);

    D(bug("ZuneRenderer: ZuneCreateTextureA(rctx=%p, %dx%d, depth=%d, fmt=0x%x, flags=0x%x)\n",
          rctx, width, height, depth, format, flags));

    /* Determine source type and create texture */
    if (filename) {
        /* Load from file via DataTypes */
        Object *dt_obj = NULL;
        struct Process *myproc;
        APTR oldwindowptr;

        D(bug("ZuneRenderer: ZuneCreateTextureA - loading from file '%s'\n", filename));

        myproc = (struct Process *)FindTask(NULL);
        oldwindowptr = myproc->pr_WindowPtr;
        myproc->pr_WindowPtr = (APTR)-1;

        dt_obj = NewDTObject((APTR)filename,
            DTA_SourceType,     DTST_FILE,
            DTA_GroupID,        GID_PICTURE,
            PDTA_Remap,         FALSE,
            PDTA_DestMode,      PMODE_V43,
            TAG_DONE);

        myproc->pr_WindowPtr = oldwindowptr;

        if (!dt_obj) {
            D(bug("ZuneRenderer: ZuneCreateTextureA - Failed to load '%s'\n", filename));
            EXIT_FUNCTION("ZuneCreateTextureA");
            return NULL;
        }

        texture = CreateTextureFromDatatypeInternal(dt_obj, flags);
        DisposeDTObject(dt_obj);

    } else if (dt_object) {
        /* Extract from DataTypes object */
        D(bug("ZuneRenderer: ZuneCreateTextureA - from datatype object %p\n", dt_object));
        texture = CreateTextureFromDatatypeInternal(dt_object, flags);

    } else if (from_board && rctx) {
        /* Capture from current DrawingBoard */
        D(bug("ZuneRenderer: ZuneCreateTextureA - from DrawingBoard\n"));
        texture = CreateTextureFromDrawingBoardInternal(base, rctx, flags);
        EXIT_FUNCTION("ZuneCreateTextureA");
        return texture;

    } else if (data) {
        /* Copy from pixel data */
        D(bug("ZuneRenderer: ZuneCreateTextureA - from pixel data %p\n", data));
        if (width == 0 || height == 0 || pitch == 0) {
            D(bug("ZuneRenderer: ZuneCreateTextureA - invalid data params\n"));
            EXIT_FUNCTION("ZuneCreateTextureA");
            return NULL;
        }
        texture = CreateTextureFromDataInternal(data, width, height, depth, format, pitch, flags);

    } else {
        /* Create empty texture */
        D(bug("ZuneRenderer: ZuneCreateTextureA - creating empty texture\n"));
        if (width == 0 || height == 0) {
            D(bug("ZuneRenderer: ZuneCreateTextureA - invalid dimensions\n"));
            EXIT_FUNCTION("ZuneCreateTextureA");
            return NULL;
        }
        texture = AllocateTexture();
        if (texture) {
            InitializeTexture(texture, width, height, depth, format, flags);
            AllocateTextureData(texture);
        }
    }

    /* Initialize backend and add to tracking list */
    if (texture) {
        ZuneBackend *backend = GetTextureBackend(rctx, texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);
    }

    D(bug("ZuneRenderer: ZuneCreateTextureA - result=%p\n", texture));
    EXIT_FUNCTION("ZuneCreateTextureA");
    return texture;

    AROS_LIBFUNC_EXIT
}
