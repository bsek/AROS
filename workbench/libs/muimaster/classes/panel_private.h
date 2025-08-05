#ifndef _PANEL_PRIVATE_H_
#define _PANEL_PRIVATE_H_

#include <exec/types.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>

/*** Instance data **********************************************************/
struct Panel_DATA
{
    ULONG padding;             /* Internal padding */
    BOOL  expand_children;     /* Children expand to fill available space */
    STRPTR title;              /* Optional panel title */
    ULONG title_position;      /* Title position (top, bottom, left, right) */
    ULONG title_text_position; /* Title text position (centered, left/top, right/bottom) */
    BOOL  title_vertical;      /* Render title vertically (left/right positions only) */
    BOOL  collapsible;         /* Allow collapsing by clicking title */
    BOOL  collapsed;           /* Current collapsed state */

    /* Runtime state */
    BOOL  layout_dirty;     /* Layout needs refresh */

    /* Event handling */
    struct MUI_EventHandlerNode ehn;  /* Event handler for mouse clicks */
};

#endif /* _PANEL_PRIVATE_H_ */
