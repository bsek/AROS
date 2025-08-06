/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Panel class - A flexible container widget for organizing UI elements
*/

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
#include "panelgroup.h"
#include "prefs.h"
#include "support.h"
#include "textengine.h"

#define DEBUG 0

/* Title drawing constants */
#define TITLE_TEXT_PADDING 4    /* Padding around text within title area */
#define TITLE_VERTICAL_OFFSET 2 /* Vertical offset for text baseline */
#define TITLE_CONTENT_SPACING 4 /* Spacing between title and content areas */

/* Temporary style constants until panel.h is updated */
#ifndef MUIV_Panel_Style_UltraRounded
#define MUIV_Panel_Style_UltraRounded 5
#endif
#ifndef MUIV_Panel_Style_EnhancedRounded
#define MUIV_Panel_Style_EnhancedRounded 6
#endif

#include <aros/debug.h>

extern struct Library *MUIMasterBase;

/** Helper functions *******************************************************/

static void Panel_CalculateTextSize(BOOL use_vertical, Object *obj,
                                    STRPTR title, UWORD *actual_title_width,
                                    UWORD *actual_title_height) {
  /* Calculate actual title dimensions based on vertical setting */
  if (use_vertical) {
    struct TextExtent te;
    struct RastPort *rp = _rp(obj);

    if (!rp) {
      *actual_title_width = *actual_title_height = 0;
      return;
    }
    /* For vertical text, measure single character and multiply by string length
     */
    TextExtent(rp, "A", 1, &te);
    *actual_title_width = te.te_Width;
    *actual_title_height = te.te_Height * strlen(title);
  } else {
    /* Create ZText object for horizontal text */
    ZText *ztext = zune_text_new(NULL, title, ZTEXT_ARG_NONE, 0);
    if (!ztext) {
      *actual_title_width = *actual_title_height = 0;
      return;
    }

    /* Get text bounds */
    zune_text_get_bounds(ztext, obj);
    *actual_title_width = ztext->width;
    *actual_title_height = ztext->height;
    zune_text_destroy(ztext);
  }
}

static void Panel_CalculateTitleBounds(struct IClass *cl, Object *obj,
                                       struct Panel_DATA *data,
                                       LONG *title_left, LONG *title_top,
                                       LONG *title_right, LONG *title_bottom,
                                       UWORD *actual_title_width,
                                       UWORD *actual_title_height) {
  STRPTR title = data->title;
  BOOL use_vertical =
      data->title_vertical && (data->title_position == MUIV_Panel_Title_Left ||
                               data->title_position == MUIV_Panel_Title_Right);

  if (!title) {
    *title_left = *title_top = *title_right = *title_bottom = 0;
    *actual_title_width = *actual_title_height = 0;
    return;
  }

  Panel_CalculateTextSize(use_vertical, obj, title, actual_title_width,
                          actual_title_height);

  /* Calculate title area bounds for background using actual dimensions */
  switch (data->title_position) {
  case MUIV_Panel_Title_Top:
    *title_left = _left(obj) + data->padding;
    *title_top = _top(obj) + data->padding;
    *title_right = _right(obj) - data->padding;
    *title_bottom = *title_top + *actual_title_height;
    break;
  case MUIV_Panel_Title_Bottom:
    *title_left = _left(obj) + data->padding;
    *title_top = _bottom(obj) - *actual_title_height - data->padding;
    *title_right = _right(obj) - data->padding;
    *title_bottom = *title_top + *actual_title_height;
    break;
  case MUIV_Panel_Title_Left:
    *title_left = _left(obj) + data->padding;
    *title_top = _top(obj) + data->padding;
    *title_right = *title_left + *actual_title_width + (TITLE_TEXT_PADDING * 2);
    *title_bottom = _bottom(obj) - data->padding;
    break;
  case MUIV_Panel_Title_Right:
    *title_left = _right(obj) - data->padding - *actual_title_width -
                  (TITLE_TEXT_PADDING * 2);
    *title_top = _top(obj) + data->padding;
    *title_right = _right(obj) - data->padding;
    *title_bottom = _bottom(obj) - data->padding;
    break;
  default:
    *title_left = *title_top = *title_right = *title_bottom = 0;
    *actual_title_width = *actual_title_height = 0;
    break;
  }
}

/*
 * Handle mouse clicks on the panel
 * Returns MUI_EventHandlerRC_Eat if event was handled, 0 otherwise
 */
static IPTR Panel_HandleClick(struct IClass *cl, Object *obj, WORD x, WORD y) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  Object *parent;

  D(bug("Panel_HandleClick: x=%d, y=%d\n", x, y));

  /* Check if we have a title and if the click was on it */
  if (data->title && data->title_position != MUIV_Panel_Title_None) {
    LONG title_left, title_top, title_right, title_bottom;
    UWORD actual_title_width, actual_title_height;

    /* Use shared function to calculate title bounds */
    Panel_CalculateTitleBounds(cl, obj, data, &title_left, &title_top,
                               &title_right, &title_bottom, &actual_title_width,
                               &actual_title_height);

    /* Check if we got valid bounds */
    if (title_left == 0 && title_top == 0 && title_right == 0 &&
        title_bottom == 0)
      goto general_click;

    /* Check if click is within title bounds */
    if (x >= title_left && x < title_right && y >= title_top &&
        y < title_bottom) {
      D(bug("Panel_HandleClick: Title clicked at (%d,%d) in bounds "
            "[%d,%d,%d,%d]\n",
            x, y, title_left, title_top, title_right, title_bottom));

      /* If panel is collapsible, use delegation pattern to PanelGroup
       *
       * This architectural approach has several benefits:
       * 1. Centralized state management in PanelGroup
       * 2. Top-down layout management avoids parent relayout calls
       * 3. Better support for accordion-style behavior
       * 4. Cleaner separation of concerns
       */
      if (data->collapsible) {
        D(bug("Panel_HandleClick: Title clicked, delegating to PanelGroup\n"));

        /* Find parent and attempt delegation */
        get(obj, MUIA_Parent, &parent);
        if (parent) {
          IPTR result = 0;

          /* Try to toggle panel through PanelGroup - this will only succeed
           * if the parent is actually a PanelGroup and this panel is managed
           * by it */
          result = DoMethod(parent, MUIM_PanelGroup_TogglePanel, obj);

          if (result) {
            D(bug(
                "Panel_HandleClick: PanelGroup handled toggle successfully\n"));
            return MUI_EventHandlerRC_Eat;
          }

          D(bug("Panel_HandleClick: PanelGroup couldn't handle toggle "
                "(result=%ld)\n",
                result));
        } else {
          D(bug("Panel_HandleClick: No parent found\n"));
        }

        /* Fallback: handle locally if parent doesn't support PanelGroup methods
         * This maintains backward compatibility for standalone panels */
        D(bug("Panel_HandleClick: Handling collapse/expand locally\n"));
        DoMethod(obj, MUIM_Group_InitChange);
        set(obj, MUIA_Panel_Collapsed, !data->collapsed);
        DoMethod(obj, MUIM_Group_ExitChange);

        return MUI_EventHandlerRC_Eat;
      }

      /* Title was clicked but panel is not collapsible */
      D(bug("Panel_HandleClick: Title clicked but panel not collapsible\n"));
      return MUI_EventHandlerRC_Eat;
    }
  }

general_click:

  /* Click was not on title or panel is not collapsible */
  /* You can add other click handling logic here */

  D(bug("Panel_HandleClick: General panel area clicked\n"));

  /* For now, just consume the click to prevent it from bubbling up */
  /* You can change this behavior based on your needs */
  return MUI_EventHandlerRC_Eat;
}

static void Panel_DrawTitle(struct IClass *cl, Object *obj,
                            struct Panel_DATA *data) {
  struct RastPort *rp = _rp(obj);
  STRPTR title = data->title;
  ZText *ztext;
  LONG title_left, title_top, title_right, title_bottom;
  LONG text_left, text_right;
  UWORD actual_title_width, actual_title_height;
  BOOL use_vertical =
      data->title_vertical && (data->title_position == MUIV_Panel_Title_Left ||
                               data->title_position == MUIV_Panel_Title_Right);

  if (!rp || !title)
    return;

  /* Use shared function to calculate title bounds */
  Panel_CalculateTitleBounds(cl, obj, data, &title_left, &title_top,
                             &title_right, &title_bottom, &actual_title_width,
                             &actual_title_height);

  /* Check if we got valid bounds */
  if (title_left == 0 && title_top == 0 && title_right == 0 &&
      title_bottom == 0)
    return;

  /* Debug: Print title bounds */
  D(bug("Panel_DrawTitle: title bounds [%d,%d,%d,%d], actual_width=%d, "
        "actual_height=%d\n",
        title_left, title_top, title_right, title_bottom, actual_title_width,
        actual_title_height));
  D(bug("Panel_DrawTitle: object bounds [%d,%d,%d,%d]\n", _left(obj), _top(obj),
        _right(obj), _bottom(obj)));

  /* Draw red background for title area - use simple approach */
  SetAPen(rp, _pens(obj)[MPEN_MARK]); /* Use MARK pen for red-ish color */
  RectFill(rp, title_left, title_top, title_right, title_bottom);

  D(bug("Panel_DrawTitle: Drew red background from [%d,%d] to [%d,%d]\n",
        title_left, title_top, title_right, title_bottom));

  /* Set up text rendering with contrasting text */
  SetAPen(rp, _pens(obj)[MPEN_SHINE]); /* White/light text */
  SetBPen(rp, _pens(obj)[MPEN_MARK]);  /* Red background */
  SetDrMd(rp, JAM2);

  if (use_vertical) {
    /* Render text vertically - character by character */
    struct TextExtent te;
    LONG char_y, char_x;
    WORD char_height;

    /* Get single character dimensions */
    TextExtent(rp, "A", 1, &te);
    char_height = te.te_Height;

    /* Calculate starting position based on title_text_position */
    switch (data->title_text_position) {
    case MUIV_Panel_Title_Text_Left:
      char_y = title_top + TITLE_TEXT_PADDING;
      break;
    case MUIV_Panel_Title_Text_Right:
      char_y =
          title_bottom - (char_height * strlen(title)) - TITLE_TEXT_PADDING;
      break;
    case MUIV_Panel_Title_Text_Centered:
    default: {
      LONG available_height =
          title_bottom - title_top - (TITLE_TEXT_PADDING * 2);
      LONG text_offset = (available_height - actual_title_height) / 2;
      char_y = title_top + TITLE_TEXT_PADDING + text_offset;
    } break;
    }

    char_x = title_left + TITLE_TEXT_PADDING;

    /* Draw each character vertically */
    for (LONG i = 0; i < strlen(title); i++) {
      char single_char[2] = {title[i], '\0'};
      Move(rp, char_x, char_y + char_height);
      Text(rp, single_char, 1);
      char_y += char_height;
    }
  } else {
    /* Render text horizontally using ZText engine */
    /* Create ZText object for horizontal text */
    ztext = zune_text_new(NULL, title, ZTEXT_ARG_NONE, 0);
    if (!ztext)
      return;

    /* Calculate text positioning based on title_text_position */
    switch (data->title_text_position) {
    case MUIV_Panel_Title_Text_Left:
      text_left = title_left + TITLE_TEXT_PADDING;
      text_right = title_left + actual_title_width + TITLE_TEXT_PADDING;
      break;
    case MUIV_Panel_Title_Text_Right:
      text_left = title_right - actual_title_width - TITLE_TEXT_PADDING;
      text_right = title_right - TITLE_TEXT_PADDING;
      break;
    case MUIV_Panel_Title_Text_Centered:
    default: {
      LONG available_width =
          title_right - title_left - (TITLE_TEXT_PADDING * 2);
      LONG text_offset = (available_width - actual_title_width) / 2;
      text_left = title_left + TITLE_TEXT_PADDING + text_offset;
      text_right = text_left + actual_title_width;
    } break;
    }

    /* Draw text using Zune text engine with calculated position */
    zune_text_draw(ztext, obj, text_left, text_right,
                   title_top + TITLE_VERTICAL_OFFSET);

    /* Clean up */
    zune_text_destroy(ztext);
  }
}

/* Helper functions for drawing different frame styles */
/* Frame drawing functions removed - now using Area superclass frame system */

/*** Methods ****************************************************************/

IPTR Panel__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg) {
  struct Panel_DATA *data;
  struct TagItem *tags, *tag;

  D(bug("Panel__OM_NEW: obj=%p\n", obj));

  obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
  if (!obj)
    return 0;

  data = INST_DATA(cl, obj);

  /* Set default values */
  data->padding = 4;
  data->title = NULL;
  data->title_position = MUIV_Panel_Title_None;
  data->title_vertical = FALSE;
  data->layout_dirty = TRUE;

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
    }
  }

  D(bug("Panel created at %p\n", obj));
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
        data->layout_dirty = TRUE;
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
        data->collapsed = tag->ti_Data;
        data->layout_dirty = TRUE;
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
      }
      break;
    }
  }

  if (redraw)
    MUI_Redraw(obj, MADF_DRAWOBJECT);

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

IPTR Panel__MUIM_AskMinMax(struct IClass *cl, Object *obj,
                           struct MUIP_AskMinMax *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  struct MUI_MinMax *mi;
  UWORD title_width = 0, title_height = 0;

  /* Let the Group superclass calculate the children's requirements first */
  if (!data->collapsed) {
    DoSuperMethodA(cl, obj, (Msg)msg);
  } else {
    /* For collapsed panels, initialize MinMaxInfo to minimal values */
    mi = msg->MinMaxInfo;
    mi->MinWidth = 0;
    mi->MinHeight = 0;
    mi->DefWidth = 0;
    mi->DefHeight = 0;
    mi->MaxWidth = 0;
    mi->MaxHeight = 0;
  }
  mi = msg->MinMaxInfo;

  /* Calculate title dimensions if present */
  if (data->title && data->title_position != MUIV_Panel_Title_None) {
    BOOL use_vertical = FALSE;
    if (data->title_vertical &&
        (data->title_position == MUIV_Panel_Title_Left ||
         data->title_position == MUIV_Panel_Title_Right)) {
      use_vertical = TRUE;
    }

    Panel_CalculateTextSize(use_vertical, obj, data->title, &title_width,
                            &title_height);
  }

  /* Add padding to the Group's calculated sizes */
  mi->MinWidth += data->padding * 2;
  mi->MinHeight += data->padding * 2;
  mi->DefWidth += data->padding * 2;
  mi->DefHeight += data->padding * 2;
  if (mi->MaxWidth != MUI_MAXMAX)
    mi->MaxWidth += data->padding * 2;
  if (mi->MaxHeight != MUI_MAXMAX)
    mi->MaxHeight += data->padding * 2;

  /* Add title space to the Group's calculated sizes */
  if (data->title && data->title_position != MUIV_Panel_Title_None) {
    switch (data->title_position) {
    case MUIV_Panel_Title_Top:
    case MUIV_Panel_Title_Bottom:
      mi->MinHeight += title_height + (TITLE_TEXT_PADDING * 2);
      mi->DefHeight += title_height + (TITLE_TEXT_PADDING * 2);
      if (mi->MaxHeight != MUI_MAXMAX)
        mi->MaxHeight += title_height + (TITLE_TEXT_PADDING * 2);
      mi->MinWidth = MAX(mi->MinWidth, title_width + data->padding * 2);
      mi->DefWidth = MAX(mi->DefWidth, title_width + data->padding * 2);
      break;
    case MUIV_Panel_Title_Left:
    case MUIV_Panel_Title_Right:
      mi->MinWidth +=
          title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
      mi->DefWidth +=
          title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
      if (mi->MaxWidth != MUI_MAXMAX)
        mi->MaxWidth +=
            title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
      mi->MinHeight = MAX(mi->MinHeight, title_height + data->padding * 2);
      mi->DefHeight = MAX(mi->DefHeight, title_height + data->padding * 2);
      break;
    }
  }
  /* Debug output for calculated sizes */
  D(bug("Panel__MUIM_AskMinMax: Group calculated sizes:\n"));
  D(bug("  MinWidth=%d, MinHeight=%d\n", mi->MinWidth, mi->MinHeight));
  D(bug("  DefWidth=%d, DefHeight=%d\n", mi->DefWidth, mi->DefHeight));
  D(bug("  MaxWidth=%d, MaxHeight=%d\n", mi->MaxWidth, mi->MaxHeight));
  D(bug("  Title dimensions: width=%d, height=%d\n", title_width,
        title_height));
  D(bug("  Padding=%d, Title position=%d\n", data->padding,
        data->title_position));

  return 0;
}

IPTR Panel__MUIM_Layout(struct IClass *cl, Object *obj,
                        struct MUIP_Layout *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  WORD title_space = 0;
  WORD orig_left, orig_top, orig_width, orig_height;

  /* Calculate title dimensions if present */
  if (data->title && data->title_position != MUIV_Panel_Title_None) {
    BOOL use_vertical = FALSE;
    WORD title_width = 0, title_height = 0;
    if (data->title_vertical &&
        (data->title_position == MUIV_Panel_Title_Left ||
         data->title_position == MUIV_Panel_Title_Right)) {
      use_vertical = TRUE;
    }

    Panel_CalculateTextSize(use_vertical, obj, data->title, &title_width,
                            &title_height);
    switch (data->title_position) {
    case MUIV_Panel_Title_Top:
    case MUIV_Panel_Title_Bottom:
      title_space = title_height + (TITLE_TEXT_PADDING * 2);
      break;
    case MUIV_Panel_Title_Left:
    case MUIV_Panel_Title_Right:
      title_space =
          title_width + (TITLE_TEXT_PADDING * 2) + TITLE_CONTENT_SPACING;
      break;
    }
  }

  /* Store original bounds */
  orig_left = _left(obj);
  orig_top = _top(obj);
  orig_width = _width(obj);
  orig_height = _height(obj);

  /* Temporarily adjust the object's content area bounds for Group layout */
  switch (data->title_position) {
  case MUIV_Panel_Title_Top:
    _top(obj) += data->padding + title_space;
    _height(obj) -= data->padding * 2 + title_space;
    _left(obj) += data->padding;
    _width(obj) -= data->padding * 2;
    break;
  case MUIV_Panel_Title_Bottom:
    _top(obj) += data->padding;
    _height(obj) -= data->padding * 2 + title_space;
    _left(obj) += data->padding;
    _width(obj) -= data->padding * 2;
    break;
  case MUIV_Panel_Title_Left:
    _left(obj) += data->padding + title_space;
    _width(obj) -= data->padding * 2 + title_space;
    _top(obj) += data->padding;
    _height(obj) -= data->padding * 2;
    break;
  case MUIV_Panel_Title_Right:
    _left(obj) += data->padding;
    _width(obj) -= data->padding * 2 + title_space;
    _top(obj) += data->padding;
    _height(obj) -= data->padding * 2;
    break;
  default:
    /* No title, just apply padding */
    _left(obj) += data->padding;
    _top(obj) += data->padding;
    _width(obj) -= data->padding * 2;
    _height(obj) -= data->padding * 2;
    break;
  }

  /* Let the Group class handle the actual layout of children */
  IPTR result = 0;

  if (!data->collapsed) {
    result = DoSuperMethodA(cl, obj, (Msg)msg);
  }

  /* Restore original bounds */
  _left(obj) = orig_left;
  _top(obj) = orig_top;
  _width(obj) = orig_width;
  _height(obj) = orig_height;

  return result;
}

IPTR Panel__MUIM_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  IPTR result;

  /* Let superclass handle frame, background, and children drawing */
  result = DoSuperMethodA(cl, obj, (Msg)msg);

  /* Draw title if present and we're drawing the object */
  if ((msg->flags & MADF_DRAWOBJECT) && data->title &&
      data->title_position != MUIV_Panel_Title_None) {
    Panel_DrawTitle(cl, obj, data);
  }

  return result;
}

IPTR Panel__MUIM_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  if (!DoSuperMethodA(cl, obj, (Msg)msg))
    return FALSE;

  data->layout_dirty = TRUE;

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

  /* Remove event handler */
  if (data->ehn.ehn_Events) {
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR)&data->ehn);
  }

  return DoSuperMethodA(cl, obj, (Msg)msg);
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
        retval = Panel_HandleClick(cl, obj, x, y);
        if (retval)
          return retval;
      }
    }
  }

  /* Pass unhandled events to superclass */
  return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Panel__MUIM_Group_InitChange(struct IClass *cl, Object *obj, Msg msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);

  data->layout_dirty = TRUE;
  return DoSuperMethodA(cl, obj, msg);
}

IPTR Panel__MUIM_Group_ExitChange(struct IClass *cl, Object *obj, Msg msg) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  IPTR result;

  result = DoSuperMethodA(cl, obj, msg);

  if (data->layout_dirty) {
    /* Trigger layout recalculation by forcing parent relayout */
    if (_parent(obj)) {
      Object *parent = _parent(obj);
      /* Hide, relayout, and show to trigger AskMinMax */
      DoMethod(parent, MUIM_Hide);
      DoMethod(parent, MUIM_Layout);
      DoMethod(parent, MUIM_Show);
      MUI_Redraw(parent, MADF_DRAWOBJECT);
    }
    data->layout_dirty = FALSE;
  }

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
  case MUIM_HandleEvent:
    return Panel__MUIM_HandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
  case MUIM_Group_InitChange:
    return Panel__MUIM_Group_InitChange(cl, obj, msg);
  case MUIM_Group_ExitChange:
    return Panel__MUIM_Group_ExitChange(cl, obj, (APTR)msg);

  default:
    return DoSuperMethodA(cl, obj, msg);
  }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Panel_desc = {MUIC_Panel, MUIC_Group,
                                                  sizeof(struct Panel_DATA),
                                                  (void *)Panel_Dispatcher};
#endif /* ZUNE_BUILTIN_PANEL */
