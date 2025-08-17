#ifndef _PANEL_TITLE_H_
#define _PANEL_TITLE_H_

/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Panel title utility functions
*/

#include <exec/types.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>

/* Title drawing constants */
#define TITLE_TEXT_PADDING 4    /* Padding around text within title area */
#define TITLE_CONTENT_SPACING 4 /* Spacing between title and content areas */

/* Forward declarations */
struct Panel_DATA;

/**
 * Calculate the actual size of title text
 *
 * @param use_vertical - Whether to render text vertically
 * @param obj - MUI object for rendering context
 * @param title - Title text string
 * @param actual_title_width - Output: calculated title width
 * @param actual_title_height - Output: calculated title height
 */
void Panel_CalculateTextSize(BOOL use_vertical, Object *obj,
                            STRPTR title, UWORD *actual_title_width,
                            UWORD *actual_title_height);

/**
 * Calculate the bounds for the title area
 *
 * @param cl - Class pointer
 * @param obj - MUI object
 * @param data - Panel instance data
 * @param title_left - Output: left coordinate of title area
 * @param title_top - Output: top coordinate of title area
 * @param title_right - Output: right coordinate of title area
 * @param title_bottom - Output: bottom coordinate of title area
 * @param actual_title_width - Output: actual title width
 * @param actual_title_height - Output: actual title height
 */
void Panel_CalculateTitleBounds(struct IClass *cl, Object *obj,
                               struct Panel_DATA *data,
                               LONG *title_left, LONG *title_top,
                               LONG *title_right, LONG *title_bottom,
                               UWORD *actual_title_width,
                               UWORD *actual_title_height);

/**
 * Draw the panel title
 *
 * @param cl - Class pointer
 * @param obj - MUI object
 * @param data - Panel instance data
 */
void Panel_DrawTitle(struct IClass *cl, Object *obj, struct Panel_DATA *data);

/**
 * Handle mouse clicks on the title area
 *
 * @param cl - Class pointer
 * @param obj - MUI object
 * @param x - Mouse x coordinate
 * @param y - Mouse y coordinate
 * @return - TRUE if click was handled, FALSE otherwise
 */
IPTR Panel_HandleTitleClick(struct IClass *cl, Object *obj, WORD x, WORD y);

#endif /* _PANEL_TITLE_H_ */
