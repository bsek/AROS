/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Simple PanelGroup test - Basic test without custom objects to isolate issues
*/

#include <clib/alib_protos.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdio.h>

/* Objects */
Object *app;
Object *WD_Main;
Object *BT_Test;
Object *TX_Status;

/****************************************************************
 Simple test without custom classes
*****************************************************************/
void TestBasicMUI(void) {
  printf("Testing basic MUI functionality...\n");

  /* Test if we can create basic MUI objects */
  Object *test_group = VGroup, Child, TextObject, MUIA_Text_Contents,
         "Test Text", End, Child, SimpleButton("Test Button"), End;

  if (test_group) {
    printf("SUCCESS: Basic MUI group creation works\n");
    MUI_DisposeObject(test_group);
  } else {
    printf("ERROR: Basic MUI group creation failed\n");
  }

  set(TX_Status, MUIA_Text_Contents,
      "Basic MUI test completed - check console");
}

/****************************************************************
 Setup notifications
*****************************************************************/
void SetupNotifications(void) {
  DoMethod(BT_Test, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
           MUIM_Application_ReturnID, 1);
}

/****************************************************************
 Handle button presses
*****************************************************************/
void HandleButtons(ULONG id) {
  switch (id) {
  case 1:
    TestBasicMUI();
    break;
  default:
    return;
  }
}

/****************************************************************
 Initialize GUI
*****************************************************************/
BOOL init_gui(void) {
  printf("Initializing simple test GUI...\n");

  app = ApplicationObject, MUIA_Application_Title, "Simple MUI Test",
  MUIA_Application_Version, "$VER: Simple MUI Test 1.0 (24.12.2024)",
  MUIA_Application_Copyright, "© 2024 AROS Development Team",
  MUIA_Application_Author, "AROS Development Team",
  MUIA_Application_Description, "Simple MUI test without custom classes",
  MUIA_Application_Base, "SIMPLETEST",

  SubWindow, WD_Main = WindowObject, MUIA_Window_Title, "Simple MUI Test",
  MUIA_Window_ID, MAKE_ID('S', 'I', 'M', 'P'), MUIA_Window_CloseGadget, TRUE,
  MUIA_Window_Width, 400, MUIA_Window_Height, 200,

  WindowContents, VGroup, Child, VGroup, GroupFrameT("Simple Test"), Child,
  TextObject, MUIA_Text_Contents,
  "This is a simple test to verify basic MUI functionality.",
  MUIA_Text_PreParse, MUIX_C, End, Child, BT_Test = SimpleButton("Run Test"),
  Child, TX_Status = TextObject, MUIA_Text_Contents,
  "Click 'Run Test' to test basic MUI", MUIA_Frame, MUIV_Frame_Text, End, End,
  End, End, End;

  if (!app) {
    printf("ERROR: Failed to create application object\n");
    return FALSE;
  }

  printf("Application object created successfully: %p\n", app);

  /* Setup close notification */
  DoMethod(WD_Main, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
           MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

  /* Setup button notifications */
  SetupNotifications();

  printf("GUI initialization completed\n");
  return TRUE;
}

/****************************************************************
 Cleanup GUI
*****************************************************************/
void deinit_gui(void) {
  if (app) {
    printf("Disposing application object\n");
    MUI_DisposeObject(app);
  }
}

/****************************************************************
 Main event loop
*****************************************************************/
void loop(void) {
  LONG id;
  ULONG sigs = 0;

  printf("Entering main loop\n");

  while ((id = DoMethod(app, MUIM_Application_NewInput, &sigs)) !=
         MUIV_Application_ReturnID_Quit) {
    if (id > 0) {
      printf("Button ID %ld pressed\n", id);
      HandleButtons(id);
    }

    if (sigs) {
      sigs = Wait(sigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
      if (sigs & SIGBREAKF_CTRL_C) {
        printf("Ctrl+C pressed, exiting\n");
        break;
      }
      if (sigs & SIGBREAKF_CTRL_D) {
        printf("Ctrl+D pressed, exiting\n");
        break;
      }
    }
  }

  printf("Exiting main loop\n");
}

/****************************************************************
 Main function
*****************************************************************/
int main(void) {
  printf("=== Simple MUI Test Started ===\n");
  printf("This test uses only standard MUI objects\n");
  printf("to isolate any issues with custom Panel classes.\n\n");

  if (!init_gui()) {
    printf("ERROR: Failed to initialize GUI\n");
    return 1;
  }

  printf("Opening main window...\n");
  set(WD_Main, MUIA_Window_Open, TRUE);

  if (!xget(WD_Main, MUIA_Window_Open)) {
    printf("ERROR: Failed to open main window\n");
    deinit_gui();
    return 1;
  }

  printf("Main window opened successfully\n");
  printf("=== Test ready - use the GUI or press Ctrl+C to exit ===\n");

  loop();

  printf("Closing main window...\n");
  set(WD_Main, MUIA_Window_Open, FALSE);

  deinit_gui();

  printf("=== Simple MUI Test Completed ===\n");
  return 0;
}
