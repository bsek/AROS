/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Interactive PanelGroup class demo - Demonstrates container for managing
   collapsible panels
*/

#include "libraries/mui.h"
#include "muizunesupport.h"
#include <stdio.h>

/* Objects */
Object *app;
Object *WD_Main;
Object *demo_panelgroup;

/* Control buttons */
Object *BT_CollapseAll, *BT_ExpandAll, *BT_ToggleMultiple;
Object *BT_CollapsePanel1, *BT_ExpandPanel1, *BT_TogglePanel1;
Object *BT_CollapsePanel2, *BT_ExpandPanel2, *BT_TogglePanel2;
Object *BT_CollapsePanel3, *BT_ExpandPanel3, *BT_TogglePanel3;
Object *TX_Status;

/* Panel objects */
Object *panel1, *panel2, *panel3;

/* Current settings */
static BOOL allow_multiple = TRUE;

/* Sample content items */
static STRPTR list_items1[] = {"File 1.txt", "File 2.doc", "File 3.pdf", NULL};
static STRPTR list_items2[] = {"Option A", "Option B", "Option C", "Option D",
                               NULL};
static STRPTR list_items3[] = {"Task 1: Complete", "Task 2: In Progress",
                               "Task 3: Pending", NULL};

/****************************************************************
 Update status display
*****************************************************************/
void UpdateStatus(void) {
  static char status_text[256];

  snprintf(
      status_text, sizeof(status_text),
      "Allow Multiple: %s | Panel States: %s %s %s",
      allow_multiple ? "Yes" : "No",
      (BOOL)DoMethod(demo_panelgroup, MUIM_PanelGroup_GetPanelState, panel1)
          ? "C"
          : "E",
      (BOOL)DoMethod(demo_panelgroup, MUIM_PanelGroup_GetPanelState, panel2)
          ? "C"
          : "E",
      (BOOL)DoMethod(demo_panelgroup, MUIM_PanelGroup_GetPanelState, panel3)
          ? "C"
          : "E");

  set(TX_Status, MUIA_Text_Contents, status_text);
}

/****************************************************************
 Create the panels for the demo
*****************************************************************/
Object *CreatePanel1(void) {
  return panel1 = VPanel, MUIA_Panel_Title, "Files", MUIA_Panel_TitlePosition,
         MUIV_Panel_Title_Top, MUIA_Panel_Collapsible, TRUE, MUIA_Panel_Padding,
         8,

         Child, TextObject, MUIA_Text_Contents,
         "Document files in project folder:", MUIA_Text_PreParse, MUIX_L, End,

         Child, ListviewObject, MUIA_Listview_List, ListObject,
         MUIA_List_SourceArray, list_items1, End, End,

         Child, HGroup, Child, SimpleButton("Open"), Child,
         SimpleButton("Delete"), Child, SimpleButton("Rename"), End,

         End;
}

Object *CreatePanel2(void) {
  return panel2 = VPanel, MUIA_Panel_Title, "Settings",
         MUIA_Panel_TitlePosition, MUIV_Panel_Title_Top, MUIA_Panel_Collapsible,
         TRUE, MUIA_Panel_Padding, 8,

         Child, TextObject, MUIA_Text_Contents,
         "Configuration Options:", MUIA_Text_PreParse, MUIX_L, End,

         Child, VGroup, Child, HGroup, Child, MakeLabel("Theme:"), Child,
         CycleObject, MUIA_Cycle_Entries, list_items2, End, End,

         Child, HGroup, Child, MakeLabel("Auto-save:"), Child, CheckMark(TRUE),
         End,

         Child, HGroup, Child, MakeLabel("Backup count:"), Child, StringObject,
         StringFrame, MUIA_String_Contents, "5", MUIA_String_Integer, 5, End,
         End, End,

         Child, HGroup, Child, SimpleButton("Apply"), Child,
         SimpleButton("Reset"), End,

         End;
}

Object *CreatePanel3(void) {
  return panel3 = VPanel, MUIA_Panel_Title, "Tasks", MUIA_Panel_TitlePosition,
         MUIV_Panel_Title_Top, MUIA_Panel_Collapsible, TRUE, MUIA_Panel_Padding,
         8, MUIA_Panel_Collapsed, TRUE, // Start collapsed

         Child, TextObject, MUIA_Text_Contents,
         "Current project tasks:", MUIA_Text_PreParse, MUIX_L, End,

         Child, ListviewObject, MUIA_Listview_List, ListObject,
         MUIA_List_SourceArray, list_items3, End, End,

         Child, HGroup, Child, SimpleButton("Add Task"), Child,
         SimpleButton("Mark Done"), Child, SimpleButton("Edit"), End,

         End;
}

/****************************************************************
 Create control panel
*****************************************************************/
Object *CreateControlPanel(void) {
  return VGroup, Child, TextObject, MUIA_Text_Contents,
         "PanelGroup Demo Controls", MUIA_Text_PreParse, MUIX_C "\033b",
         MUIA_Text_SetMax, TRUE, End,

         Child, TextObject, MUIA_Text_Contents,
         "Control the panel group and individual panels below.",
         MUIA_Text_PreParse, MUIX_C, MUIA_Frame, MUIV_Frame_Text, End,

         /* Group controls */
         Child, VGroup, GroupFrameT("Group Controls"), Child, HGroup, Child,
         BT_CollapseAll = SimpleButton("Collapse All"), Child,
         BT_ExpandAll = SimpleButton("Expand All"), Child,
         BT_ToggleMultiple = SimpleButton("Toggle Multiple Mode"), End, End,

         /* Individual panel controls */
         Child, VGroup, GroupFrameT("Panel Controls"), Child, HGroup, Child,
         MakeLabel("Files Panel:"), Child,
         BT_CollapsePanel1 = SimpleButton("Collapse"), Child,
         BT_ExpandPanel1 = SimpleButton("Expand"), Child,
         BT_TogglePanel1 = SimpleButton("Toggle"), End, Child, HGroup, Child,
         MakeLabel("Settings Panel:"), Child,
         BT_CollapsePanel2 = SimpleButton("Collapse"), Child,
         BT_ExpandPanel2 = SimpleButton("Expand"), Child,
         BT_TogglePanel2 = SimpleButton("Toggle"), End, Child, HGroup, Child,
         MakeLabel("Tasks Panel:"), Child,
         BT_CollapsePanel3 = SimpleButton("Collapse"), Child,
         BT_ExpandPanel3 = SimpleButton("Expand"), Child,
         BT_TogglePanel3 = SimpleButton("Toggle"), End, End,

         /* Status */
         Child, VGroup, GroupFrameT("Status"), Child, TX_Status = TextObject,
         MUIA_Text_Contents, "", MUIA_Frame, MUIV_Frame_Text, End, End,

         End;
}

/****************************************************************
 Create the demo panel group
*****************************************************************/
Object *CreateDemoPanelGroup(void) {
  return demo_panelgroup = VPanelGroup, MUIA_PanelGroup_AllowMultiple,
         allow_multiple,

         Child, CreatePanel1(), Child, CreatePanel2(), Child, CreatePanel3(),

         End;
}

/****************************************************************
 Setup button notifications
*****************************************************************/
void SetupNotifications(void) {
  /* Group control buttons */
  DoMethod(BT_CollapseAll, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 1);
  DoMethod(BT_ExpandAll, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 2);
  DoMethod(BT_ToggleMultiple, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 3);

  /* Panel 1 controls */
  DoMethod(BT_CollapsePanel1, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 10);
  DoMethod(BT_ExpandPanel1, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 11);
  DoMethod(BT_TogglePanel1, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 12);

  /* Panel 2 controls */
  DoMethod(BT_CollapsePanel2, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 20);
  DoMethod(BT_ExpandPanel2, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 21);
  DoMethod(BT_TogglePanel2, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 22);

  /* Panel 3 controls */
  DoMethod(BT_CollapsePanel3, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 30);
  DoMethod(BT_ExpandPanel3, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 31);
  DoMethod(BT_TogglePanel3, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 32);
}

/****************************************************************
 Handle button presses
*****************************************************************/
void HandleButtons(ULONG id) {
  switch (id) {
  /* Group controls */
  case 1:
    set(demo_panelgroup, MUIA_PanelGroup_CollapseAll, TRUE);
    break;
  case 2:
    set(demo_panelgroup, MUIA_PanelGroup_ExpandAll, TRUE);
    break;
  case 3:
    allow_multiple = !allow_multiple;
    set(demo_panelgroup, MUIA_PanelGroup_AllowMultiple, allow_multiple);
    break;

  /* Panel 1 controls */
  case 10:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_CollapsePanel, panel1);
    break;
  case 11:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_ExpandPanel, panel1);
    break;
  case 12:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_TogglePanel, panel1);
    break;

  /* Panel 2 controls */
  case 20:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_CollapsePanel, panel2);
    break;
  case 21:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_ExpandPanel, panel2);
    break;
  case 22:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_TogglePanel, panel2);
    break;

  /* Panel 3 controls */
  case 30:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_CollapsePanel, panel3);
    break;
  case 31:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_ExpandPanel, panel3);
    break;
  case 32:
    DoMethod(demo_panelgroup, MUIM_PanelGroup_TogglePanel, panel3);
    break;

  default:
    return;
  }

  UpdateStatus();
}

/****************************************************************
 Initialize GUI
*****************************************************************/
BOOL init_gui(void) {
  app = ApplicationObject, MUIA_Application_Title, "PanelGroup Demo",
  MUIA_Application_Version, "$VER: PanelGroup Demo 1.0 (24.12.2024)",
  MUIA_Application_Copyright, "© 2024 AROS Development Team",
  MUIA_Application_Author, "AROS Development Team",
  MUIA_Application_Description, "PanelGroup class demonstration",
  MUIA_Application_Base, "PANELGROUPDEMO",

  SubWindow, WD_Main = WindowObject, MUIA_Window_Title,
  "PanelGroup Demo - Controls & Demo", MUIA_Window_ID,
  MAKE_ID('M', 'A', 'I', 'N'), MUIA_Window_CloseGadget, TRUE, MUIA_Window_Width,
  800, MUIA_Window_Height, 600,

  WindowContents, HGroup,
  /* Control panel on the left */
      Child, VGroup, GroupFrameT("Controls"), MUIA_Weight, 40, Child,
  CreateControlPanel(), End,

  /* Demo panel group on the right */
      Child, VGroup, GroupFrameT("PanelGroup Demo"), MUIA_Weight, 60, Child,
  ScrollgroupObject, MUIA_Scrollgroup_Contents, VGroupV, VirtualFrame, Child,
  CreateDemoPanelGroup(), End, End, End, End, End, End;

  if (!app) {
    return FALSE;
  }

  /* Setup notifications */
  SetupNotifications();

  /* Initialize status */
  UpdateStatus();

  /* Open main window */
  set(WD_Main, MUIA_Window_Open, TRUE);

  return TRUE;
}

/****************************************************************
 Cleanup GUI
*****************************************************************/
void deinit_gui(void) {
  if (app) {
    MUI_DisposeObject(app);
  }
}

/****************************************************************
 Main event loop
*****************************************************************/
void loop(void) {
  ULONG sigs = 0;

  while (DoMethod(app, MUIM_Application_NewInput, &sigs) !=
         MUIV_Application_ReturnID_Quit) {
    if (sigs) {
      sigs = Wait(sigs | SIGBREAKF_CTRL_C);
      if (sigs & SIGBREAKF_CTRL_C)
        break;
    }

    ULONG id = (ULONG)xget(app, MUIA_Application_ReturnID);
    if (id != MUIV_Application_ReturnID_Quit) {
      HandleButtons(id);
    }
  }
}

/****************************************************************
 Main function
*****************************************************************/
int main(void) {
  if (!init_gui()) {
    printf("Failed to initialize GUI\n");
    return 1;
  }

  printf("PanelGroup Demo started. Close window or press Ctrl+C to exit.\n");
  printf("Use the control buttons to test PanelGroup functionality.\n");

  loop();

  deinit_gui();
  return 0;
}
