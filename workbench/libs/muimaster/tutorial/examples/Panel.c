/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Interactive Panel class demo - Control panel settings with buttons
*/

#include "libraries/mui.h"
#include "muizunesupport.h"
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
Object *ST_Title;
Object *TX_Status;

/* Current settings */
static LONG current_title_pos = MUIV_Panel_Title_Top;
static LONG current_text_pos = MUIV_Panel_Title_Text_Centered;
static LONG current_padding = 8;
static BOOL current_vertical = FALSE;
static char current_title[256] = "Demo Panel";

/* Sample content items */
static STRPTR sample_items[] = {"Item 1", "Item 2", "Item 3", "Item 4", NULL};

/****************************************************************
 Update status text with current settings
*****************************************************************/
void UpdateStatus(void) {
  static char status_text[512];
  static char *pos_names[] = {"None", "Top", "Bottom", "Left", "Right"};
  static char *text_names[] = {"Centered", "Left", "Right"};

  snprintf(status_text, sizeof(status_text),
           "Title: %s | Text: %s | Padding: %ld | Vertical: %s",
           pos_names[current_title_pos], text_names[current_text_pos],
           current_padding, current_vertical ? "Yes" : "No");

  set(TX_Status, MUIA_Text_Contents, status_text);
}

/****************************************************************
 Recreate the demo panel with current settings
*****************************************************************/
void RecreatePanel(void) {
  Object *demo_group = (Object *)xget(WD_Demo, MUIA_Window_RootObject);
  Object *parent = (Object *)xget(demo_panel, MUIA_Parent);
  Object *new_panel;

  /* Create new panel with current settings */
  new_panel = VPanel, MUIA_Panel_Title, current_title, MUIA_Panel_TitlePosition,
  current_title_pos, MUIA_Panel_TitleTextPosition, current_text_pos,
  MUIA_Panel_TitleVertical, current_vertical, MUIA_Panel_Padding,
  current_padding, MUIA_Panel_Collapsible, TRUE,

  Child, TextObject, MUIA_Text_Contents, "Sample Content", MUIA_Text_PreParse,
  MUIX_C, End, Child, HGroup, Child, SimpleButton("OK"), Child,
  SimpleButton("Cancel"), End, Child, ListviewObject, MUIA_Listview_List,
  ListObject, MUIA_List_SourceArray, sample_items, End, End, Child,
  StringObject, StringFrame, MUIA_String_Contents, "Enter text here...", End,
  End;

  if (new_panel) {
    /* Replace the old panel in the demo window */
    DoMethod(parent, MUIM_Group_InitChange);
    DoMethod(parent, OM_REMMEMBER, demo_panel);
    DoMethod(parent, OM_ADDMEMBER, new_panel);
    DoMethod(parent, MUIM_Group_ExitChange);

    MUI_DisposeObject(demo_panel);
    demo_panel = new_panel;

    UpdateStatus();
  }
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
         BT_VerticalToggle = SimpleButton("Toggle Vertical Title"), End, End,

         /* Status */
         Child, VGroup, GroupFrameT("Current Settings"), Child,
         TX_Status = TextObject, MUIA_Text_Contents, "", MUIA_Frame,
         MUIV_Frame_Text, End, End, End;
}

/****************************************************************
 Create the initial demo panel
*****************************************************************/
Object *CreateInitialPanel(void) {
  return demo_panel = VPanel,
         // Panel configuration
         MUIA_Panel_Title, current_title, MUIA_Panel_TitlePosition,
         current_title_pos, MUIA_Panel_TitleTextPosition, current_text_pos,
         MUIA_Panel_TitleVertical, current_vertical, MUIA_Panel_Padding,
         current_padding, MUIA_Panel_Collapsible, TRUE,

         // Panel contents (vertical layout)
         Child, TextObject, MUIA_Text_Contents, "Sample Content",
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
    break;
  case 11:
    current_title_pos = MUIV_Panel_Title_Top;
    break;
  case 12:
    current_title_pos = MUIV_Panel_Title_Bottom;
    break;
  case 13:
    current_title_pos = MUIV_Panel_Title_Left;
    break;
  case 14:
    current_title_pos = MUIV_Panel_Title_Right;
    break;

  /* Text position changes */
  case 20:
    current_text_pos = MUIV_Panel_Title_Text_Left;
    break;
  case 21:
    current_text_pos = MUIV_Panel_Title_Text_Centered;
    break;
  case 22:
    current_text_pos = MUIV_Panel_Title_Text_Right;
    break;

  /* Padding changes */
  case 32:
    if (current_padding > 0)
      current_padding--;
    break;
  case 33:
    if (current_padding < 20)
      current_padding++;
    break;

  /* Toggle changes */
  case 40:
    current_vertical = !current_vertical;
    break;

  /* Title text change */
  case 50:
    strncpy(current_title, (char *)xget(ST_Title, MUIA_String_Contents),
            sizeof(current_title) - 1);
    current_title[sizeof(current_title) - 1] = '\0';
    break;

  default:
    return;
  }

  RecreatePanel();
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
