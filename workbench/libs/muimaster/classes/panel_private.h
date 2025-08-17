#ifndef _PANEL_PRIVATE_H_
#define _PANEL_PRIVATE_H_

#include <exec/types.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>

/*** Instance data **********************************************************/
struct Panel_DATA
{
    ULONG padding;             /* Internal padding */
    STRPTR title;              /* Optional panel title */
    ULONG title_position;      /* Title position (top, left) */
    ULONG title_text_position; /* Title text position (centered, left/top, right/bottom) */
    BOOL  title_vertical;      /* Render title vertically (left/right positions only) */
    BOOL  collapsible;         /* Allow collapsing by clicking title */
    BOOL  collapsed;           /* Current collapsed state */
    BOOL  show_separator;      /* Draw separator between title and content */

    /* Runtime state */
    BOOL  layout_dirty;     /* Layout needs refresh */
    ULONG expanded_width;   /* Store width when expanded to preserve it when collapsed */
    ULONG expanded_height;  /* Store height when expanded to preserve it when collapsed */

    /* Event handling */
    struct MUI_EventHandlerNode ehn;  /* Event handler for mouse clicks */

    /* Parent layout hook */
    struct Hook *title_clicked_hook;  /* Hook called when parent needs to recalculate layout */
};



#endif /* _PANEL_PRIVATE_H_ */
