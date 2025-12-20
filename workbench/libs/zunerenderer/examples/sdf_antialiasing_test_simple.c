/*
    SDF Antialiasing Test - Simplified Zune Application
    Replicates the functionality of the HTML SDF test for the zunerenderer
   library

    Copyright (C) 2024, The AROS Development Team. All rights reserved.
*/

#include "proto/zunerenderer.h"
#include <exec/types.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <clib/alib_protos.h>
#include <libraries/mui.h>
#include <libraries/zunerenderer.h>

/* Application objects */
Object *app = NULL;
Object *wnd = NULL;
Object *canvas_area = NULL;
Object *corner_radius_slider = NULL;
Object *rect_width_slider = NULL;
Object *rect_height_slider = NULL;
Object *border_width_slider = NULL;
Object *antialias_checkbox = NULL;
Object *pixel_locking_checkbox = NULL;

/* Timing variables */
static ULONG frame_count = 0;
static double total_time = 0.0;
static double min_time = 999999.0;
static double max_time = 0.0;
static ULONG eclock_freq = 0;

/* Canvas dimensions */
#define CANVAS_WIDTH 400
#define CANVAS_HEIGHT 300

/* Test parameters */
static struct {
  float corner_radius;
  float rect_width;
  float rect_height;
  float border_width;
  BOOL antialias;
  BOOL pixel_locking;
  ULONG fill_color;
  ULONG border_color;
  ULONG bg_color;
} test_params = {
    .corner_radius = 15.0f,
    .rect_width = 200.0f,
    .rect_height = 150.0f,
    .border_width = 5.0f,
    .antialias = TRUE,
    .pixel_locking = TRUE,
    .fill_color = ZUNE_COLOR_RGB24(76, 175, 80),    /* Green */
    .border_color = ZUNE_COLOR_RGB24(33, 150, 243), /* Blue */
    .bg_color = ZUNE_COLOR_RGB24(0, 0, 0),          /* Purple */
};

/* Custom canvas area class */
struct CanvasData {
  struct MUI_AreaData mad;
  struct DrawingBoard *offscreen_buffer;
  struct RenderPort *offscreen_rp;
  struct RenderPort *screen_rp;
  struct ColorMap *colormap;
};

/* Global reference to canvas object for direct updates */
static Object *global_canvas_obj = NULL;

static IPTR CanvasNew(struct IClass *cl, Object *obj, struct opSet *msg) {
  struct CanvasData *data;

  if (!(obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg)))
    return 0;

  data = INST_DATA(cl, obj);

  /* Initialize render objects to NULL */
  data->offscreen_buffer = NULL;
  data->offscreen_rp = NULL;
  data->screen_rp = NULL;
  data->colormap = NULL;

  return (IPTR)obj;
}

static IPTR CanvasDispose(struct IClass *cl, Object *obj, Msg msg) {
  struct CanvasData *data = INST_DATA(cl, obj);

  /* Clean up render objects */
  if (data->offscreen_rp) {
    DestroyRenderPort(data->offscreen_rp);
    data->offscreen_rp = NULL;
  }
  if (data->screen_rp) {
    DestroyRenderPort(data->screen_rp);
    data->screen_rp = NULL;
  }
  if (data->offscreen_buffer) {
    DestroyDrawingBoard(data->offscreen_buffer);
    data->offscreen_buffer = NULL;
  }

  return DoSuperMethodA(cl, obj, msg);
}

static IPTR CanvasShow(struct IClass *cl, Object *obj, Msg msg) {
  struct CanvasData *data = INST_DATA(cl, obj);

  if (!DoSuperMethodA(cl, obj, msg))
    return FALSE;

  /* Get window and colormap */
  struct Window *window = NULL;
  Object *win_obj = (Object *)XGET(obj, MUIA_WindowObject);
  if (win_obj) {
    window = (struct Window *)XGET(win_obj, MUIA_Window_Window);
  }
  data->colormap = window ? window->WScreen->ViewPort.ColorMap : NULL;

  /* Create the offscreen buffer */
  data->offscreen_buffer =
      CreateDrawingBoard(CANVAS_WIDTH, CANVAS_HEIGHT, 32,
                         ZUNE_DRAWINGBOARD_HARDWARE | ZUNE_DRAWINGBOARD_CACHED);
  if (!data->offscreen_buffer) {
    printf("ERROR: Cannot create DrawingBoard\n");
    return FALSE;
  }

  /* Create RenderPorts */
  data->offscreen_rp = CreateRenderPortWithDrawingBoard(
      data->colormap, data->offscreen_buffer);
  data->screen_rp = CreateRenderPort(data->colormap, _rp(obj));

  if (!data->offscreen_rp || !data->screen_rp) {
    printf("ERROR: Cannot create RenderPorts\n");
    return FALSE;
  }

  /* Store global reference for direct updates */
  global_canvas_obj = obj;

  return TRUE;
}

static IPTR CanvasCleanup(struct IClass *cl, Object *obj, Msg msg) {
  struct CanvasData *data = INST_DATA(cl, obj);

  /* Clean up render objects */
  if (data->offscreen_rp) {
    DestroyRenderPort(data->offscreen_rp);
    data->offscreen_rp = NULL;
  }
  if (data->screen_rp) {
    DestroyRenderPort(data->screen_rp);
    data->screen_rp = NULL;
  }
  if (data->offscreen_buffer) {
    DestroyDrawingBoard(data->offscreen_buffer);
    data->offscreen_buffer = NULL;
  }

  /* Clear global reference */
  if (global_canvas_obj == obj) {
    global_canvas_obj = NULL;
  }

  return DoSuperMethodA(cl, obj, msg);
}

static IPTR CanvasAskMinMax(struct IClass *cl, Object *obj,
                            struct MUIP_AskMinMax *msg) {
  DoSuperMethodA(cl, obj, (Msg)msg);

  msg->MinMaxInfo->MinWidth = CANVAS_WIDTH;
  msg->MinMaxInfo->MinHeight = CANVAS_HEIGHT;
  msg->MinMaxInfo->DefWidth = CANVAS_WIDTH;
  msg->MinMaxInfo->DefHeight = CANVAS_HEIGHT;
  msg->MinMaxInfo->MaxWidth = CANVAS_WIDTH;
  msg->MinMaxInfo->MaxHeight = CANVAS_HEIGHT;

  return 0;
}

static IPTR CanvasDraw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg) {
  struct CanvasData *data = INST_DATA(cl, obj);

  DoSuperMethodA(cl, obj, (Msg)msg);

  if ((msg->flags & MADF_DRAWOBJECT) && data->offscreen_rp && data->screen_rp) {
    /* Set antialiasing quality */
    // ZuneSetAntialiasingQuality(data->offscreen_rp, 1);

    /* Clear background */
    ZuneDrawRectangleXYWH(data->offscreen_rp, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                          ZUNE_BRUSH_SOLID(test_params.bg_color));

    int rect_x = (CANVAS_WIDTH - test_params.rect_width) * 0.5;
    int rect_y = (CANVAS_HEIGHT - test_params.rect_height) * 0.5;
    ZuneDrawRectangleOutlineXYWH(
        data->offscreen_rp, rect_x, rect_y, test_params.rect_width,
        test_params.rect_height, test_params.border_color);

    /* Calculate rectangle position (centered) */
    // ZuneDrawRectangleRoundedOutlineStyledAAXYWH(data->offscreen_rp,
    //                                  (int)rect_x,
    //                                  (int)rect_y,
    //                                  (int)test_params.rect_width,
    //                                  (int)test_params.rect_height,
    //                                  test_params.corner_radius,
    //                                  test_params.border_width,
    //                                  test_params.border_color);

    /* Draw filled rounded rectangle using SDF rendering */
    // ZuneDrawRectangleRoundedAA(data->offscreen_rp,
    //                          (int)rect_x, (int)rect_y,
    //                          (int)test_params.rect_width,
    //                          (int)test_params.rect_height,
    //                          test_params.corner_radius,
    //                          test_params.fill_color);

    /* Blit offscreen buffer to screen */
    BlitDrawingBoardToRenderPortRects(data->offscreen_buffer, data->screen_rp,
                                      0, 0, _mleft(obj), _mtop(obj),
                                      CANVAS_WIDTH, CANVAS_HEIGHT);
  }

  return 0;
}

static IPTR CanvasDispatcher(struct IClass *cl, Object *obj, Msg msg) {
  switch (msg->MethodID) {
  case OM_NEW:
    return CanvasNew(cl, obj, (struct opSet *)msg);
  case OM_DISPOSE:
    return CanvasDispose(cl, obj, msg);
  case MUIM_Show:
    return CanvasShow(cl, obj, msg);
  case MUIM_Cleanup:
    return CanvasCleanup(cl, obj, msg);
  case MUIM_AskMinMax:
    return CanvasAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
  case MUIM_Draw:
    return CanvasDraw(cl, obj, (struct MUIP_Draw *)msg);
  }
  return DoSuperMethodA(cl, obj, msg);
}

static struct MUI_CustomClass *canvas_class = NULL;

static void create_canvas_class(void) {
  if (!canvas_class) {
    canvas_class = MUI_CreateCustomClass(
        NULL, MUIC_Area, NULL, sizeof(struct CanvasData), CanvasDispatcher);
  }
}

static void delete_canvas_class(void) {
  if (canvas_class) {
    MUI_DeleteCustomClass(canvas_class);
    canvas_class = NULL;
  }
}

/* Update canvas rendering */
static void update_canvas() {
  struct timeval tv_start, tv_end;
  LONG elapsed_time;

  CurrentTime(&tv_start.tv_secs, &tv_start.tv_micro);

  if (global_canvas_obj) {
    struct CanvasData *data =
        INST_DATA(canvas_class->mcc_Class, global_canvas_obj);

    if (data && data->offscreen_rp && data->screen_rp &&
        data->offscreen_buffer) {
      int left = _mleft(global_canvas_obj);
      int top = _mtop(global_canvas_obj);

      struct RenderPort *current_renderport = data->screen_rp;
      if (test_params.pixel_locking) {
        current_renderport = data->offscreen_rp;
        LockDrawingBoardPixels(current_renderport,
                               &data->offscreen_buffer->pitch);

        left = 0;
        top = 0;
      }
      ZuneDrawRectangleXYWH(current_renderport, left, top, CANVAS_WIDTH,
                            CANVAS_HEIGHT,
                            ZUNE_BRUSH_SOLID(test_params.bg_color));

      /* Calculate rectangle position (centered) */
      float rect_x = (CANVAS_WIDTH - test_params.rect_width) + left;
      float rect_y = (CANVAS_HEIGHT - test_params.rect_width) + top;

      if (test_params.antialias) {
        /* Draw antialiased rounded rectangle with border */
        // ZuneDrawRectangleRoundedOutlineStyledAAXYWH(current_renderport,
        //                                  (int)rect_x,
        //                                  (int)rect_y,
        //                                  (int)test_params.rect_width,
        //                                  (int)test_params.rect_height,
        //                                  test_params.corner_radius,
        //                                  test_params.border_width,
        //                                  test_params.border_color);
        ZuneDrawLineAAPoints(current_renderport, rect_x, rect_y, rect_x + 10,
                             rect_y + 100, test_params.fill_color);
        // ZuneFillCircleStyledAAAt(current_renderport, (int)rect_x,
        // (int)rect_y, test_params.rect_width, test_params.border_width,
        // test_params.fill_color, test_params.border_color);
      } else {
        /* Draw non-antialiased rectangle outline */
        ZuneDrawRectangleOutlineStyledXYWH(
            current_renderport, rect_x, rect_y, test_params.rect_width,
            test_params.rect_height, test_params.border_width,
            test_params.border_color);
      }
      if (test_params.pixel_locking) {
        UnlockDrawingBoardPixels(current_renderport);
        /* Blit offscreen buffer to screen */
        BlitDrawingBoardToRenderPortRects(
            data->offscreen_buffer, data->screen_rp, 0, 0,
            _mleft(global_canvas_obj), _mtop(global_canvas_obj), CANVAS_WIDTH,
            CANVAS_HEIGHT);
      }
    }
  }

  CurrentTime(&tv_end.tv_secs, &tv_end.tv_micro);
  elapsed_time = (tv_end.tv_sec - tv_start.tv_sec) * 1000000 + tv_end.tv_micro -
                 tv_start.tv_micro;

  frame_count++;
  total_time += elapsed_time;

  if (elapsed_time < min_time) {
    min_time = elapsed_time;
  }
  if (elapsed_time > max_time) {
    max_time = elapsed_time;
  }

  // if (frame_count % 60 == 0) {  /* Print stats every 60 frames */
  printf(
      "Frame %lu: Current: %ld us, Avg: %.2f us, Min: %.2f us, Max: %.2f us\n",
      frame_count, elapsed_time, total_time / frame_count, min_time, max_time);
  // }
}

/* GUI initialization */
static void init_gui(void) {
  create_canvas_class();

  app = ApplicationObject, MUIA_Application_Title, "SDF Test",
  MUIA_Application_Version, "$VER: SDF Test 1.0",

  SubWindow, wnd = WindowObject, MUIA_Window_Title, "SDF Antialiasing Test",
  MUIA_Window_ID, MAKE_ID('S', 'D', 'F', 'T'),

  WindowContents, HGroup,
  /* Canvas area */
      Child, VGroup, Child, TextObject, MUIA_Text_Contents, "Preview",
  MUIA_Text_PreParse, "\33c\33u", MUIA_Frame, MUIV_Frame_Text, End, Child,
  canvas_area = NewObject(canvas_class->mcc_Class, NULL, TAG_END), End,

  /* Controls */
      Child, VGroup, Child, TextObject, MUIA_Text_Contents, "Controls",
  MUIA_Text_PreParse, "\33c\33u", MUIA_Frame, MUIV_Frame_Text, End,

  Child, HGroup, Child, TextObject, MUIA_Text_Contents, "Corner:", MUIA_Frame,
  MUIV_Frame_None, End, Child, corner_radius_slider = SliderObject,
  MUIA_Numeric_Min, 0, MUIA_Numeric_Max, 50, MUIA_Numeric_Value,
  (LONG)test_params.corner_radius, End, End,

  Child, HGroup, Child, TextObject, MUIA_Text_Contents, "Width:", MUIA_Frame,
  MUIV_Frame_None, End, Child, rect_width_slider = SliderObject,
  MUIA_Numeric_Min, 50, MUIA_Numeric_Max, 350, MUIA_Numeric_Value,
  (LONG)test_params.rect_width, End, End,

  Child, HGroup, Child, TextObject, MUIA_Text_Contents, "Height:", MUIA_Frame,
  MUIV_Frame_None, End, Child, rect_height_slider = SliderObject,
  MUIA_Numeric_Min, 50, MUIA_Numeric_Max, 250, MUIA_Numeric_Value,
  (LONG)test_params.rect_height, End, End,

  Child, HGroup, Child, TextObject, MUIA_Text_Contents, "Border:", MUIA_Frame,
  MUIV_Frame_None, End, Child, border_width_slider = SliderObject,
  MUIA_Numeric_Min, 0, MUIA_Numeric_Max, 20, MUIA_Numeric_Value,
  (LONG)test_params.border_width, End, End,

  Child, HGroup, Child,
  antialias_checkbox = MUI_MakeObject(MUIO_Checkmark, NULL), Child, TextObject,
  MUIA_Text_Contents, "Antialiasing", MUIA_Frame, MUIV_Frame_None, End, Child,
  HSpace(0), End,

  Child, HGroup, Child,
  pixel_locking_checkbox = MUI_MakeObject(MUIO_Checkmark, NULL), Child,
  TextObject, MUIA_Text_Contents, "Pixel Locking", MUIA_Frame, MUIV_Frame_None,
  End, Child, HSpace(0), End,

  Child, VSpace(0), End, End, End, End;

  if (app) {
    /* Window close notification */
    DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    /* Slider notifications */
    DoMethod(corner_radius_slider, MUIM_Notify, MUIA_Numeric_Value,
             MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, 1);
    DoMethod(rect_width_slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, 2);
    DoMethod(rect_height_slider, MUIM_Notify, MUIA_Numeric_Value,
             MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, 3);
    DoMethod(border_width_slider, MUIM_Notify, MUIA_Numeric_Value,
             MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, 4);
    DoMethod(antialias_checkbox, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, 5);
    DoMethod(pixel_locking_checkbox, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, 6);

    /* Set initial checkbox states */
    set(antialias_checkbox, MUIA_Selected, test_params.antialias);
    set(pixel_locking_checkbox, MUIA_Selected, test_params.pixel_locking);

    set(wnd, MUIA_Window_Open, TRUE);
  }
}

static void deinit_gui(void) {
  if (wnd)
    set(wnd, MUIA_Window_Open, FALSE);
  if (app)
    MUI_DisposeObject(app);
  delete_canvas_class();
}

int main(void) {
  LONG returnid;
  LONG running = TRUE;

  printf("SDF Antialiasing Test - Simplified Version\n");
  printf("==========================================\n");

  init_gui();
  if (!app) {
    printf("ERROR: Failed to create application\n");
    return 20;
  }

  printf("Application created successfully!\n");
  printf("Use the sliders to adjust SDF parameters.\n\n");

  while (running) {
    returnid = DoMethod(app, MUIM_Application_NewInput, (IPTR)&running);

    if (running) {
      switch (returnid) {
      case MUIV_Application_ReturnID_Quit:
        running = FALSE;
        break;

      case 1: /* Corner radius */
      {
        LONG level;
        get(corner_radius_slider, MUIA_Numeric_Value, &level);
        test_params.corner_radius = (float)level;
        printf("Corner radius: %.1f\n", test_params.corner_radius);
        update_canvas();
      } break;

      case 2: /* Rectangle width */
      {
        LONG level;
        get(rect_width_slider, MUIA_Numeric_Value, &level);
        test_params.rect_width = (float)level;
        printf("Width: %.1f\n", test_params.rect_width);
        update_canvas();
      } break;

      case 3: /* Rectangle height */
      {
        LONG level;
        get(rect_height_slider, MUIA_Numeric_Value, &level);
        test_params.rect_height = (float)level;
        printf("Height: %.1f\n", test_params.rect_height);
        update_canvas();
      } break;

      case 4: /* Border width */
      {
        LONG level;
        get(border_width_slider, MUIA_Numeric_Value, &level);
        test_params.border_width = (float)level;
        printf("Border width: %.1f\n", test_params.border_width);
        update_canvas();
      } break;

      case 5: /* Antialiasing checkbox */
      {
        LONG selected;
        get(antialias_checkbox, MUIA_Selected, &selected);
        test_params.antialias = (BOOL)selected;
        printf("Antialiasing: %s\n", test_params.antialias ? "ON" : "OFF");
        update_canvas();
      } break;

      case 6: /* Pixel locking checkbox */
      {
        LONG selected;
        get(pixel_locking_checkbox, MUIA_Selected, &selected);
        test_params.pixel_locking = (BOOL)selected;
        printf("Pixel Locking: %s\n", test_params.pixel_locking ? "ON" : "OFF");
        update_canvas();
      } break;
      }
    }

    if (running && returnid == MUIV_Application_ReturnID_Quit)
      running = FALSE;
  }

  printf("Shutting down...\n");
  deinit_gui();
  return 0;
}
