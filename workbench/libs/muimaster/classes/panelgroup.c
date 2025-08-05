/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    PanelGroup class implementation - A container for managing collapsible
   panels
*/

#include <clib/alib_protos.h>
#include <exec/memory.h>
#include <intuition/classes.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "debug.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "panel.h"
#include "panelgroup.h"
#include "panelgroup_private.h"
#include "support.h"

/****** PanelGroup.mui/MUIC_PanelGroup ************************************

    NAME
        MUIC_PanelGroup -- Container class for managing collapsible panels

    SUPERCLASS
        MUIC_Group

    DESCRIPTION
        PanelGroup is a specialized Group subclass designed to manage
        multiple Panel objects. It provides functionality for collapsing
        and expanding panels, either individually or in groups.

        Key features:
        - Automatic panel state management
        - Optional single-panel expansion mode
        - Collapse/expand all functionality
        - Panel state notifications
        - Integration with Panel class collapse/expand

    METHODS
        OM_NEW -- Create PanelGroup object
        OM_DISPOSE -- Dispose PanelGroup object
        OM_SET -- Set PanelGroup attributes
        OM_GET -- Get PanelGroup attributes
        MUIM_Group_InitChange -- Begin group changes
        MUIM_Group_ExitChange -- End group changes
        MUIM_PanelGroup_CollapsePanel -- Collapse a specific panel
        MUIM_PanelGroup_ExpandPanel -- Expand a specific panel
        MUIM_PanelGroup_TogglePanel -- Toggle a panel's state
        MUIM_PanelGroup_GetPanelState -- Get panel's current state

    ATTRIBUTES
        MUIA_PanelGroup_AllowMultiple (BOOL)
            Allow multiple panels to be expanded simultaneously.
            Default: TRUE

        MUIA_PanelGroup_Animated (BOOL)
            Use animated transitions for expand/collapse operations.
            Default: FALSE (not yet implemented)

        MUIA_PanelGroup_ExpandedPanel (Object *)
            Get/Set the currently expanded panel (when AllowMultiple is FALSE).
            Default: NULL

        MUIA_PanelGroup_CollapseAll (BOOL)
            Set to TRUE to collapse all panels.
            Write-only trigger attribute.

        MUIA_PanelGroup_ExpandAll (BOOL)
            Set to TRUE to expand all panels.
            Write-only trigger attribute.

***************************************************************************/

#define DEBUG 0

#define MADF_SETUP (1 << 28) /* PRIV - zune-specific */

/* Helper functions */
static struct PanelNode *FindPanelNode(struct PanelGroup_DATA *data,
                                       Object *panel);
static BOOL IsPanelCollapsible(Object *panel);
static BOOL IsPanelCollapsed(Object *panel);
static void SetPanelCollapsed(Object *panel, BOOL collapsed);
static void UpdatePanelStates(struct IClass *cl, Object *obj);

/**************************************************************************
 Helper function to find a panel in our tracking list
**************************************************************************/
static struct PanelNode *FindPanelNode(struct PanelGroup_DATA *data,
                                       Object *panel) {
  struct PanelNode *node;

  ForeachNode(&data->panel_list, node) {
    if (node->panel == panel)
      return node;
  }
  return NULL;
}

/**************************************************************************
 Helper function to check if a panel is collapsible
**************************************************************************/
static BOOL IsPanelCollapsible(Object *panel) {
  BOOL collapsible = FALSE;

  if (panel && (_flags(panel) & MADF_SETUP)) {
    get(panel, MUIA_Panel_Collapsible, &collapsible);
  }

  return collapsible;
}

/**************************************************************************
 Helper function to check if a panel is collapsed
**************************************************************************/
static BOOL IsPanelCollapsed(Object *panel) {
  BOOL collapsed = FALSE;

  if (panel && (_flags(panel) & MADF_SETUP)) {
    get(panel, MUIA_Panel_Collapsed, &collapsed);
  }

  return collapsed;
}

/**************************************************************************
 Helper function to set panel collapsed state
**************************************************************************/
static void SetPanelCollapsed(Object *panel, BOOL collapsed) {
  if (panel && (_flags(panel) & MADF_SETUP)) {
    set(panel, MUIA_Panel_Collapsed, collapsed);
  }
}

/**************************************************************************
 Update panel states in our tracking list
**************************************************************************/
static void UpdatePanelStates(struct IClass *cl, Object *obj) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;

  ForeachNode(&data->panel_list, node) {
    if (node->panel) {
      node->collapsed = IsPanelCollapsed(node->panel);
      node->collapsible = IsPanelCollapsible(node->panel);
    }
  }
}

/**************************************************************************
 OM_NEW method - Create new PanelGroup object
**************************************************************************/
IPTR PanelGroup__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg) {
  struct PanelGroup_DATA *data;
  struct TagItem *tags, *tag;

  D(bug("PanelGroup__OM_NEW: obj=%p\n", obj));

  obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
  if (!obj)
    return 0;

  data = INST_DATA(cl, obj);

  /* Initialize default values */
  data->allow_multiple = TRUE;
  data->animated = FALSE;
  data->expanded_panel = NULL;
  data->panel_count = 0;
  data->layout_dirty = FALSE;
  data->in_collapse_all = FALSE;
  data->in_expand_all = FALSE;

  /* Initialize panel list */
  NewList((struct List *)&data->panel_list);

  /* Initialize event handler */
  data->ehn.ehn_Events = 0;
  data->ehn.ehn_Priority = 0;
  data->ehn.ehn_Flags = 0;
  data->ehn.ehn_Object = obj;
  data->ehn.ehn_Class = cl;

  /* Parse initial taglist */
  for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
    switch (tag->ti_Tag) {
    case MUIA_PanelGroup_AllowMultiple:
      data->allow_multiple = tag->ti_Data ? TRUE : FALSE;
      break;
    case MUIA_PanelGroup_Animated:
      data->animated = tag->ti_Data ? TRUE : FALSE;
      break;
    case MUIA_PanelGroup_ExpandedPanel:
      data->expanded_panel = (Object *)tag->ti_Data;
      if (!data->allow_multiple && data->expanded_panel) {
        /* In single panel mode, collapse all others */
        data->layout_dirty = TRUE;
      }
      break;
    }
  }

  D(bug("PanelGroup__OM_NEW: success, obj=%p\n", obj));
  return (IPTR)obj;
}

/**************************************************************************
 OM_DISPOSE method - Dispose PanelGroup object
**************************************************************************/
IPTR PanelGroup__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node, *next;

  D(bug("PanelGroup__OM_DISPOSE: obj=%p\n", obj));

  /* Free all panel tracking nodes */
  ForeachNodeSafe(&data->panel_list, node, next) {
    Remove((struct Node *)node);
    mui_free(node);
  }

  return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 OM_SET method - Set PanelGroup attributes
**************************************************************************/
IPTR PanelGroup__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct TagItem *tags, *tag;
  BOOL relayout = FALSE;

  for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
    switch (tag->ti_Tag) {
    case MUIA_PanelGroup_AllowMultiple:
      if (data->allow_multiple != (tag->ti_Data ? TRUE : FALSE)) {
        data->allow_multiple = tag->ti_Data ? TRUE : FALSE;
        if (!data->allow_multiple && data->expanded_panel) {
          /* Switch to single panel mode - collapse all except expanded */
          struct PanelNode *node;
          ForeachNode(&data->panel_list, node) {
            if (node->panel != data->expanded_panel && node->collapsible) {
              SetPanelCollapsed(node->panel, TRUE);
            }
          }
          relayout = TRUE;
        }
      }
      break;

    case MUIA_PanelGroup_Animated:
      data->animated = tag->ti_Data ? TRUE : FALSE;
      break;

    case MUIA_PanelGroup_ExpandedPanel:
      if (data->expanded_panel != (Object *)tag->ti_Data) {
        Object *old_panel = data->expanded_panel;
        data->expanded_panel = (Object *)tag->ti_Data;

        if (!data->allow_multiple) {
          /* Collapse old panel if it exists and is collapsible */
          if (old_panel && IsPanelCollapsible(old_panel)) {
            SetPanelCollapsed(old_panel, TRUE);
          }

          /* Expand new panel if it exists and is collapsible */
          if (data->expanded_panel &&
              IsPanelCollapsible(data->expanded_panel)) {
            SetPanelCollapsed(data->expanded_panel, FALSE);
          }
          relayout = TRUE;
        }
      }
      break;

    case MUIA_PanelGroup_CollapseAll:
      if (tag->ti_Data && !data->in_collapse_all) {
        data->in_collapse_all = TRUE;
        DoMethod(obj, MUIM_PanelGroup_CollapsePanel, NULL);
        data->in_collapse_all = FALSE;
        relayout = TRUE;
      }
      break;

    case MUIA_PanelGroup_ExpandAll:
      if (tag->ti_Data && !data->in_expand_all) {
        data->in_expand_all = TRUE;
        DoMethod(obj, MUIM_PanelGroup_ExpandPanel, NULL);
        data->in_expand_all = FALSE;
        relayout = TRUE;
      }
      break;
    }
  }

  if (relayout) {
    data->layout_dirty = TRUE;
    UpdatePanelStates(cl, obj);
    DoMethod(obj, MUIM_Group_InitChange);
    DoMethod(obj, MUIM_Group_ExitChange);
  }

  return DoSuperMethodA(cl, obj, (Msg)msg);
}

/**************************************************************************
 OM_GET method - Get PanelGroup attributes
**************************************************************************/
IPTR PanelGroup__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);

  switch (msg->opg_AttrID) {
  case MUIA_PanelGroup_AllowMultiple:
    *msg->opg_Storage = data->allow_multiple;
    return TRUE;
  case MUIA_PanelGroup_Animated:
    *msg->opg_Storage = data->animated;
    return TRUE;
  case MUIA_PanelGroup_ExpandedPanel:
    *msg->opg_Storage = (IPTR)data->expanded_panel;
    return TRUE;
  }

  return DoSuperMethodA(cl, obj, (Msg)msg);
}

/**************************************************************************
 MUIM_Group_InitChange method - Begin group changes
**************************************************************************/
IPTR PanelGroup__MUIM_Group_InitChange(struct IClass *cl, Object *obj,
                                       Msg msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);

  data->layout_dirty = TRUE;
  return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 MUIM_Group_ExitChange method - End group changes
**************************************************************************/
IPTR PanelGroup__MUIM_Group_ExitChange(struct IClass *cl, Object *obj,
                                       Msg msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  IPTR result;

  result = DoSuperMethodA(cl, obj, msg);

  if (data->layout_dirty) {
    UpdatePanelStates(cl, obj);
    data->layout_dirty = FALSE;

    /* Scan for new/removed panels */
    DoMethod(obj, MUIM_PanelGroup_ScanPanels);
  }

  return result;
}

/**************************************************************************
 MUIM_PanelGroup_ScanPanels method - Scan for Panel children
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_ScanPanels(struct IClass *cl, Object *obj,
                                            Msg msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  Object *child;
  struct List *children;
  struct PanelNode *node, *next;
  BOOL found;

  D(bug("PanelGroup__MUIM_PanelGroup_ScanPanels: obj=%p\n", obj));

  /* Get children list */
  get(obj, MUIA_Group_ChildList, &children);
  if (!children)
    return FALSE;

  /* Mark all existing nodes as not found */
  ForeachNode(&data->panel_list, node) {
    node->panel = (Object *)((IPTR)node->panel | 1); /* Mark with low bit */
  }

  /* Scan children for Panel objects */
  ForeachNode(children, child) {
    if (!child)
      continue;

    /* Check if this is a Panel object by testing for Panel attributes */
    BOOL is_panel = FALSE;
    IPTR test_val;
    if (GetAttr(MUIA_Panel_Collapsible, child, &test_val)) {
      is_panel = TRUE;
    }

    if (is_panel) {
      /* Look for existing node */
      found = FALSE;
      ForeachNode(&data->panel_list, node) {
        if (((IPTR)node->panel & ~1) == (IPTR)child) {
          node->panel = child; /* Clear mark */
          found = TRUE;
          break;
        }
      }

      /* Add new panel if not found */
      if (!found) {
        DoMethod(obj, MUIM_PanelGroup_AddPanel, child);
      }
    }
  }

  /* Remove nodes that weren't found (marked panels) */
  ForeachNodeSafe(&data->panel_list, node, next) {
    if ((IPTR)node->panel & 1) {
      Object *panel = (Object *)((IPTR)node->panel & ~1);
      DoMethod(obj, MUIM_PanelGroup_RemovePanel, panel);
    }
  }

  return TRUE;
}

/**************************************************************************
 MUIM_PanelGroup_AddPanel method - Add a panel to our tracking
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_AddPanel(
    struct IClass *cl, Object *obj, struct MUIP_PanelGroup_AddPanel *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;

  D(bug("PanelGroup__MUIM_PanelGroup_AddPanel: panel=%p\n", msg->panel));

  if (!msg->panel)
    return FALSE;

  /* Check if panel is already tracked */
  if (FindPanelNode(data, msg->panel))
    return TRUE; /* Already tracked */

  /* Create new tracking node */
  node = mui_alloc_struct(struct PanelNode);
  if (!node)
    return FALSE;

  node->panel = msg->panel;
  node->collapsed = IsPanelCollapsed(msg->panel);
  node->collapsible = IsPanelCollapsible(msg->panel);

  AddTail((struct List *)&data->panel_list, (struct Node *)node);
  data->panel_count++;

  D(bug("PanelGroup__MUIM_PanelGroup_AddPanel: added panel, count=%ld\n",
        data->panel_count));

  return TRUE;
}

/**************************************************************************
 MUIM_PanelGroup_RemovePanel method - Remove a panel from our tracking
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_RemovePanel(
    struct IClass *cl, Object *obj, struct MUIP_PanelGroup_RemovePanel *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;

  D(bug("PanelGroup__MUIM_PanelGroup_RemovePanel: panel=%p\n", msg->panel));

  if (!msg->panel)
    return FALSE;

  node = FindPanelNode(data, msg->panel);
  if (!node)
    return FALSE;

  /* If this was the expanded panel, clear it */
  if (data->expanded_panel == msg->panel)
    data->expanded_panel = NULL;

  Remove((struct Node *)node);
  mui_free(node);
  data->panel_count--;

  D(bug("PanelGroup__MUIM_PanelGroup_RemovePanel: removed panel, count=%ld\n",
        data->panel_count));

  return TRUE;
}

/**************************************************************************
 MUIM_PanelGroup_CollapsePanel method - Collapse panel(s)
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_CollapsePanel(
    struct IClass *cl, Object *obj, struct MUIP_PanelGroup_CollapsePanel *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;
  BOOL changed = FALSE;

  D(bug("PanelGroup__MUIM_PanelGroup_CollapsePanel: panel=%p\n", msg->panel));

  if (msg->panel) {
    /* Collapse specific panel */
    node = FindPanelNode(data, msg->panel);
    if (node && node->collapsible && !node->collapsed) {
      SetPanelCollapsed(msg->panel, TRUE);
      node->collapsed = TRUE;

      /* Clear expanded panel if this was it */
      if (data->expanded_panel == msg->panel)
        data->expanded_panel = NULL;

      changed = TRUE;
    }
  } else {
    /* Collapse all panels */
    ForeachNode(&data->panel_list, node) {
      if (node->collapsible && !node->collapsed) {
        SetPanelCollapsed(node->panel, TRUE);
        node->collapsed = TRUE;
        changed = TRUE;
      }
    }
    data->expanded_panel = NULL;
  }

  if (changed) {
    data->layout_dirty = TRUE;
    DoMethod(obj, MUIM_Group_InitChange);
    DoMethod(obj, MUIM_Group_ExitChange);
  }

  return changed;
}

/**************************************************************************
 MUIM_PanelGroup_ExpandPanel method - Expand panel(s)
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_ExpandPanel(
    struct IClass *cl, Object *obj, struct MUIP_PanelGroup_ExpandPanel *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;
  BOOL changed = FALSE;

  D(bug("PanelGroup__MUIM_PanelGroup_ExpandPanel: panel=%p\n", msg->panel));

  if (msg->panel) {
    /* Expand specific panel */
    node = FindPanelNode(data, msg->panel);
    if (node && node->collapsible && node->collapsed) {
      /* If single panel mode, collapse others first */
      if (!data->allow_multiple && data->expanded_panel &&
          data->expanded_panel != msg->panel) {
        DoMethod(obj, MUIM_PanelGroup_CollapsePanel, data->expanded_panel);
      }

      SetPanelCollapsed(msg->panel, FALSE);
      node->collapsed = FALSE;

      if (!data->allow_multiple)
        data->expanded_panel = msg->panel;

      changed = TRUE;
    }
  } else {
    /* Expand all panels (only if multiple allowed) */
    if (data->allow_multiple) {
      ForeachNode(&data->panel_list, node) {
        if (node->collapsible && node->collapsed) {
          SetPanelCollapsed(node->panel, FALSE);
          node->collapsed = FALSE;
          changed = TRUE;
        }
      }
    }
  }

  if (changed) {
    data->layout_dirty = TRUE;
    DoMethod(obj, MUIM_Group_InitChange);
    DoMethod(obj, MUIM_Group_ExitChange);
  }

  return changed;
}

/**************************************************************************
 MUIM_PanelGroup_TogglePanel method - Toggle panel state
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_TogglePanel(
    struct IClass *cl, Object *obj, struct MUIP_PanelGroup_TogglePanel *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;

  D(bug("PanelGroup__MUIM_PanelGroup_TogglePanel: panel=%p\n", msg->panel));

  if (!msg->panel)
    return FALSE;

  node = FindPanelNode(data, msg->panel);
  if (!node || !node->collapsible)
    return FALSE;

  if (node->collapsed) {
    return DoMethod(obj, MUIM_PanelGroup_ExpandPanel, msg->panel);
  } else {
    return DoMethod(obj, MUIM_PanelGroup_CollapsePanel, msg->panel);
  }
}

/**************************************************************************
 MUIM_PanelGroup_GetPanelState method - Get panel collapse state
**************************************************************************/
IPTR PanelGroup__MUIM_PanelGroup_GetPanelState(
    struct IClass *cl, Object *obj, struct MUIP_PanelGroup_GetPanelState *msg) {
  struct PanelGroup_DATA *data = INST_DATA(cl, obj);
  struct PanelNode *node;

  if (!msg->panel)
    return MUIV_PanelGroup_Panel_Expanded;

  node = FindPanelNode(data, msg->panel);
  if (!node)
    return MUIV_PanelGroup_Panel_Expanded;

  return node->collapsed ? MUIV_PanelGroup_Panel_Collapsed
                         : MUIV_PanelGroup_Panel_Expanded;
}

/**************************************************************************
 Main dispatcher
**************************************************************************/
BOOPSI_DISPATCHER(IPTR, PanelGroup_Dispatcher, cl, obj, msg) {
  switch (msg->MethodID) {
  case OM_NEW:
    return PanelGroup__OM_NEW(cl, obj, (struct opSet *)msg);
  case OM_DISPOSE:
    return PanelGroup__OM_DISPOSE(cl, obj, msg);
  case OM_SET:
    return PanelGroup__OM_SET(cl, obj, (struct opSet *)msg);
  case OM_GET:
    return PanelGroup__OM_GET(cl, obj, (struct opGet *)msg);
  case MUIM_Group_InitChange:
    return PanelGroup__MUIM_Group_InitChange(cl, obj, msg);
  case MUIM_Group_ExitChange:
    return PanelGroup__MUIM_Group_ExitChange(cl, obj, msg);
  case MUIM_PanelGroup_AddPanel:
    return PanelGroup__MUIM_PanelGroup_AddPanel(
        cl, obj, (struct MUIP_PanelGroup_AddPanel *)msg);
  case MUIM_PanelGroup_RemovePanel:
    return PanelGroup__MUIM_PanelGroup_RemovePanel(
        cl, obj, (struct MUIP_PanelGroup_RemovePanel *)msg);
  case MUIM_PanelGroup_CollapsePanel:
    return PanelGroup__MUIM_PanelGroup_CollapsePanel(
        cl, obj, (struct MUIP_PanelGroup_CollapsePanel *)msg);
  case MUIM_PanelGroup_ExpandPanel:
    return PanelGroup__MUIM_PanelGroup_ExpandPanel(
        cl, obj, (struct MUIP_PanelGroup_ExpandPanel *)msg);
  case MUIM_PanelGroup_TogglePanel:
    return PanelGroup__MUIM_PanelGroup_TogglePanel(
        cl, obj, (struct MUIP_PanelGroup_TogglePanel *)msg);
  case MUIM_PanelGroup_GetPanelState:
    return PanelGroup__MUIM_PanelGroup_GetPanelState(
        cl, obj, (struct MUIP_PanelGroup_GetPanelState *)msg);
  case MUIM_PanelGroup_ScanPanels:
    return PanelGroup__MUIM_PanelGroup_ScanPanels(cl, obj, msg);
  }

  return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/**************************************************************************
 Class descriptor
**************************************************************************/
const struct __MUIBuiltinClass _MUI_PanelGroup_desc = {
    MUIC_PanelGroup, MUIC_Group, sizeof(struct PanelGroup_DATA),
    (void *)PanelGroup_Dispatcher};
