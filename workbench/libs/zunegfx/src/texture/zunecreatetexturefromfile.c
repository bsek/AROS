/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneCreateTextureFromFile
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/datatypes.h>

#define DT_V44_SUPPORT
#include <datatypes/pictureclass.h>

#include "../zunegfx_intern.h"
#include "texture_intern.h"

AROS_LH4(struct ZuneTexture *, ZuneCreateTextureFromFile,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(CONST_STRPTR, filename, A1),
         AROS_LHA(struct Screen *, screen, A2),
         AROS_LHA(ULONG, flags, D0),
         struct Library *, ZuneGfxBase, 74, zunegfx)
{
    AROS_LIBFUNC_INIT

    struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
    struct ZuneTexture *texture = NULL;
    Object *dt_obj = NULL;
    struct Process *myproc;
    APTR oldwindowptr;

    ENTER_FUNCTION("ZuneCreateTextureFromFile");

    if (!filename) {
        D(bug("ZuneGfx: ZuneCreateTextureFromFile - NULL filename\n"));
        return NULL;
    }

    /* Suppress DOS requesters during loading */
    myproc = (struct Process *)FindTask(NULL);
    oldwindowptr = myproc->pr_WindowPtr;
    myproc->pr_WindowPtr = (APTR)-1;

    /* Load the image via DataTypes - match working legacy code from datatypescache.c */
    dt_obj = NewDTObject((APTR)filename,
        DTA_SourceType,     DTST_FILE,
        DTA_GroupID,        GID_PICTURE,
        PDTA_Remap,         FALSE,          /* We want raw pixels */
        PDTA_DestMode,      PMODE_V43,
        TAG_DONE);

    /* Restore window pointer */
    myproc->pr_WindowPtr = oldwindowptr;

    if (!dt_obj) {
        D(bug("ZuneGfx: ZuneCreateTextureFromFile - Failed to load '%s'\n", filename));
        return NULL;
    }

    /* Create texture from the DataTypes object */
    texture = CreateTextureFromDatatypeInternal(dt_obj, flags);

    /* Immediately dispose of the DataTypes object - we have the pixels now */
    DisposeDTObject(dt_obj);
    dt_obj = NULL;

    if (texture) {
        /* Initialize backend if available */
        ZuneBackend *backend = GetTextureBackend(rctx, texture);
        if (backend && backend->ops && backend->ops->InitTexture) {
            if (backend->ops->InitTexture(texture)) {
                texture->backend_type = backend->ops->type;
            }
        }
        AddTextureToList(base, texture);

        D(bug("ZuneGfx: ZuneCreateTextureFromFile - Success: %dx%d texture from '%s'\n",
              texture->width, texture->height, filename));
    } else {
        D(bug("ZuneGfx: ZuneCreateTextureFromFile - Failed to create texture from '%s'\n",
              filename));
    }

    EXIT_FUNCTION("ZuneCreateTextureFromFile");
    return texture;

    AROS_LIBFUNC_EXIT
}
