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
#define MUIA_Panel_Style                (TAG_USER | 0x40000001)
#define MUIA_Panel_Spacing              (TAG_USER | 0x40000003)
#define MUIA_Panel_Padding              (TAG_USER | 0x40000004)
#define MUIA_Panel_Title                (TAG_USER | 0x40000007)
#define MUIA_Panel_TitlePosition        (TAG_USER | 0x40000008)
#define MUIA_Panel_TitleTextPosition    (TAG_USER | 0x40000009)
#define MUIA_Panel_TitleVertical        (TAG_USER | 0x4000000A)
#define MUIA_Panel_Collapsible          (TAG_USER | 0x4000000B)
#define MUIA_Panel_Collapsed            (TAG_USER | 0x4000000C)

/* Panel style values */
#define MUIV_Panel_Style_Plain      0
#define MUIV_Panel_Style_Raised     1
#define MUIV_Panel_Style_Recessed   2
#define MUIV_Panel_Style_Groove     3
#define MUIV_Panel_Style_Ridge      4

/* Title position values */
#define MUIV_Panel_Title_None       0
#define MUIV_Panel_Title_Top        1
#define MUIV_Panel_Title_Bottom     2
#define MUIV_Panel_Title_Left       3
#define MUIV_Panel_Title_Right      4

/* Title text position values */
#define MUIV_Panel_Title_Text_Centered 0
#define MUIV_Panel_Title_Text_Left     1
#define MUIV_Panel_Title_Text_Right    2

extern const struct __MUIBuiltinClass _MUI_Panel_desc; /* PRIV */

#endif /* _MUI_CLASSES_PANEL_H */
