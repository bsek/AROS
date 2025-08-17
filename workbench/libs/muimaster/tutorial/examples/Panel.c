/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Interactive Panel class demo - Control panel settings with buttons
*/

#include "libraries/mui.h"
#include "muizunesupport.h"
#include <proto/graphics.h>
#include <stdio.h>

/* Objects */
Object *app;
Object *WD_Main;
Object *WD_Demo;
Object *demo_panel;

/* Control buttons */
Object *BT_TitleNone, *BT_TitleTop, *BT_TitleBottom, *BT_TitleLeft,
    *BT_TitleRight;
Object *BT_TextLeft, *BT_TextCenter, *BT_TextRight;
Object *BT_PaddingDec, *BT_PaddingInc;
Object *BT_VerticalToggle;
Object *BT_CollapseToggle;
Object *ST_Title;
Object *TX_Status;

/* Current settings */
static LONG current_title_pos = MUIV_Panel_Title_Top;
static LONG current_text_pos = MUIV_Panel_Title_Text_Centered;
static LONG current_padding = 8;
static BOOL current_vertical = FALSE;
static BOOL current_collapsed = FALSE;
static char current_title[256] = "Demo Panel";

/* Sample content items */
static STRPTR sample_items[] = {"Item 1", "Item 2", "Item 3", "Item 4", NULL};

/**
 * Creates a rounded rectangle clipping region
 * This is a simplified version for demonstration purposes
 */
static struct Region *CreateRoundedRegion(int left, int top, int width,
                                          int height, int radius) {
  struct Region *region = NewRegion();
  if (!region)
    return NULL;

  /* Clamp radius to reasonable bounds */
  if (radius <= 0 || radius > width / 2 || radius > height / 2) {
    radius = 0;
  }

  if (radius == 0) {
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
  rect.MinX = left + radius;
  rect.MinY = top;
  rect.MaxX = left + width - radius - 1;
  rect.MaxY = top + height - 1;
  OrRectRegion(region, &rect);

  /* Left and right rectangles (reduced height) */
  rect.MinX = left;
  rect.MinY = top + radius;
  rect.MaxX = left + radius - 1;
  rect.MaxY = top + height - radius - 1;
  OrRectRegion(region, &rect);

  rect.MinX = left + width - radius;
  rect.MinY = top + radius;
  rect.MaxX = left + width - 1;
  rect.MaxY = top + height - radius - 1;
  OrRectRegion(region, &rect);

  /* Add rounded corners using simple circle approximation */
  int corners[4][2] = {
      {left + radius, top + radius},              /* top-left */
      {left + width - radius - 1, top + radius},  /* top-right */
      {left + radius, top + height - radius - 1}, /* bottom-left */
      {left + width - radius - 1,
       top + height - radius - 1}}; /* bottom-right */

  for (int corner = 0; corner < 4; corner++) {
    int cx = corners[corner][0];
    int cy = corners[corner][1];

    for (int dy = -radius; dy <= radius; dy++) {
      for (int dx = -radius; dx <= radius; dx++) {
        if (dx * dx + dy * dy <= radius * radius) {
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

/**
 * Debug hook - Step 1: No clipping to verify basic functionality
 */
AROS_UFH3S(ULONG, Step1_NoClippingHook, AROS_UFHA(struct Hook *, hook, A0),
           AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== STEP 1: No Clipping Test ===\n");
  printf("Hook called for object %p\n", obj);
  printf("  Frame width: %d\n", clipinfo->frame_width);
  printf("  Border radius: %d\n", clipinfo->border_radius);
  printf("  Has rounded corners: %s\n",
         clipinfo->has_rounded_corners ? "Yes" : "No");
  printf("  Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  /* Step 1: Always return NULL to disable clipping completely */
  *clipregion = NULL;
  printf("✓ STEP 1: No clipping applied - content should be fully visible\n\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Debug hook - Step 2: Simple rectangular clipping to test basic region
 * functionality
 */
AROS_UFH3S(ULONG, Step2_RectangleClippingHook,
           AROS_UFHA(struct Hook *, hook, A0), AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== STEP 2: Simple Rectangle Clipping Test ===\n");
  printf("Hook called for object %p\n", obj);
  printf("  Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  /* Step 2: Create a simple rectangular region with small margin */
  struct Region *region = NewRegion();
  if (region) {
    int margin = 4; /* Small margin for testing */
    struct Rectangle rect = {.MinX = _left(obj) + margin,
                             .MinY = _top(obj) + margin,
                             .MaxX = _left(obj) + _width(obj) - margin - 1,
                             .MaxY = _top(obj) + _height(obj) - margin - 1};

    if (rect.MinX <= rect.MaxX && rect.MinY <= rect.MaxY) {
      OrRectRegion(region, &rect);
      *clipregion = region;
      printf("✓ STEP 2: Created rectangular clipping region with %d pixel "
             "margin\n",
             margin);
      printf("    Region: (%d,%d) to (%d,%d)\n", rect.MinX, rect.MinY,
             rect.MaxX, rect.MaxY);
    } else {
      DisposeRegion(region);
      *clipregion = NULL;
      printf("✗ STEP 2: Invalid rectangle bounds\n");
    }
  } else {
    *clipregion = NULL;
    printf("✗ STEP 2: Failed to create region\n");
  }
  printf("\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Debug hook - Step 3: Full object rectangular clipping
 */
AROS_UFH3S(ULONG, Step3_FullRectangleHook, AROS_UFHA(struct Hook *, hook, A0),
           AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== STEP 3: Full Rectangle Clipping Test ===\n");
  printf("Hook called for object %p\n", obj);

  /* Step 3: Create full rectangular region (no margin) */
  struct Region *region = NewRegion();
  if (region) {
    struct Rectangle rect = {.MinX = _left(obj),
                             .MinY = _top(obj),
                             .MaxX = _left(obj) + _width(obj) - 1,
                             .MaxY = _top(obj) + _height(obj) - 1};

    OrRectRegion(region, &rect);
    *clipregion = region;
    printf("✓ STEP 3: Created full rectangular clipping region\n");
    printf("    Region: (%d,%d) to (%d,%d)\n", rect.MinX, rect.MinY, rect.MaxX,
           rect.MaxY);
  } else {
    *clipregion = NULL;
    printf("✗ STEP 3: Failed to create region\n");
  }
  printf("\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Debug hook - Step 4: Rounded corner clipping
 */
AROS_UFH3S(ULONG, Step4_RoundedClippingHook, AROS_UFHA(struct Hook *, hook, A0),
           AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== STEP 4: Rounded Corner Clipping Test ===\n");
  printf("Hook called for object %p\n", obj);
  printf("  Frame width: %d\n", clipinfo->frame_width);
  printf("  Border radius: %d\n", clipinfo->border_radius);
  printf("  Has rounded corners: %s\n",
         clipinfo->has_rounded_corners ? "Yes" : "No");
  printf("  Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  if (clipinfo->has_rounded_corners && clipinfo->border_radius > 0) {
    /* Use our custom region creation function */
    *clipregion = CreateRoundedRegion(_left(obj), _top(obj), _width(obj),
                                      _height(obj), clipinfo->border_radius);

    if (*clipregion) {
      printf("✓ STEP 4: Created rounded region %p for object %p (radius=%d)\n",
             *clipregion, obj, clipinfo->border_radius);
      printf("    Region bounds: (%d,%d) to (%d,%d)\n",
             (*clipregion)->bounds.MinX, (*clipregion)->bounds.MinY,
             (*clipregion)->bounds.MaxX, (*clipregion)->bounds.MaxY);
    } else {
      printf("✗ STEP 4: Failed to create rounded region\n");
    }
  } else {
    *clipregion = NULL;
    printf("✓ STEP 4: No rounded corners detected, no clipping applied\n");
  }
  printf("\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Coordinate system test hook - tries relative coordinates
 */
AROS_UFH3S(ULONG, CoordinateTest1_RelativeHook,
           AROS_UFHA(struct Hook *, hook, A0), AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== COORDINATE TEST 1: Relative Coordinates ===\n");
  printf("Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  /* Test 1: Use relative coordinates (0,0) based */
  struct Region *region = NewRegion();
  if (region) {
    int margin = 4;
    struct Rectangle rect = {.MinX = margin,
                             .MinY = margin,
                             .MaxX = _width(obj) - margin - 1,
                             .MaxY = _height(obj) - margin - 1};

    if (rect.MinX <= rect.MaxX && rect.MinY <= rect.MaxY) {
      OrRectRegion(region, &rect);
      *clipregion = region;
      printf("✓ COORD TEST 1: Created RELATIVE region with %d margin\n",
             margin);
      printf("    Region: (%d,%d) to (%d,%d)\n", rect.MinX, rect.MinY,
             rect.MaxX, rect.MaxY);
    } else {
      DisposeRegion(region);
      *clipregion = NULL;
      printf("✗ COORD TEST 1: Invalid relative bounds\n");
    }
  } else {
    *clipregion = NULL;
    printf("✗ COORD TEST 1: Failed to create region\n");
  }
  printf("\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Coordinate system test hook - tries larger margins with absolute coordinates
 */
AROS_UFH3S(ULONG, CoordinateTest2_LargeMarginHook,
           AROS_UFHA(struct Hook *, hook, A0), AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== COORDINATE TEST 2: Large Margin Absolute ===\n");
  printf("Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  /* Test 2: Use absolute coordinates but with larger margin */
  struct Region *region = NewRegion();
  if (region) {
    int margin = 20; /* Much larger margin */
    struct Rectangle rect = {.MinX = _left(obj) + margin,
                             .MinY = _top(obj) + margin,
                             .MaxX = _left(obj) + _width(obj) - margin - 1,
                             .MaxY = _top(obj) + _height(obj) - margin - 1};

    if (rect.MinX <= rect.MaxX && rect.MinY <= rect.MaxY) {
      OrRectRegion(region, &rect);
      *clipregion = region;
      printf("✓ COORD TEST 2: Created ABSOLUTE region with %d margin\n",
             margin);
      printf("    Region: (%d,%d) to (%d,%d)\n", rect.MinX, rect.MinY,
             rect.MaxX, rect.MaxY);
    } else {
      DisposeRegion(region);
      *clipregion = NULL;
      printf("✗ COORD TEST 2: Invalid absolute bounds\n");
    }
  } else {
    *clipregion = NULL;
    printf("✗ COORD TEST 2: Failed to create region\n");
  }
  printf("\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Coordinate system test hook - tries center region only
 */
AROS_UFH3S(ULONG, CoordinateTest3_CenterOnlyHook,
           AROS_UFHA(struct Hook *, hook, A0), AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== COORDINATE TEST 3: Center Region Only ===\n");
  printf("Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  /* Test 3: Very small region in the center */
  struct Region *region = NewRegion();
  if (region) {
    int w = _width(obj);
    int h = _height(obj);
    struct Rectangle rect = {.MinX = _left(obj) + w / 4,
                             .MinY = _top(obj) + h / 4,
                             .MaxX = _left(obj) + 3 * w / 4 - 1,
                             .MaxY = _top(obj) + 3 * h / 4 - 1};

    OrRectRegion(region, &rect);
    *clipregion = region;
    printf("✓ COORD TEST 3: Created CENTER region\n");
    printf("    Region: (%d,%d) to (%d,%d)\n", rect.MinX, rect.MinY, rect.MaxX,
           rect.MaxY);
  } else {
    *clipregion = NULL;
    printf("✗ COORD TEST 3: Failed to create region\n");
  }
  printf("\n");

  return 0;
  AROS_USERFUNC_EXIT
}

/**
 * Panel-specific debug hook - helps identify Panel class drawing issues
 */
AROS_UFH3S(ULONG, PanelDebugHook, AROS_UFHA(struct Hook *, hook, A0),
           AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("=== PANEL DEBUG: Clipping Hook Called ===\n");
  printf("Object: %p (should be a Panel)\n", obj);
  printf("  Class: %s\n", OCLASS(obj)->cl_ID);
  printf("  Frame width: %d\n", clipinfo->frame_width);
  printf("  Border radius: %d\n", clipinfo->border_radius);
  printf("  Has rounded corners: %s\n",
         clipinfo->has_rounded_corners ? "Yes" : "No");
  printf("  Object bounds: (%d,%d) size %dx%d\n", _left(obj), _top(obj),
         _width(obj), _height(obj));

  /* For Panel debugging, start with no clipping to verify basic functionality
   */
  *clipregion = NULL;
  printf("✓ PANEL DEBUG: No clipping applied for initial Panel test\n\n");

  return 0;
  AROS_USERFUNC_EXIT
}

AROS_UFH3S(ULONG, TestFrameClippingHook, AROS_UFHA(struct Hook *, hook, A0),
           AROS_UFHA(Object *, obj, A2),
           AROS_UFHA(struct MUIP_FrameClippingHook *, msg, A1)) {
  AROS_USERFUNC_INIT

  struct MUI_FrameClipInfo *clipinfo = msg->clipinfo;
  struct Region **clipregion = msg->clipregion;

  printf("TestFrameClippingHook called for object %p\n", obj);
  printf("  Frame width: %d\n", clipinfo->frame_width);
  printf("  Border radius: %d\n", clipinfo->border_radius);
  printf("  Has rounded corners: %s\n",
         clipinfo->has_rounded_corners ? "Yes" : "No");

  if (clipinfo->has_rounded_corners && clipinfo->border_radius > 0) {
    printf("  Creating clipping region...\n");
    /* For this test, we'll create a simple rectangular clipping region
     * In a real implementation, you would create a proper rounded region
     * based on the border_radius value */
    struct Region *region = NewRegion();
    if (region) {
      struct Rectangle rect = {.MinX = _left(obj),
                               .MinY = _top(obj),
                               .MaxX = _left(obj) + _width(obj) - 1,
                               .MaxY = _top(obj) + _height(obj) - 1};
      OrRectRegion(region, &rect);
      *clipregion = region;
      printf("  Clipping region created successfully\n");
    } else {
      *clipregion = NULL;
      printf("  Failed to create clipping region\n");
    }
  } else {
    printf("  No clipping needed for this frame\n");
    *clipregion = NULL;
  }

  return 0;
  AROS_USERFUNC_EXIT
}

/****************************************************************
 Update status text with current settings
*****************************************************************/
void UpdateStatus(void) {
  static char status_text[512];
  static char *pos_names[] = {"None", "Top", "Bottom", "Left", "Right"};
  static char *text_names[] = {"Centered", "Left", "Right"};

  ULONG panel_width = demo_panel ? _width(demo_panel) : 0;

  snprintf(status_text, sizeof(status_text),
           "Title: %s | Text: %s | Padding: %ld | Vertical: %s | Collapsed: %s "
           "| Width: %ld",
           pos_names[current_title_pos], text_names[current_text_pos],
           current_padding, current_vertical ? "Yes" : "No",
           current_collapsed ? "Yes" : "No", panel_width);

  set(TX_Status, MUIA_Text_Contents, status_text);
}

/****************************************************************
 Create control buttons panel
*****************************************************************/
Object *CreateControlPanel(void) {
  return VGroup, Child, TextObject, MUIA_Text_Contents, "Panel Demo Controls",
         MUIA_Text_PreParse, MUIX_C "\033b", MUIA_Text_SetMax, TRUE, End,

         Child, TextObject, MUIA_Text_Contents,
         "Click buttons below to change the demo panel.\nWatch the separate "
         "demo window for live updates!",
         MUIA_Text_PreParse, MUIX_C, MUIA_Frame, MUIV_Frame_Text, End,

         /* Title controls */
         Child, VGroup, GroupFrameT("Title Position"), Child, HGroup, Child,
         BT_TitleNone = SimpleButton("None"), Child,
         BT_TitleTop = SimpleButton("Top"), Child,
         BT_TitleBottom = SimpleButton("Bottom"), Child,
         BT_TitleLeft = SimpleButton("Left"), Child,
         BT_TitleRight = SimpleButton("Right"), End, Child, HGroup, Child,
         MakeLabel("Text Position:"), Child, BT_TextLeft = SimpleButton("Left"),
         Child, BT_TextCenter = SimpleButton("Center"), Child,
         BT_TextRight = SimpleButton("Right"), End, Child, HGroup, Child,
         MakeLabel("Title Text:"), Child, ST_Title = StringObject, StringFrame,
         MUIA_String_Contents, current_title, MUIA_String_MaxLen, 255, End, End,
         End,

         /* Layout controls */
         Child, VGroup, GroupFrameT("Layout"), Child, HGroup, Child,
         MakeLabel("Padding:"), Child, BT_PaddingDec = SimpleButton("-"), Child,
         BT_PaddingInc = SimpleButton("+"), End, Child, HGroup, Child,
         BT_VerticalToggle = SimpleButton("Toggle Vertical Title"), End, Child,
         HGroup, Child,
         BT_CollapseToggle = SimpleButton("Toggle Collapse/Expand"), End, End,

         Child, TextObject, MUIA_Text_Contents,
         "Use 'Toggle Collapse/Expand' to show/hide panel content while "
         "keeping the title visible.\n"
         "Notice how the panel maintains its width when collapsed.",
         MUIA_Text_PreParse, MUIX_I, MUIA_Frame, MUIV_Frame_Text, End,

         /* Status */
         Child, VGroup, GroupFrameT("Current Settings"), Child,
         TX_Status = TextObject, MUIA_Text_Contents, "", MUIA_Frame,
         MUIV_Frame_Text, End, End, End;
}

/****************************************************************
 Create the initial demo panel
*****************************************************************/
Object *CreateInitialPanel(void) {

  /* Use same hook as RecreatePanel for consistency */
  static struct Hook rounded_hook = {.h_Entry = (HOOKFUNC)PanelDebugHook};

  return demo_panel = VPanel,
         // Panel configuration with clipping hook
         MUIA_Panel_Title, current_title, MUIA_Panel_TitlePosition,
         current_title_pos, MUIA_Panel_TitleTextPosition, current_text_pos,
         MUIA_Panel_TitleVertical, current_vertical, MUIA_Panel_Padding,
         current_padding, MUIA_Panel_Collapsible, TRUE, MUIA_Panel_Collapsed,
         current_collapsed, MUIA_Frame, "D06666", MUIA_FrameClippingHook,
         &rounded_hook,

         // Panel contents (vertical layout)
         Child, TextObject, MUIA_Text_Contents, "Panel Content with Clipping",
         MUIA_Text_PreParse, MUIX_C, End,

         Child, HGroup, Child, SimpleButton("OK"), Child,
         SimpleButton("Cancel"), End,

         Child, ListviewObject, MUIA_Listview_List, ListObject,
         MUIA_List_SourceArray, sample_items, End, End,

         Child, StringObject, StringFrame, MUIA_String_Contents,
         "Enter text here...", End,

         End; // VPanel
}

/****************************************************************
 Setup button notifications
*****************************************************************/
void SetupNotifications(void) {
  /* Title position buttons */
  DoMethod(BT_TitleNone, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 10);
  DoMethod(BT_TitleTop, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 11);
  DoMethod(BT_TitleBottom, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 12);
  DoMethod(BT_TitleLeft, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 13);
  DoMethod(BT_TitleRight, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 14);

  /* Text position buttons */
  DoMethod(BT_TextLeft, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 20);
  DoMethod(BT_TextCenter, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 21);
  DoMethod(BT_TextRight, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 22);

  /* Padding buttons */
  DoMethod(BT_PaddingDec, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 32);
  DoMethod(BT_PaddingInc, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 33);

  /* Toggle buttons */
  DoMethod(BT_VerticalToggle, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 40);
  DoMethod(BT_CollapseToggle, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 41);

  /* String notification for title changes */
  DoMethod(ST_Title, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, app,
           2, MUIM_Application_ReturnID, 50);
}

/****************************************************************
 Handle button presses
*****************************************************************/
void HandleButtons(ULONG id) {
  switch (id) {
  /* Title position changes */
  case 10:
    current_title_pos = MUIV_Panel_Title_None;
    SetAttrs(demo_panel, MUIA_Panel_TitlePosition, current_title_pos, TAG_DONE);
    break;
  case 11:
    current_title_pos = MUIV_Panel_Title_Top;
    SetAttrs(demo_panel, MUIA_Panel_TitlePosition, current_title_pos, TAG_DONE);
    break;
  case 13:
    current_title_pos = MUIV_Panel_Title_Left;
    SetAttrs(demo_panel, MUIA_Panel_TitlePosition, current_title_pos, TAG_DONE);
    break;

  /* Text position changes */
  case 20:
    current_text_pos = MUIV_Panel_Title_Text_Left;
    SetAttrs(demo_panel, MUIA_Panel_TitleTextPosition, current_text_pos,
             TAG_DONE);
    break;
  case 21:
    current_text_pos = MUIV_Panel_Title_Text_Centered;
    SetAttrs(demo_panel, MUIA_Panel_TitleTextPosition, current_text_pos,
             TAG_DONE);
    break;
  case 22:
    current_text_pos = MUIV_Panel_Title_Text_Right;
    SetAttrs(demo_panel, MUIA_Panel_TitleTextPosition, current_text_pos,
             TAG_DONE);
    break;

  /* Padding changes */
  case 32:
    if (current_padding > 0) {
      current_padding--;
      SetAttrs(demo_panel, MUIA_Panel_Padding, current_padding, TAG_DONE);
    }
    break;
  case 33:
    if (current_padding < 20) {
      current_padding++;
      SetAttrs(demo_panel, MUIA_Panel_Padding, current_padding, TAG_DONE);
    }
    break;

  /* Toggle changes */
  case 40:
    current_vertical = !current_vertical;
    SetAttrs(demo_panel, MUIA_Panel_TitleVertical, current_vertical, TAG_DONE);
    break;
  case 41:
    current_collapsed = !current_collapsed;
    printf("=== COLLAPSE STATE CHANGED ===\n");
    printf("Panel collapsed state: %s\n", current_collapsed ? "TRUE" : "FALSE");
    if (demo_panel) {
      printf("Panel current width: %d\n", _width(demo_panel));
    }
    SetAttrs(demo_panel, MUIA_Panel_Collapsed, current_collapsed, TAG_DONE);
    break;

  /* Title text change */
  case 50:
    strncpy(current_title, (char *)xget(ST_Title, MUIA_String_Contents),
            sizeof(current_title) - 1);
    current_title[sizeof(current_title) - 1] = '\0';
    SetAttrs(demo_panel, MUIA_Panel_Title, current_title, TAG_DONE);
    break;

  default:
    return;
  }

  UpdateStatus();
}

/****************************************************************
 Allocate resources for gui
*****************************************************************/
BOOL init_gui(void) {
  app = ApplicationObject, MUIA_Application_Title,
  (IPTR) "Interactive Panel Demo", MUIA_Application_Version,
  (IPTR) "$VER: InteractivePanelDemo 1.0 (06.12.2024)",
  MUIA_Application_Copyright, (IPTR) "© 2024 AROS Development Team",
  MUIA_Application_Author, (IPTR) "AROS Development Team",
  MUIA_Application_Description, (IPTR) "Interactive Panel class demonstration",
  MUIA_Application_Base, (IPTR) "IPANELDEMO",

  SubWindow, WD_Main = WindowObject, MUIA_Window_Title, (IPTR) "Panel Controls",
  MUIA_Window_ID, MAKE_ID('P', 'C', 'T', 'L'), MUIA_Window_Width, 600,
  MUIA_Window_Height, 500,

  WindowContents, CreateControlPanel(), End,

  SubWindow, WD_Demo = WindowObject, MUIA_Window_Title, (IPTR) "Demo Panel",
  MUIA_Window_ID, MAKE_ID('D', 'E', 'M', 'O'), MUIA_Window_Width, 400,
  MUIA_Window_Height, 300,

  WindowContents, VGroup, GroupFrameT("Live Demo Panel"), Child,
  CreateInitialPanel(), End, End, End;

  if (app) {
    /* Quit application if either window's close gadget is pressed */
    DoMethod(WD_Main, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(WD_Demo, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    SetupNotifications();
    UpdateStatus();

    return TRUE;
  }

  return FALSE;
}

/****************************************************************
 Deallocates all gui resources
*****************************************************************/
void deinit_gui(void) {
  if (app) {
    MUI_DisposeObject(app);
  }
}

/****************************************************************
 The message loop
*****************************************************************/
void loop(void) {
  ULONG sigs = 0;
  LONG id;

  while ((id = DoMethod(app, MUIM_Application_NewInput, &sigs)) !=
         MUIV_Application_ReturnID_Quit) {
    if (id > 0) {
      HandleButtons(id);
    }

    if (sigs) {
      sigs = Wait(sigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
      if (sigs & SIGBREAKF_CTRL_C)
        break;
      if (sigs & SIGBREAKF_CTRL_D)
        break;
    }
  }
}

/****************************************************************
 The main entry point
*****************************************************************/
int main(int argc, char *argv[]) {
  if (open_libs()) {
    if (init_gui()) {
      set(WD_Main, MUIA_Window_Open, TRUE);
      set(WD_Demo, MUIA_Window_Open, TRUE);

      if (xget(WD_Main, MUIA_Window_Open) && xget(WD_Demo, MUIA_Window_Open)) {
        loop();
      }

      set(WD_Main, MUIA_Window_Open, FALSE);
      set(WD_Demo, MUIA_Window_Open, FALSE);
      deinit_gui();
    }

    close_libs();
  }

  return RETURN_OK;
}
