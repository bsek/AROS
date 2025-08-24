/*
    Copyright (C) 2025, The AROS Development Team.
    All rights reserved.

    Test program for rounded frame support in Zune/MUI
*/

#include <dos/dos.h>
#include <exec/types.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#ifdef __AROS__
#include <libraries/mui.h>
#endif
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

#ifndef __AROS__

#include <mui.h>
#undef SysBase

/* On AmigaOS we build a fake library base, because it's not compiled as
 * sharedlibrary yet */
#include "muimaster_intern.h"

int openmuimaster(void) {
  static struct MUIMasterBase_intern MUIMasterBase_instance;
  MUIMasterBase = (struct Library *)&MUIMasterBase_instance;

  MUIMasterBase_instance.sysbase = *((struct ExecBase **)4);
  MUIMasterBase_instance.dosbase = OpenLibrary("dos.library", 37);
  MUIMasterBase_instance.utilitybase = OpenLibrary("utility.library", 37);
  MUIMasterBase_instance.aslbase = OpenLibrary("asl.library", 37);
  MUIMasterBase_instance.gfxbase = OpenLibrary("graphics.library", 37);
  MUIMasterBase_instance.layersbase = OpenLibrary("layers.library", 37);
  MUIMasterBase_instance.intuibase = OpenLibrary("intuition.library", 37);
  MUIMasterBase_instance.cxbase = OpenLibrary("commodities.library", 37);
  MUIMasterBase_instance.keymapbase = OpenLibrary("keymap.library", 37);
  __zune_prefs_init(&__zprefs);

  return 1;
}

void closemuimaster(void) {}

#else

int openmuimaster(void) {
  if ((MUIMasterBase = OpenLibrary("muimaster.library", 0)))
    return 1;
  return 0;
}

void closemuimaster(void) {
  if (MUIMasterBase)
    CloseLibrary(MUIMasterBase);
}

#endif

int main(int argc, char **argv) {
  Object *app;
  Object *mainWin;

  if (!openmuimaster())
    return 0;

  /*
   * Create test application with various frame types to test rounded frame
   * support
   */
  app = ApplicationObject, MUIA_Application_Author, "AROS Development Team",
  MUIA_Application_Title, "Rounded Frame Test", MUIA_Application_Version,
  "$VER: RoundedFrameTest 1.0 (02.01.25)", MUIA_Application_Description,
  "Test program for rounded frame support", SubWindow, mainWin = WindowObject,
  MUIA_Window_Title, "Rounded Frame Test - Zune", MUIA_Window_ID,
  MAKE_ID('R', 'N', 'D', 'F'), WindowContents, VGroup, Child, TextObject,
  MUIA_Text_Contents, "\33cRounded Frame Test", MUIA_Text_SetMax, TRUE,
  MUIA_Font, MUIV_Font_Big, End, Child, VSpace(10),

  /* Row 1: Compare normal vs rounded frames */
      Child, HGroup, Child, VGroup, MUIA_Frame, MUIV_Frame_Group, Child,
  TextObject, MUIA_Text_Contents, "\33cNormal Frames", MUIA_Font,
  MUIV_Font_Tiny, End, Child, VSpace(5), Child, RectangleObject,
  MUIA_Background, MUII_ButtonBack, MUIA_Frame, MUIV_Frame_Button,
  MUIA_FixHeight, 50, MUIA_InnerLeft, 10, MUIA_InnerRight, 10, MUIA_InnerTop,
  10, MUIA_InnerBottom, 10, End, Child, VSpace(5), Child, RectangleObject,
  MUIA_Background, MUII_FILLSHINE, MUIA_Frame, MUIV_Frame_Text, MUIA_FixHeight,
  50, MUIA_InnerLeft, 10, MUIA_InnerRight, 10, MUIA_InnerTop, 10,
  MUIA_InnerBottom, 10, End, End, Child, VGroup, MUIA_Frame, MUIV_Frame_Group,
  Child, TextObject, MUIA_Text_Contents, "\33cRounded Frames", MUIA_Font,
  MUIV_Font_Tiny, End, Child, VSpace(5), Child, RectangleObject,
  MUIA_Background, MUII_ButtonBack, MUIA_Frame,
  "D13333333", /* FST_ROUNDED frame spec */
      MUIA_FixHeight, 50, MUIA_InnerLeft, 10, MUIA_InnerRight, 10,
  MUIA_InnerTop, 10, MUIA_InnerBottom, 10, End, Child, VSpace(5), Child,
  RectangleObject, MUIA_Background, MUII_FILLSHINE, MUIA_Frame,
  "D03333333", /* FST_ROUNDED down frame spec */
      MUIA_FixHeight, 50, MUIA_InnerLeft, 10, MUIA_InnerRight, 10,
  MUIA_InnerTop, 10, MUIA_InnerBottom, 10, End, End, End,

  Child, VSpace(10),

  /* Row 2: Different backgrounds with rounded frames */
      Child, TextObject, MUIA_Text_Contents,
  "\33cDifferent Backgrounds with Rounded Frames", MUIA_Font, MUIV_Font_Tiny,
  End, Child, VSpace(5), Child, HGroup, Child, RectangleObject, MUIA_Background,
  MUII_SHADOWBACK, MUIA_Frame, "D13333333", MUIA_FixHeight, 60, MUIA_InnerLeft,
  15, MUIA_InnerRight, 15, MUIA_InnerTop, 15, MUIA_InnerBottom, 15, End, Child,
  RectangleObject, MUIA_Background, MUII_TextBack, MUIA_Frame, "D13333333",
  MUIA_FixHeight, 60, MUIA_InnerLeft, 15, MUIA_InnerRight, 15, MUIA_InnerTop,
  15, MUIA_InnerBottom, 15, End, Child, RectangleObject, MUIA_Background,
  MUII_FILLBACK, MUIA_Frame, "D13333333", MUIA_FixHeight, 60, MUIA_InnerLeft,
  15, MUIA_InnerRight, 15, MUIA_InnerTop, 15, MUIA_InnerBottom, 15, End, End,

  Child, VSpace(10),

  /* Row 3: Nested rounded frames */
      Child, VGroup, MUIA_Frame, "D13333333", MUIA_Background, MUII_SHINEBACK,
  MUIA_InnerLeft, 15, MUIA_InnerRight, 15, MUIA_InnerTop, 15, MUIA_InnerBottom,
  15, Child, TextObject, MUIA_Text_Contents, "\33cNested Rounded Frame",
  MUIA_Font, MUIV_Font_Tiny, End, Child, VSpace(5), Child, HGroup, Child,
  RectangleObject, MUIA_Background, MUII_BACKGROUND, MUIA_Frame, "D03333333",
  MUIA_FixHeight, 40, MUIA_InnerLeft, 10, MUIA_InnerRight, 10, MUIA_InnerTop,
  10, MUIA_InnerBottom, 10, End, Child, RectangleObject, MUIA_Background,
  MUII_SHINE, MUIA_Frame, "D03333333", MUIA_FixHeight, 40, MUIA_InnerLeft, 10,
  MUIA_InnerRight, 10, MUIA_InnerTop, 10, MUIA_InnerBottom, 10, End, End, End,

  Child, VSpace(10),

  /* Row 4: Test with actual content */
      Child, HGroup, Child, VGroup, MUIA_Frame, "D13333333", MUIA_Background,
  MUII_REQUESTERBACK, MUIA_InnerLeft, 10, MUIA_InnerRight, 10, MUIA_InnerTop,
  10, MUIA_InnerBottom, 10, Child, TextObject, MUIA_Text_Contents,
  "Rounded frame\nwith text content\nand multiple lines", MUIA_Text_SetMax,
  FALSE, End, End, Child, VGroup, MUIA_Frame, "D03333333", MUIA_Background,
  MUII_BUTTONBACK, MUIA_InnerLeft, 10, MUIA_InnerRight, 10, MUIA_InnerTop, 10,
  MUIA_InnerBottom, 10, Child, TextObject, MUIA_Text_Contents,
  "Another rounded\nframe with down\nstate appearance", MUIA_Text_SetMax, FALSE,
  End, End, End,

  Child, VSpace(10), Child, HGroup, Child, HSpace(0), Child,
  SimpleButton("Close"), Child, HSpace(0), End, End, End, End;

  if (!app) {
    fprintf(stderr, "ERROR: can't create application object.\n");
    goto error;
  }
  printf("Created Rounded Frame Test Application object %p\n", app);

  DoMethod(mainWin, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
           MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

  /*
   * Open window and ALWAYS check.
   */
  set(mainWin, MUIA_Window_Open, TRUE);
  if (!XGET(mainWin, MUIA_Window_Open)) {
    MUI_DisposeObject(app);
    fprintf(stderr, "ERROR: can't open main window.\n");
    goto error;
  }

  printf("Window opened successfully. Testing rounded frames...\n");
  printf("Frame specifications used:\n");
  printf("  D13333333 = FST_ROUNDED up state\n");
  printf("  D03333333 = FST_ROUNDED down state\n");
  printf("Check that corner areas show parent background properly!\n");

  /*
   * Main event loop
   */
  {
    ULONG sigs = 0;

    while (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs) !=
           MUIV_Application_ReturnID_Quit) {
      if (sigs) {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
          break;
      }
    }
  }

  set(mainWin, MUIA_Window_Open, FALSE);
  MUI_DisposeObject(app);

  printf("Rounded Frame Test completed.\n");

error:
  closemuimaster();
  return 0;
}
