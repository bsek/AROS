#ifndef _PANELGROUP_PRIVATE_H_
#define _PANELGROUP_PRIVATE_H_

#include <exec/types.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>

/*** Instance data **********************************************************/
struct PanelGroup_DATA
{
    BOOL  allow_multiple;      /* Allow multiple panels expanded simultaneously */
    BOOL  animated;            /* Use animated expand/collapse transitions */
    Object *expanded_panel;    /* Currently expanded panel (if allow_multiple is FALSE) */

    /* Panel tracking */
    struct MinList panel_list; /* List of managed panels */
    ULONG panel_count;         /* Number of panels in the group */

    /* Runtime state */
    BOOL  layout_dirty;        /* Layout needs refresh */
    BOOL  in_collapse_all;     /* Flag to prevent recursion during collapse all */
    BOOL  in_expand_all;       /* Flag to prevent recursion during expand all */

    /* Event handling */
    struct MUI_EventHandlerNode ehn;  /* Event handler for panel notifications */
};

/* Panel tracking node */
struct PanelNode
{
    struct MinNode node;
    Object *panel;             /* The panel object */
    BOOL   collapsed;          /* Current state of this panel */
    BOOL   collapsible;        /* Whether this panel can be collapsed */
};

/* Internal method IDs */
#define MUIM_PanelGroup_AddPanel        (TAG_USER | 0x41000201)
#define MUIM_PanelGroup_RemovePanel     (TAG_USER | 0x41000202)
#define MUIM_PanelGroup_UpdatePanel     (TAG_USER | 0x41000203)
#define MUIM_PanelGroup_NotifyChange    (TAG_USER | 0x41000204)

/* Internal method parameter structures */
struct MUIP_PanelGroup_AddPanel {
    STACKED ULONG MethodID;
    STACKED Object *panel;
};

struct MUIP_PanelGroup_RemovePanel {
    STACKED ULONG MethodID;
    STACKED Object *panel;
};

struct MUIP_PanelGroup_UpdatePanel {
    STACKED ULONG MethodID;
    STACKED Object *panel;
    STACKED BOOL collapsed;
};

struct MUIP_PanelGroup_NotifyChange {
    STACKED ULONG MethodID;
    STACKED Object *panel;
    STACKED ULONG state;
};

#endif /* _PANELGROUP_PRIVATE_H_ */
