/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Panel title utility functions implementation
*/

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

#include "imspec_intern.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "panel_private.h"
#include "panel_title.h"
#include "panelgroup.h"
#include "support.h"
#include "textengine.h"

#define DEBUG 0
#include <aros/debug.h>

/* Arrow constants from vector table */
#define ARROW_UP_VECTOR 0
#define ARROW_DOWN_VECTOR 1
#define ARROW_LEFT_VECTOR 2
#define ARROW_RIGHT_VECTOR 3

/* Arrow size constants */
#define ARROW_WIDTH 12
#define ARROW_HEIGHT 12
#define ARROW_MARGIN 4

extern struct Library *MUIMasterBase;

/* Forward declarations for arrow drawing */
static void Panel_DrawFilledArrow(struct RastPort *rp, LONG left, LONG top,
                                  LONG width, LONG height, ULONG direction);

void Panel_CalculateTextSize(BOOL use_vertical, Object *obj, STRPTR title,
                             UWORD *actual_title_width,
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
    *actual_title_width = ztext->width + TITLE_TEXT_PADDING * 2;
    *actual_title_height = ztext->height + TITLE_TEXT_PADDING * 2;
    zune_text_destroy(ztext);
  }
}

void Panel_CalculateTitleBounds(struct IClass *cl, Object *obj,
                                struct Panel_DATA *data, LONG *title_left,
                                LONG *title_top, LONG *title_right,
                                LONG *title_bottom, UWORD *actual_text_width,
                                UWORD *actual_text_height) {
  STRPTR title = data->title;
  BOOL use_vertical =
      data->title_vertical && (data->title_position == MUIV_Panel_Title_Left);

  if (!title) {
    *title_left = *title_top = *title_right = *title_bottom = 0;
    *actual_text_width = *actual_text_height = 0;
    return;
  }

  Panel_CalculateTextSize(use_vertical, obj, title, actual_text_width,
                          actual_text_height);

  /* Calculate title area bounds for background using actual dimensions */
  switch (data->title_position) {
  case MUIV_Panel_Title_Top:
    *title_left = _left(obj) + data->padding;
    *title_top = _top(obj) + data->padding;
    *title_right = _right(obj) - data->padding;
    *title_bottom = *title_top + *actual_text_height;
    break;
  case MUIV_Panel_Title_Left:
    *title_left = _left(obj) + data->padding;
    *title_top = _top(obj) + data->padding;
    *title_right = *title_left + *actual_text_width + (TITLE_TEXT_PADDING * 2);
    /* Add space for arrow if collapsible */
    if (data->collapsible) {
      *title_right += ARROW_WIDTH + ARROW_MARGIN;
    }
    *title_bottom = _bottom(obj) - data->padding;
    break;
  default:
    *title_left = *title_top = *title_right = *title_bottom = 0;
    *actual_text_width = *actual_text_height = 0;
    break;
  }
}

static void Panel_DrawFilledArrow(struct RastPort *rp, LONG left, LONG top,
                                  LONG width, LONG height, ULONG direction) {
  LONG cx = left + width / 2;
  LONG cy = top + height / 3;
  LONG arrow_size = 4; /* Fixed size for reliability */

  /* Draw filled arrow using simple RectFill approach */
  switch (direction) {
  case ARROW_DOWN_VECTOR:
    /* Draw upward triangle using horizontal lines */
    for (LONG y = 0; y <= arrow_size; y++) {
      LONG line_width = (arrow_size - y) * 2;
      if (line_width > 0) {
        RectFill(rp, cx - line_width / 2, cy - arrow_size + y,
                 cx + line_width / 2, cy - arrow_size + y);
      }
    }
    break;

  case ARROW_UP_VECTOR:
    /* Draw downward triangle using horizontal lines */
    for (LONG y = 0; y <= arrow_size; y++) {
      LONG line_width = (arrow_size - y) * 2;
      if (line_width > 0) {
        RectFill(rp, cx - line_width / 2, cy + arrow_size - y,
                 cx + line_width / 2, cy + arrow_size - y);
      }
    }
    break;

  case ARROW_RIGHT_VECTOR:
    /* Draw leftward triangle using vertical lines */
    for (LONG x = 0; x <= arrow_size; x++) {
      LONG line_height = (arrow_size - x) * 2;
      if (line_height > 0) {
        RectFill(rp, cx - arrow_size + x, cy - line_height / 2,
                 cx - arrow_size + x, cy + line_height / 2);
      }
    }
    break;

  case ARROW_LEFT_VECTOR:
    /* Draw rightward triangle using vertical lines */
    for (LONG x = 0; x <= arrow_size; x++) {
      LONG line_height = (arrow_size - x) * 2;
      if (line_height > 0) {
        RectFill(rp, cx + arrow_size - x, cy - line_height / 2,
                 cx + arrow_size - x, cy + line_height / 2);
      }
    }
    break;
  }
}

static void Panel_DrawArrow(Object *obj, struct Panel_DATA *data,
                            LONG arrow_left, LONG arrow_top) {
  struct RastPort *rp = _rp(obj);
  ULONG arrow_direction;

  if (!rp || !data->collapsible)
    return;

  /* Choose arrow direction based on title position and collapsed state */
  switch (data->title_position) {
  case MUIV_Panel_Title_Top:
    arrow_direction = data->collapsed ? ARROW_RIGHT_VECTOR : ARROW_DOWN_VECTOR;
    break;
  case MUIV_Panel_Title_Left:
    arrow_direction = data->collapsed ? ARROW_UP_VECTOR : ARROW_RIGHT_VECTOR;
    break;

  default:
    return;
  }

  /* Set pen color for arrow */
  SetAPen(rp, _pens(obj)[MPEN_SHINE]); /* Use bright pen for visibility */
  SetDrMd(rp, JAM1);

  /* Draw filled arrow */
  Panel_DrawFilledArrow(rp, arrow_left, arrow_top, ARROW_WIDTH, ARROW_HEIGHT,
                        arrow_direction);
}

static void Panel_DrawSeparator(Object *obj, UWORD x, UWORD y, UWORD width) {
  struct RastPort *rp = _rp(obj);

  if (!rp)
    return;

  /* Draw a horizontal line */
  SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
  SetDrMd(rp, JAM1);
  Move(rp, x, y);
  Draw(rp, x + width - 1, y);
}

void Panel_DrawTitle(struct IClass *cl, Object *obj, struct Panel_DATA *data) {
  struct RastPort *rp = _rp(obj);
  STRPTR title = data->title;
  ZText *ztext;
  LONG title_left, title_top, title_right, title_bottom;
  LONG text_left, text_right;
  UWORD actual_text_width, actual_text_height;
  BOOL use_vertical =
      data->title_vertical && data->title_position == MUIV_Panel_Title_Left;

  /* Debug: Entry point confirmation */
  D(bug("Panel_DrawTitle: ENTRY - title='%s' collapsible=%s collapsed=%s "
        "pos=%d\n",
        title, data->collapsible ? "TRUE" : "FALSE",
        data->collapsed ? "TRUE" : "FALSE", (int)data->title_position));

  if (!rp || !title) {
    D(bug("Panel_DrawTitle: EXIT early - rp=%p title=%p\n", rp, title));
    return;
  }

  /* Use shared function to calculate title bounds */
  Panel_CalculateTitleBounds(cl, obj, data, &title_left, &title_top,
                             &title_right, &title_bottom, &actual_text_width,
                             &actual_text_height);

  /* Check if we got valid bounds */
  if (title_left == 0 && title_top == 0 && title_right == 0 &&
      title_bottom == 0)
    return;

  /* Draw arrow FIRST if collapsible, before background */
  if (data->collapsible) {
    LONG arrow_x, arrow_y;

    /* Calculate arrow position before drawing background */
    switch (data->title_position) {
    case MUIV_Panel_Title_Top:
      arrow_x = title_left;
      arrow_y = title_top + ((title_bottom - title_top - ARROW_HEIGHT) / 2);
      break;
    case MUIV_Panel_Title_Left:
      arrow_x = title_left + (title_right - title_left - ARROW_WIDTH) / 2;
      arrow_y = title_bottom - ARROW_HEIGHT - TITLE_TEXT_PADDING;
      break;
    default:
      arrow_x = arrow_y = 0;
      break;
    }

    if (arrow_x > 0 && arrow_y > 0) {
      D(bug("Panel_DrawTitle: Pre-background arrow at (%d,%d)\n", (int)arrow_x,
            (int)arrow_y));
      Panel_DrawArrow(obj, data, arrow_x, arrow_y);
    }
  }

  /* Set up text rendering with contrasting text */
  SetAPen(rp, _pens(obj)[MPEN_SHINE]); /* White/light text */
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
      LONG text_offset = (available_height - actual_text_height) / 2;
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

    /* Arrow already drawn before background, skip duplicate drawing for
     * vertical text */
    D(bug("Panel_DrawTitle: Vertical text - arrow already drawn before "
          "background\n"));
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
      text_right = title_left + actual_text_width + TITLE_TEXT_PADDING;
      break;
    case MUIV_Panel_Title_Text_Right:
      text_left = title_right - actual_text_width - TITLE_TEXT_PADDING;
      text_right = title_right - TITLE_TEXT_PADDING;
      break;
    case MUIV_Panel_Title_Text_Centered:
    default: {
      LONG available_width =
          title_right - title_left - (TITLE_TEXT_PADDING * 2);
      LONG text_offset = (available_width - actual_text_width) / 2;
      text_left = title_left + TITLE_TEXT_PADDING + text_offset;
      text_right = text_left + actual_text_width;
    } break;
    }

    /* Draw text using Zune text engine with calculated position */
    zune_text_draw(ztext, obj, text_left, text_right,
                   title_top + ((title_bottom - title_top) / 2) -
                       (actual_text_height / 2));

    /* Clean up */
    zune_text_destroy(ztext);

    Panel_DrawSeparator(obj, title_left, title_bottom,
                        title_right - title_left);
  }
}

IPTR Panel_HandleTitleClick(struct IClass *cl, Object *obj, WORD x, WORD y) {
  struct Panel_DATA *data = INST_DATA(cl, obj);
  Object *parent;

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
      return MUI_EventHandlerRC_Eat;

    /* Check if click is within title bounds (including arrow area) */
    if (x >= title_left && x < title_right && y >= title_top &&
        y < title_bottom) {
      D(bug("Panel_HandleTitleClick: Title clicked at (%d,%d) in bounds "
            "[%d,%d,%d,%d]\n",
            x, y, title_left, title_top, title_right, title_bottom));

      /* If panel is collapsible, use delegation pattern to PanelGroup
       *
       * This architectural approach has several benefits:
       * 1. Centralized state management in PanelGroup
       * 2. Top-down layout management avoids parent relayout calls
       * 3. Better support for accordion-style behaviour
       */
      if (data->collapsible) {
        D(bug("Panel_HandleTitleClick: Title clicked, delegating to "
              "PanelGroup\n"));

        /* Find parent and attempt delegation */
        get(obj, MUIA_Parent, &parent);
        if (parent) {
          IPTR result = 0;

          /* Try to toggle panel through PanelGroup - this will only succeed
           * if the parent is actually a PanelGroup and this panel is managed
           * by it */
          result = DoMethod(parent, MUIM_PanelGroup_TogglePanel, obj);

          if (result) {
            D(bug("Panel_HandleTitleClick: PanelGroup handled toggle "
                  "successfully\n"));

            return 0;
          }
        }

        /* Fallback: handle locally if parent doesn't support PanelGroup methods
         * This maintains backward compatibility for standalone panels */
        D(bug("Panel_HandleTitleClick: Handling collapse/expand locally\n"));
        DoMethod(obj, MUIM_Group_InitChange);
        set(obj, MUIA_Panel_Collapsed, !data->collapsed);
        DoMethod(obj, MUIM_Group_ExitChange);

        /* Call title clicked hook if set, this enables non parentgroup parents
         * to relayout if needed */
        if (data->title_clicked_hook) {
          CallHookPkt(data->title_clicked_hook, obj, NULL);
        }
      }
    }
  }

  return 0;
}
