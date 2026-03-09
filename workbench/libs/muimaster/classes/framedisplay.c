/*
    Copyright  2002-2003, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <stdio.h>

#include <clib/alib_protos.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include <string.h>

#include "datatypescache.h"
#include "debug.h"
#include "frame.h"
#include "framedisplay_private.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "support_classes.h"

#define DEBUG 0
#include <aros/debug.h>

extern struct Library *MUIMasterBase;

IPTR Framedisplay__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Framedisplay_DATA *data;
    struct TagItem *tags;
    struct TagItem *tag;

    D(bug("Framedisplay_New starts\n"));

    obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);

    /* Make sure we start from a defined frame specification. Otherwise we end up
     * using random stack data which may index past the builtin frame table and
     * smear garbage (as seen with the clipboard popframe in Zune prefs when
     * double buffering changes the memory layout). A simple rectangular frame is
     * a safe default until a real spec gets assigned. */
    memset(&data->fs_intern, 0, sizeof(data->fs_intern));
    data->fs_intern.type = FST_RECT;
    data->spec[0] = '\0';

    /* parse initial taglist */

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
        switch (tag->ti_Tag) {
        case MUIA_Framedisplay_Spec:
            zune_frame_spec_to_intern((CONST_STRPTR)tag->ti_Data, &data->fs_intern);
            break;
        }
    }

    D(bug("Framedisplay_New(%lx) spec=%lx\n", obj, data->fs_intern));
    return (IPTR)obj;
}

IPTR Framedisplay__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Framedisplay_DATA *data = INST_DATA(cl, obj);
    struct TagItem *tags;
    struct TagItem *tag;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
        switch (tag->ti_Tag) {
        case MUIA_Framedisplay_Spec:
            zune_frame_spec_to_intern((CONST_STRPTR)tag->ti_Data, &data->fs_intern);
            MUI_Redraw(obj, MADF_DRAWOBJECT);
            break;
        }
    }

    return (IPTR)DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Framedisplay__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct Framedisplay_DATA *data = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case MUIA_Framedisplay_Spec:
        zune_frame_intern_to_spec(&data->fs_intern, (STRPTR)data->spec);
        *msg->opg_Storage = (IPTR)data->spec;
        return (TRUE);
    }

    return (IPTR)DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Framedisplay__MUIM_AskMinMax(struct IClass *cl, Object *obj,
                                  struct MUIP_AskMinMax *msg)
{
    DoSuperMethodA(cl, obj, (Msg)msg);

    msg->MinMaxInfo->MinWidth += 8;
    msg->MinMaxInfo->MinHeight += 8;

    msg->MinMaxInfo->DefWidth += 16;
    msg->MinMaxInfo->DefHeight += 16;

    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;

    return 1;
}

IPTR Framedisplay__MUIM_Draw(struct IClass *cl, Object *obj,
                             struct MUIP_Draw *msg)
{
    struct Framedisplay_DATA *data = INST_DATA(cl, obj);
    const struct ZuneFrameGfx *zframe;
    APTR region;
    WORD ileft, itop, iright, ibottom;
    WORD innerWidth, innerHeight, maxOffset;
    WORD startX, startY, endX, endY, delta;
    int i;

    D(bug("Framedisplay__MUIM_Draw(%p): type=%d state=%d flags=0x%lx\n",
          obj, data->fs_intern.type, data->fs_intern.state, msg->flags));

    DoSuperMethodA(cl, obj, (Msg)msg);

    if (!(msg->flags & MADF_DRAWOBJECT))
        return 0;

    zframe = zune_zframe_get(obj, &data->fs_intern);
    D(bug("Framedisplay__MUIM_Draw(%p): zframe=%p draw=%p\n", obj, zframe, zframe ? zframe->draw : NULL));
    if (!zframe)
        return 0;

    struct dt_frame_image temp_frame;
    struct MUI_FrameSpec_intern tempframe;
    struct dt_frame_image *frame_img =
        zune_frame_prepare_for_drawing(zframe, &data->fs_intern, &temp_frame);

    zframe->draw(frame_img, muiRenderInfo(obj), _left(obj), _top(obj),
                 _width(obj), _height(obj), _left(obj), _top(obj), _width(obj),
                 _height(obj));

    ileft = _mleft(obj) + zframe->ileft + data->fs_intern.innerLeft;
    itop = _mtop(obj) + zframe->itop + data->fs_intern.innerTop;
    iright = _mright(obj) - zframe->iright - data->fs_intern.innerRight;
    ibottom = _mbottom(obj) - zframe->ibottom - data->fs_intern.innerBottom;

    SetAPen(_rp(obj), _pens(obj)[MPEN_SHADOW]);

    region = MUI_AddClipping(muiRenderInfo(obj), ileft, itop, iright - ileft + 1,
                             ibottom - itop + 1);

    innerWidth = iright - ileft;
    innerHeight = ibottom - itop;

    if (innerWidth >= 0 && innerHeight >= 0) {
        maxOffset = innerWidth + innerHeight;

        for (i = 0; i <= maxOffset; i += 4) {
            startX = ileft;
            startY = itop + i;

            if (startY > ibottom) {
                delta = startY - ibottom;
                startY = ibottom;
                startX += delta;
                if (startX > iright)
                    startX = iright;
            }

            endX = ileft + i;
            endY = itop;

            if (endX > iright) {
                delta = endX - iright;
                endX = iright;
                endY += delta;
                if (endY > ibottom)
                    endY = ibottom;
            }

            Move(_rp(obj), startX, startY);
            Draw(_rp(obj), endX, endY);
        }
    }

    MUI_RemoveClipping(muiRenderInfo(obj), region);

    return 1;
}

IPTR Framedisplay__MUIM_DragQuery(struct IClass *cl, Object *obj,
                                  struct MUIP_DragQuery *msg)
{
    struct MUI_FrameSpec *dummy = NULL;

    if (msg->obj == obj)
        return MUIV_DragQuery_Refuse;
    if (!get(msg->obj, MUIA_Framedisplay_Spec, &dummy))
        return MUIV_DragQuery_Refuse;
    return MUIV_DragQuery_Accept;
}

IPTR Framedisplay__MUIM_DragDrop(struct IClass *cl, Object *obj,
                                 struct MUIP_DragDrop *msg)
{
    struct MUI_FrameSpec *spec = NULL;

    get(msg->obj, MUIA_Framedisplay_Spec, &spec);
    set(obj, MUIA_Framedisplay_Spec, (IPTR)spec);
    return 0;
}

#if ZUNE_BUILTIN_FRAMEDISPLAY
BOOPSI_DISPATCHER(IPTR, Framedisplay_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID) {
    case OM_NEW:
        return Framedisplay__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_SET:
        return Framedisplay__OM_SET(cl, obj, (APTR)msg);
    case OM_GET:
        return Framedisplay__OM_GET(cl, obj, (APTR)msg);
    case MUIM_AskMinMax:
        return Framedisplay__MUIM_AskMinMax(cl, obj, (APTR)msg);
    case MUIM_Draw:
        return Framedisplay__MUIM_Draw(cl, obj, (APTR)msg);
    case MUIM_DragQuery:
        return Framedisplay__MUIM_DragQuery(cl, obj, (APTR)msg);
    case MUIM_DragDrop:
        return Framedisplay__MUIM_DragDrop(cl, obj, (APTR)msg);
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Framedisplay_desc = {
    MUIC_Framedisplay, MUIC_Area, sizeof(struct Framedisplay_DATA),
    (void *)Framedisplay_Dispatcher
};
#endif /* ZUNE_BUILTIN_FRAMEDISPLAY */
