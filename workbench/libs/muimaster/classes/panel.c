/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Panel class - A flexible container widget for organizing UI elements
*/

#include "clib/intuition_protos.h"
#include "clib/muimaster_protos.h"
#include "graphics/rastport.h"
#include "intuition/intuition.h"
#include "libraries/mui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "muimaster_intern.h"

#include "classes/window.h"
#include "mui.h"
#include "panel.h"
#include "panel_private.h"
#include "panel_title.h"
#include "panelgroup.h"
#include "prefs.h"
#include "support.h"
#include "textengine.h"

#define DEBUG 1
#include <aros/debug.h>

#ifndef MADF_SHOWME
#define MADF_SHOWME (1 << 9) /* PRIV - mui verified */
#endif

extern struct Library *MUIMasterBase;

/** Helper functions *******************************************************/

/**
 * Creates a rounded rectangle clipping region
 * This is a simplified version for demonstration purposes
 */
static struct Region *CreateRoundedRegion(int left, int top, int width,
                                          int height, int frame_radius,
                                          int frame_width) {
  struct Region *region = NewRegion();
  if (!region)
    return NULL;

  /* Clamp radius to reasonable bounds */
  if (frame_radius <= 0 || frame_radius > width / 2 ||
      frame_radius > height / 2) {
    frame_radius = 0;
  }

  if (frame_radius == 0) {
    /* Rectangular region */
    struct Rectangle rect = {.MinX = left,
                             .MinY = top,
                             .MaxX = left + width - 1,
                             .MaxY = top + height - 1};
    OrRectRegion(region, &rect);
    return region;
  }

  /* Create rounded rectangle by adding rectangular sections and corners */
  struct Rectangle rect;

  /* Center rectangle (full height, reduced width) */
  rect.MinX = left + frame_radius;
  rect.MinY = top;
  rect.MaxX = left + width - frame_radius - 1;
  rect.MaxY = top + height - 1;
  OrRectRegion(region, &rect);

  /* Left and right rectangles (reduced height) */
  rect.MinX = left;
  rect.MinY = top + frame_radius;
  rect.MaxX = left + frame_radius - 1;
  rect.MaxY = top + height - frame_radius - 1;
  OrRectRegion(region, &rect);

  rect.MinX = left + width - frame_radius;
  rect.MinY = top + frame_radius;
  rect.MaxX = left + width - 1;
  rect.MaxY = top + height - frame_radius - 1;
  OrRectRegion(region, &rect);

  /* Add rounded corners using simple circle approximation */
  int corners[4][2] = {
      {left + frame_radius, top + frame_radius},              /* top-left */
      {left + width - frame_radius - 1, top + frame_radius},  /* top-right */
      {left + frame_radius, top + height - frame_radius - 1}, /* bottom-left */
      {left + width - frame_radius - 1,
       top + height - frame_radius - 1}}; /* bottom-right */

  for (int corner = 0; corner < 4; corner++) {
    int cx = corners[corner][0];
    int cy = corners[corner][1];

    for (int dy = -frame_radius; dy <= frame_radius; dy++) {
      for (int dx = -frame_radius; dx <= frame_radius; dx++) {
        if (dx * dx + dy * dy <= frame_radius * frame_radius) {
          rect.MinX = cx + dx;
          rect.MinY = cy + dy;
          rect.MaxX = cx + dx;
          rect.MaxY = cy + dy;
          OrRectRegion(region, &rect);
        }
      }
    }
  }

  return region;
}

/*** Methods ****************************************************************/

IPTR Panel__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg) {
  struct Panel_DATA *data;
  struct TagItem *tags, *tag;

  obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
  if (!obj)
    return 0;

  data = INST_DATA(cl, obj);

  data->title = NULL;
  data->title_position = MUIV_Panel_Title_None;
  data->title_vertical = FALSE;
  data->layout_dirty = TRUE;
  data->expanded_width = 0;
  data->expanded_height = 0;

  /* Parse initial taglist */
  for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
    switch (tag->ti_Tag) {
    case MUIA_Panel_Padding:
      data->padding = tag->ti_Data;
      break;
    case MUIA_Panel_Title:
      data->title = StrDup((STRPTR)tag->ti_Data);
      break;
    case MUIA_Panel_TitleTextPosition:
      data->title_text_position = tag->ti_Data;
      break;
    case MUIA_Panel_TitlePosition:
      data->title_position = tag->ti_Data;
      break;
    case MUIA_Panel_TitleVertical:
      data->title_vertical = tag->ti_Data;
      break;
    case MUIA_Panel_Collapsible:
      data->collapsible = tag->ti_Data;
      break;
    case MUIA_Panel_Collapsed:
      data->collapsed = tag->ti_Data;
      break;
    case MUIA_Panel_TitleClickedHook:
      data->title_clicked_hook = (struct Hook *)tag->ti_Data;
      break;
    }
  }

  return (IPTR)obj;
}

IPTR Panel__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  if (data->title)
    mui_free(data->title);

  return DoSuperMethodA(cl, obj, msg);
}

IPTR Panel__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  struct TagItem *tags, *tag;
  BOOL redraw = FALSE;

  for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
    switch (tag->ti_Tag) {

    case MUIA_Panel_Padding:
      if (data->padding != tag->ti_Data) {
        data->padding = tag->ti_Data;
        redraw = TRUE;
      }
      break;
    case MUIA_Panel_Title:
      if (data->title)
        mui_free(data->title);
      data->title = StrDup((STRPTR)tag->ti_Data);
      redraw = TRUE;
      break;
    case MUIA_Panel_TitlePosition:
      if (data->title_position != tag->ti_Data) {
        data->title_position = tag->ti_Data;
        redraw = TRUE;
      }
      break;
    case MUIA_Panel_TitleTextPosition:
      if (data->title_text_position != tag->ti_Data) {
        data->title_text_position = tag->ti_Data;
        redraw = TRUE;
      }
      break;
    case MUIA_Panel_TitleVertical:
      if (data->title_vertical != tag->ti_Data) {
        data->title_vertical = tag->ti_Data;
        redraw = TRUE;
      }
      break;
    case MUIA_Panel_Collapsible:
      if (data->collapsible != tag->ti_Data) {
        data->collapsible = tag->ti_Data;
      }
      break;

    case MUIA_Panel_Collapsed:
      if (data->collapsed != tag->ti_Data) {
        BOOL was_expanded = !data->collapsed;
        BOOL will_be_collapsed = tag->ti_Data;

        /* If transitioning from expanded to collapsed, store current dimensions
         */
        if (was_expanded && will_be_collapsed) {
          ULONG current_width = _width(obj);
          ULONG current_height = _height(obj);
          if (current_width > 0 && current_height > 0) {
            /* Only update if we haven't stored valid dimensions yet, or
             * if current dimensions are significantly different (user resized)
             */
            if (data->expanded_width == 0 || data->expanded_height == 0 ||
                abs((int)current_width - (int)data->expanded_width) > 10 ||
                abs((int)current_height - (int)data->expanded_height) > 10) {
              data->expanded_width = current_width;
              data->expanded_height = current_height;
              D(bug("Panel: Storing expanded dimensions %dx%d before "
                    "collapsing\n",
                    current_width, current_height));
            }
          } else if (data->expanded_width == 0 || data->expanded_height == 0) {
            /* Panel hasn't been laid out yet, use reasonable defaults */
            /* Don't actually collapse yet - wait for first layout */
            data->collapsed = FALSE;
            return DoSuperMethodA(cl, obj, (Msg)msg);
          }
        }

        /* If transitioning from collapsed to expanded, we already have stored
         * dimensions */
        if (!was_expanded && !will_be_collapsed) {
          D(bug("Panel: Expanding to stored dimensions %dx%d\n",
                data->expanded_width, data->expanded_height));
        }

        data->collapsed = tag->ti_Data;
        redraw = TRUE;

        /* Hide/show children based on collapsed state */
        struct MinList *children = NULL;
        get(obj, MUIA_Group_ChildList, &children);
        if (children) {
          Object *child;
          APTR cstate = children->mlh_Head;
          while ((child = NextObject(&cstate))) {
            set(child, MUIA_ShowMe, !data->collapsed);
          }
        }

        /* Trigger relayout */
        DoMethod(obj, MUIM_Group_InitChange);
        DoMethod(obj, MUIM_Group_ExitChange);

        /* Call title clicked hook if set */
        if (data->title_clicked_hook) {
          CallHookPkt(data->title_clicked_hook, obj, NULL);
        }
      }
      break;
    }
  }

  if (redraw) {
    MUI_Redraw(obj, MADF_DRAWOBJECT);
  }

  return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Panel__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  switch (msg->opg_AttrID) {
  case MUIA_Panel_Padding:
    *msg->opg_Storage = data->padding;
    return TRUE;
  case MUIA_Panel_Title:
    *msg->opg_Storage = (IPTR)data->title;
    return TRUE;
  case MUIA_Panel_TitlePosition:
    *msg->opg_Storage = data->title_position;
    return TRUE;
  case MUIA_Panel_TitleTextPosition:
    *msg->opg_Storage = data->title_text_position;
    return TRUE;
  case MUIA_Panel_TitleVertical:
    *msg->opg_Storage = data->title_vertical;
    return TRUE;
  case MUIA_Panel_Collapsible:
    *msg->opg_Storage = data->collapsible;
    return TRUE;
  case MUIA_Panel_Collapsed:
    *msg->opg_Storage = data->collapsed;
    return TRUE;
  }

  return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Panel__MUIM_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  IPTR result;

  /* Add clipping if frame has border radius */
  APTR frame_clip = (APTR)-1;
  struct MUI_FrameClipInfo clipinfo;
  if (DoMethod(obj, MUIM_QueryFrameClipping, &clipinfo)) {
    if (clipinfo.has_rounded_corners) {
      struct Region *clipregion =
          CreateRoundedRegion(_left(obj), _top(obj), _width(obj), _height(obj),
                              clipinfo.border_radius, clipinfo.frame_width);

      frame_clip = MUI_AddClipRegion(muiRenderInfo(obj), clipregion);
    }
  }

  /* Let superclass handle frame, background, and children drawing */
  result = DoSuperMethodA(cl, obj, (Msg)msg);

  /* Draw title if present and we're drawing the object */
  if ((msg->flags & MADF_DRAWOBJECT) && data->title &&
      data->title_position != MUIV_Panel_Title_None) {
    Panel_DrawTitle(cl, obj, data);
  }

  if (frame_clip != (APTR)-1) {
    MUI_RemoveClipRegion(muiRenderInfo(obj), frame_clip);
  }
  return result;
}

IPTR Panel__MUIM_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  if (!DoSuperMethodA(cl, obj, (Msg)msg))
    return FALSE;

  /* Set initial child visibility based on collapsed state */
  if (data->collapsed) {
    struct MinList *children = NULL;
    get(obj, MUIA_Group_ChildList, &children);
    if (children) {
      Object *child;
      APTR cstate = children->mlh_Head;
      while ((child = NextObject(&cstate))) {
        set(child, MUIA_ShowMe, FALSE);
      }
    }
  }

  /* Set up event handler for mouse clicks */
  data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
  data->ehn.ehn_Priority = 0;
  data->ehn.ehn_Flags = 0;
  data->ehn.ehn_Object = obj;
  data->ehn.ehn_Class = cl;

  DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR)&data->ehn);

  return TRUE;
}

IPTR Panel__MUIM_Cleanup(struct IClass *cl, Object *obj,
                         struct MUIP_Cleanup *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  if (data->ehn.ehn_Object != NULL) {
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehn);
    data->ehn.ehn_Object = NULL;
  }

  return DoSuperMethodA(cl, obj, (Msg)msg);
}

/**************************************************************************
 MUIM_Show - Called when object becomes visible
 This is an ideal place to capture actual displayed size after layout
**************************************************************************/
IPTR Panel__MUIM_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  IPTR result;

  /* Call superclass first */
  result = DoSuperMethodA(cl, obj, (Msg)msg);

  return result;
}

IPTR Panel__MUIM_HandleEvent(struct IClass *cl, Object *obj,
                             struct MUIP_HandleEvent *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  IPTR retval = 0;

  if (msg->imsg && msg->imsg->Class == IDCMP_MOUSEBUTTONS) {
    UWORD code = msg->imsg->Code;
    WORD x = msg->imsg->MouseX;
    WORD y = msg->imsg->MouseY;

    if (code == SELECTDOWN) {
      /* Check if click is within our object bounds */
      if (_isinobject(obj, x, y)) {
        retval = Panel_HandleTitleClick(cl, obj, x, y);
        if (retval)
          return retval;
      }
    }
  }

  /* Pass unhandled events to superclass */
  return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Panel__MUIM_Group_InitChange(struct IClass *cl, Object *obj, Msg msg) {
  return DoSuperMethodA(cl, obj, msg);
}

IPTR Panel__MUIM_Group_ExitChange(struct IClass *cl, Object *obj, Msg msg) {
  return DoSuperMethodA(cl, obj, msg);
}

IPTR Panel__MUIM_AskMinMax(struct IClass *cl, Object *obj,
                           struct MUIP_AskMinMax *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  struct MUI_MinMax *mi;
  UWORD title_width = 0, title_height = 0;

  // Calculate title dimensions if present
  if (data->title && data->title_position != MUIV_Panel_Title_None) {
    BOOL use_vertical = FALSE;
    if (data->title_vertical && data->title_position == MUIV_Panel_Title_Left)
      use_vertical = TRUE;

    Panel_CalculateTextSize(use_vertical, obj, data->title, &title_width,
                            &title_height);
  }

  if (!data->collapsed) {
    // Let the Group superclass calculate the children's requirements first
    DoSuperMethodA(cl, obj, (Msg)msg);
    mi = msg->MinMaxInfo;

    // Add padding to the Group's calculated sizes
    mi->MinWidth += data->padding * 2;
    mi->MinHeight += data->padding * 2;
    mi->DefWidth += data->padding * 2;
    mi->DefHeight += data->padding * 2;
    if (mi->MaxWidth != MUI_MAXMAX)
      mi->MaxWidth += data->padding * 2;
    if (mi->MaxHeight != MUI_MAXMAX)
      mi->MaxHeight += data->padding * 2;

    // Add title space to the Group's calculated sizes
    if (data->title && data->title_position != MUIV_Panel_Title_None) {
      switch (data->title_position) {
      case MUIV_Panel_Title_Top:
        mi->MinHeight += title_height + (TITLE_TEXT_PADDING * 2);
        mi->DefHeight += title_height + (TITLE_TEXT_PADDING * 2);
        if (mi->MaxHeight != MUI_MAXMAX)
          mi->MaxHeight += title_height + (TITLE_TEXT_PADDING * 2);
        mi->MinWidth =
            MAX(mi->MinWidth,
                title_width + (TITLE_TEXT_PADDING * 2) + data->padding * 2);
        mi->DefWidth =
            MAX(mi->DefWidth,
                title_width + (TITLE_TEXT_PADDING * 2) + data->padding * 2);
        break;
      case MUIV_Panel_Title_Left:
        mi->MinWidth +=
            title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
        mi->DefWidth +=
            title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
        if (mi->MaxWidth != MUI_MAXMAX)
          mi->MaxWidth +=
              title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
        mi->MinHeight =
            MAX(mi->MinHeight,
                title_height + (TITLE_TEXT_PADDING * 2) + data->padding * 2);
        mi->DefHeight =
            MAX(mi->DefHeight,
                title_height + (TITLE_TEXT_PADDING * 2) + data->padding * 2);
        break;
      }
    }
  } else {
    // For collapsed panels, calculate minimum size for just the title
    mi = msg->MinMaxInfo;

    // Start with title-only dimensions
    ULONG collapsed_width = data->padding * 2;
    ULONG collapsed_height = data->padding * 2;

    if (data->title && data->title_position != MUIV_Panel_Title_None) {
      switch (data->title_position) {
      case MUIV_Panel_Title_Top:
        // Vertical collapse: minimize height to title only
        collapsed_height += title_height;
        // For vertical collapse, preserve full width by getting it from
        // superclass
        if (data->expanded_width > 0) {
          collapsed_width = data->expanded_width;
        } else {
          // Get proper width from superclass calculation
          DoSuperMethodA(cl, obj, (Msg)msg);
          collapsed_width = msg->MinMaxInfo->DefWidth + data->padding * 2;
          if (data->title &&
              title_width > collapsed_width - data->padding * 2) {
            collapsed_width =
                title_width + (TITLE_TEXT_PADDING * 2) + data->padding * 2;
          }
        }
        break;
      case MUIV_Panel_Title_Left:
        // Horizontal collapse: minimize width to title only
        collapsed_width += title_width + (TITLE_TEXT_PADDING * 2);
        // Preserve original height when collapsing horizontally
        collapsed_height =
            data->expanded_height > 0
                ? data->expanded_height
                : title_height + (TITLE_TEXT_PADDING * 2) + data->padding * 2;
        break;
      }
    }

    mi->MinWidth = collapsed_width;
    mi->MinHeight = collapsed_height;
    mi->DefWidth = collapsed_width;
    mi->DefHeight = collapsed_height;
    mi->MaxWidth = MUI_MAXMAX;
    mi->MaxHeight = collapsed_height;
  }

  return 0;
}

IPTR Panel__MUIM_Layout(struct IClass *cl, Object *obj,
                        struct MUIP_Layout *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  // Calculate title space
  WORD title_space_x = 0;
  WORD title_space_y = 0;

  if (data->title && data->title_position != MUIV_Panel_Title_None) {
    WORD title_width = 0, title_height = 0;
    BOOL use_vertical =
        data->title_vertical && data->title_position == MUIV_Panel_Title_Left;

    Panel_CalculateTextSize(use_vertical, obj, data->title, &title_width,
                            &title_height);

    switch (data->title_position) {
    case MUIV_Panel_Title_Top:
      title_space_y = title_height + (TITLE_TEXT_PADDING * 2);
      break;
    case MUIV_Panel_Title_Left:
      title_space_x =
          title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
      break;
    }
  }

  // Store original object bounds
  WORD orig_left = _left(obj);
  WORD orig_top = _top(obj);
  WORD orig_width = _width(obj);
  WORD orig_height = _height(obj);

  // Temporarily shrink the object's apparent size for Group layout
  // This tricks Group into laying out children in the smaller content area
  _left(obj) = orig_left + data->padding + title_space_x;
  _top(obj) = orig_top + data->padding + title_space_y;
  _width(obj) = orig_width - (data->padding * 2) - title_space_x;
  _height(obj) = orig_height - (data->padding * 2) - title_space_y;

  // Ensure non-negative dimensions
  if (_width(obj) < 0)
    _width(obj) = 0;
  if (_height(obj) < 0)
    _height(obj) = 0;

  // Let Group handle the actual layout of children in the adjusted space
  IPTR result = DoSuperMethodA(cl, obj, (Msg)msg);

  // Restore original bounds immediately
  _left(obj) = orig_left;
  _top(obj) = orig_top;
  _width(obj) = orig_width;
  _height(obj) = orig_height;

  return result;
}

/*** Class initialization ***************************************************/

#if ZUNE_BUILTIN_PANEL
BOOPSI_DISPATCHER(IPTR, Panel_Dispatcher, cl, obj, msg) {
  switch (msg->MethodID) {
  case OM_NEW:
    return Panel__OM_NEW(cl, obj, (struct opSet *)msg);
  case OM_DISPOSE:
    return Panel__OM_DISPOSE(cl, obj, msg);
  case OM_SET:
    return Panel__OM_SET(cl, obj, (struct opSet *)msg);
  case OM_GET:
    return Panel__OM_GET(cl, obj, (struct opGet *)msg);
  case MUIM_AskMinMax:
    return Panel__MUIM_AskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
  case MUIM_Layout:
    return Panel__MUIM_Layout(cl, obj, (struct MUIP_Layout *)msg);
  case MUIM_Draw:
    return Panel__MUIM_Draw(cl, obj, (struct MUIP_Draw *)msg);
  case MUIM_Setup:
    return Panel__MUIM_Setup(cl, obj, (struct MUIP_Setup *)msg);
  case MUIM_Cleanup:
    return Panel__MUIM_Cleanup(cl, obj, (struct MUIP_Cleanup *)msg);
  case MUIM_Show:
    return Panel__MUIM_Show(cl, obj, (struct MUIP_Show *)msg);
  case MUIM_HandleEvent:
    return Panel__MUIM_HandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
  case MUIM_Group_InitChange:
    return Panel__MUIM_Group_InitChange(cl, obj, msg);
  case MUIM_Group_ExitChange:
    return Panel__MUIM_Group_ExitChange(cl, obj, msg);
  default:
    return DoSuperMethodA(cl, obj, msg);
  }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Panel_desc = {MUIC_Panel, MUIC_Group,
                                                  sizeof(struct Panel_DATA),
                                                  (void *)Panel_Dispatcher};
#endif /* ZUNE_BUILTIN_PANEL */
