#ifndef _MUI_CLASSES_PANEL_H
#define _MUI_CLASSES_PANEL_H

/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Panel class public interface
*/

#ifndef LIBRARIES_MUI_H
#include <libraries/mui.h>
#endif

/* Panel class identifier */
#define MUIC_Panel "Panel.mui"

/* Panel attributes */
#define MUIA_Panel_Padding              (TAG_USER | 0x40000004)
#define MUIA_Panel_Title                (TAG_USER | 0x40000007)
#define MUIA_Panel_TitlePosition        (TAG_USER | 0x40000008)
#define MUIA_Panel_TitleTextPosition    (TAG_USER | 0x40000009)
#define MUIA_Panel_TitleVertical        (TAG_USER | 0x4000000A)
#define MUIA_Panel_Collapsible          (TAG_USER | 0x4000000B)
#define MUIA_Panel_Collapsed            (TAG_USER | 0x4000000C)
#define MUIA_Panel_DrawSeparator        (TAG_USER | 0x4000000D)
#define MUIA_Panel_TitleClickedHook     (TAG_USER | 0x4000000E)

/* Title position values */
#define MUIV_Panel_Title_None       0
#define MUIV_Panel_Title_Top        1
#define MUIV_Panel_Title_Left       2

/* Title text position values */
#define MUIV_Panel_Title_Text_Centered 0
#define MUIV_Panel_Title_Text_Left     1
#define MUIV_Panel_Title_Text_Right    2

extern const struct __MUIBuiltinClass _MUI_Panel_desc; /* PRIV */

#endif /* _MUI_CLASSES_PANEL_H */
