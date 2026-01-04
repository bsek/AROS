/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Locked DrawingBoard Rectangle Demo

    This demo shows how the CybergfxDrawRectangle function works with
    locked DrawingBoards, demonstrating the direct pixel manipulation
    path for optimal performance when doing many drawing operations.
*/

#include "clib/exec_protos.h"

#include <cybergraphx/cybergraphics.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunerenderer.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG 0
#include <aros/debug.h>

#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/zunerenderer.h>

/* Demo parameters */
#define DEMO_WIDTH 640
#define DEMO_HEIGHT 480
#define DEMO_DEPTH 32

/* Global variables */
struct Library *ZuneRendererBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

struct Screen *screen = NULL;
struct Window *window = NULL;
struct DrawingBoard *demo_board = NULL;
struct RenderPort *demo_rp = NULL;
struct RenderPort *window_rp = NULL;

/* Function prototypes */
BOOL InitDemo(void);
void CleanupDemo(void);
void DemoLockedRectangles();
void DemoUnlockedRectangles();
void DemoLockedAARectangles();
void DemoUnlockedAARectangles();
void DemoLockedCircles();
void DemoUnlockedCircles();
void DemoUnlockedAACircles();
void DemoLockedAACircles();
void DemoLockedLines();
void DemoUnlockedLines();
void DemoLockedAALines();
void DemoUnlockedAALines();
void DemoTextures();
void DemoTexturesTiled();
void ShowResults(const char *label);
void PerformanceTest();

static ULONG SampleDrawingBoardPixel(WORD x, WORD y);
static void DebugDumpDrawingBoardPixel(const char *label, WORD x, WORD y);

static ULONG SampleDrawingBoardPixel(WORD x, WORD y) {
  if (!demo_board)
    return 0;

  if (demo_board->pixels_locked && demo_rp) {
    struct ZunePoint point;
    point.x = x;
    point.y = y;
    return GetPixel(demo_rp, &point);
  }

  if (demo_board->rastport) {
    UBYTE px[4] = {0};
    struct RastPort *rp = demo_board->rastport;
    if (ReadPixelArray(px, 0, 0, sizeof(px), rp, x, y, 1, 1, RECTFMT_ARGB) ==
        1) {
      return ((ULONG)px[0] << 24) | ((ULONG)px[1] << 16) |
             ((ULONG)px[2] << 8) | (ULONG)px[3];
    }
    return ReadPixel(rp, x, y);
  }

  return 0;
}

static void DebugDumpDrawingBoardPixel(const char *label, WORD x, WORD y) {
  ULONG color = SampleDrawingBoardPixel(x, y);
  printf("  [debug] DrawingBoard %s (%d,%d) = 0x%08lx\n",
         label ? label : "sample", x, y, color);
}

int main(void) {
  printf("Zune Renderer - DrawingBoard Rectangle, Circle, and Line Demo\n");
  printf("=============================================================\n\n");

  if (!InitDemo()) {
    printf("ERROR: Failed to initialize demo\n");
    CleanupDemo();
    return 1;
  }

  printf("Demo initialized successfully\n");

  /* Test unlocked rectangle drawing - works with both CyberGfx and OpenGL backends */
  printf("\n1. Testing unlocked DrawingBoard rectangle drawing...\n");
  ClearRenderPort(demo_rp, ZUNE_DARKGRAY);
  DemoUnlockedRectangles();
  /* Show results */
  ShowResults("Unlocked rectangles");
  getchar();

  /* Test unlocked anti-aliased rectangle drawing */
  printf("\n2. Testing unlocked DrawingBoard anti-aliased rectangle drawing...\n");
  printf("  Calling ClearRenderPort(demo_rp, ZUNE_DARKGRAY)...\n");
  ClearRenderPort(demo_rp, ZUNE_DARKGRAY);
  printf("  ClearRenderPort done, calling DemoUnlockedAARectangles...\n");
  DemoUnlockedAARectangles();
  printf("  DemoUnlockedAARectangles done, calling ShowResults...\n");
  ShowResults("Unlocked AA rectangles");
  getchar();

  /* Test unlocked circle drawing */
  printf("\n3. Testing unlocked DrawingBoard circle drawing...\n");
  ClearRenderPort(demo_rp, ZUNE_DARKGRAY);
  DemoUnlockedCircles();
  ShowResults("Unlocked circles");
  getchar();

  /* Test unlocked anti-aliased circle drawing */
  printf("\n4. Testing unlocked DrawingBoard anti-aliased circle drawing...\n");
  ClearRenderPort(demo_rp, ZUNE_DARKGRAY);
  DemoUnlockedAACircles();
  ShowResults("Unlocked AA circles");
  getchar();

  /* Test unlocked line drawing */
  printf("\n5. Testing unlocked DrawingBoard line drawing...\n");
  ClearRenderPort(demo_rp, ZUNE_DARKGRAY);
  DemoUnlockedLines();
  ShowResults("Unlocked lines");
  getchar();

  /* Test unlocked anti-aliased line drawing */
  printf("\n6. Testing unlocked DrawingBoard anti-aliased line drawing...\n");
  ClearRenderPort(demo_rp, ZUNE_DARKGRAY);
  DemoUnlockedAALines();
  ShowResults("Unlocked AA lines");
  getchar();

  printf("\nDemo completed successfully. Press ENTER to exit.\n");
  getchar();

  CleanupDemo();
  return 0;
}

BOOL InitDemo(void) {
  /* Open required libraries */
  ZuneRendererBase = OpenLibrary("zunerenderer.library", 1);
  if (!ZuneRendererBase) {
    printf("ERROR: Cannot open zunerenderer.library\n");
    return FALSE;
  }

  CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
  if (!CyberGfxBase) {
    printf("ERROR: Cannot open cybergraphics.library\n");
    return FALSE;
  }

  IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
  if (!IntuitionBase) {
    printf("ERROR: Cannot open intuition.library\n");
    return FALSE;
  }

  GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
  if (!GfxBase) {
    printf("ERROR: Cannot open graphics.library\n");
    return FALSE;
  }

  /* Open screen for display */
  screen = LockPubScreen(
      NULL); /* NULL means the default public screen, which is Workbench */
  if (!screen) {
    printf("ERROR: Cannot lock Workbench screen\n");
    return FALSE;
  }

  /* Open window */
  window = OpenWindowTags(
      NULL, WA_CustomScreen, (IPTR)screen, WA_Left, 0, WA_Top, 0, WA_Width,
      DEMO_WIDTH, WA_Height, DEMO_HEIGHT, WA_Title,
      (IPTR) "Rectangle and Circle Drawing Demo", WA_DragBar, TRUE,
      WA_CloseGadget, TRUE, WA_IDCMP, IDCMP_CLOSEWINDOW, TAG_DONE);

  if (!window) {
    printf("ERROR: Cannot open window\n");
    return FALSE;
  }

  /* Create RenderPort bound to window first (new API) */
  window_rp = CreateRenderPortForWindow(window, screen->ViewPort.ColorMap);
  if (!window_rp) {
    printf("ERROR: Cannot create Window RenderPort\n");
    return FALSE;
  }

  /* Create DrawingBoard under the RenderPort */
  demo_board = CreateDrawingBoardForRenderPort(window_rp, DEMO_WIDTH, DEMO_HEIGHT);
  if (!demo_board) {
    printf("ERROR: Cannot create DrawingBoard\n");
    return FALSE;
  }

  /* demo_rp is now the same as window_rp, use ZuneSetTarget to switch */
  demo_rp = window_rp;

  /* Switch to DrawingBoard for offscreen rendering */
  ZuneSetTarget(demo_rp, demo_board);

  /* Clear the DrawingBoard */
  ClearDrawingBoard(demo_rp, ZUNE_DARKGRAY);

  return TRUE;
}

void CleanupDemo(void) {
  if (demo_rp) {
    DestroyRenderPort(demo_rp);
    demo_rp = NULL;
  }

  if (demo_board) {
    DestroyDrawingBoard(demo_board);
    demo_board = NULL;
  }

  if (window) {
    CloseWindow(window);
    window = NULL;
  }

  if (screen) {
    UnlockPubScreen(NULL, screen);
    screen = NULL;
  }

  if (ZuneRendererBase) {
    CloseLibrary(ZuneRendererBase);
    ZuneRendererBase = NULL;
  }

  if (CyberGfxBase) {
    CloseLibrary(CyberGfxBase);
    CyberGfxBase = NULL;
  }

  if (IntuitionBase) {
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
  }

  if (GfxBase) {
    CloseLibrary((struct Library *)GfxBase);
    GfxBase = NULL;
  }
}

void DemoLockedRectangles(void) {
  APTR pixels;
  ULONG pitch;

  printf("  Locking DrawingBoard pixels...\n");

  /* Lock the DrawingBoard for direct pixel access */
  pixels = LockDrawingBoardPixels(demo_rp, &pitch);
  if (!pixels) {
    printf("  ERROR: Cannot lock DrawingBoard pixels\n");
    return;
  }

  printf("  Pixels locked successfully (address: %p, pitch: %u)\n", pixels,
         pitch);
  printf("  Drawing rectangles using direct pixel manipulation...\n");

  /* Draw various rectangles while locked */
  /* Regular filled rectangles */
  ZuneDrawRectangleXYWH(demo_rp, 50, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneDrawRectangleXYWH(demo_rp, 170, 50, 100, 80,
                        ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneDrawRectangleXYWH(demo_rp, 290, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

  /* Regular outlined rectangles */
  ZuneDrawRectangleOutlineXYWH(demo_rp, 50, 150, 100, 80, ZUNE_YELLOW);
  ZuneDrawRectangleOutlineXYWH(demo_rp, 170, 150, 100, 80, ZUNE_MAGENTA);
  ZuneDrawRectangleOutlineStyledXYWH(demo_rp, 290, 150, 100, 80, 5.0,
                                     ZUNE_CYAN);

  /* Rounded filled rectangles */
  ZuneDrawRectangleRoundedXYWH(demo_rp, 50, 250, 100, 80, 15.0f,
                               ZUNE_BRUSH_SOLID(ZUNE_WHITE));
  ZuneDrawRectangleRoundedXYWH(demo_rp, 170, 250, 100, 80, 20.0f,
                               ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
  ZuneDrawRectangleRoundedXYWH(demo_rp, 290, 250, 100, 80, 25.0f,
                               ZUNE_BRUSH_SOLID(ZUNE_GRAY));

  /* Rounded outlined rectangles */
  ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 50, 350, 100, 80, 10.0f,
                                      ZUNE_RED);
  ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 170, 350, 100, 80, 15.0f,
                                      ZUNE_GREEN);
  ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 290, 350, 100, 80, 20.0f,
                                      ZUNE_BLUE);

  /* Overlapping rectangles to test bounds checking */
  ZuneDrawRectangleXYWH(demo_rp, 420, 100, 150, 100,
                        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawRectangleRoundedXYWH(
      demo_rp, 450, 130, 150, 100, 30.0f,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 0, 255, 255)));

  /* Rounded rectangle with gradient fill */
  struct ZuneGradientStop locked_gradient_stops[] = {
      {0.0f, ZUNE_COLOR_ARGB32(255, 255, 128, 0)},
      {0.5f, ZUNE_COLOR_ARGB32(255, 255, 0, 128)},
      {1.0f, ZUNE_COLOR_ARGB32(255, 128, 0, 255)},
  };

  struct ZuneBrush locked_gradient_brush = {
      .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
      .flags = 0,
      .internal = {0},
      .data = {.linear = {.start = ZUNE_POINT_LITERAL(0, 0),
                          .end = ZUNE_POINT_LITERAL(180, 0),
                          .stops = locked_gradient_stops,
                          .stop_count = (UWORD)(sizeof(locked_gradient_stops) /
                                                sizeof(locked_gradient_stops[0]))}}};

  ZuneDrawRectangleRoundedXYWH(demo_rp, 420, 250, 180, 80, 20.0f,
                               &locked_gradient_brush);

  /* Rounded rectangle with texture fill */
  const ULONG locked_pattern_width = 32;
  const ULONG locked_pattern_height = 32;
  const ULONG locked_pattern_pitch = locked_pattern_width * 4;
  const ULONG locked_pattern_size = locked_pattern_height * locked_pattern_pitch;
  APTR locked_pattern_pixels = AllocMem(locked_pattern_size, MEMF_PUBLIC);
  struct ZuneTexture *locked_pattern_texture = NULL;

  if (locked_pattern_pixels) {
    ULONG *pix = (ULONG *)locked_pattern_pixels;
    for (ULONG row = 0; row < locked_pattern_height; ++row) {
      for (ULONG col = 0; col < locked_pattern_width; ++col) {
        BOOL checker = (((row / 8) + (col / 8)) & 1) != 0;
        ULONG color = checker ? ZUNE_COLOR_ARGB32(255, 200, 50, 50)
                              : ZUNE_COLOR_ARGB32(255, 50, 200, 50);
        pix[row * locked_pattern_width + col] = color;
      }
    }

    locked_pattern_texture =
        CreateTextureFromData(locked_pattern_pixels, locked_pattern_width,
                              locked_pattern_height, 32,
                              ZUNE_TEXTURE_FORMAT_ARGB32, locked_pattern_pitch,
                              ZUNE_TEXTURE_WRAPPING);
  }

  if (locked_pattern_texture) {
    struct ZuneBrush locked_pattern_brush = {
        .type = ZUNE_BRUSH_TYPE_TEXTURE,
        .flags = 0,
        .internal = {0},
        .data = {.texture = {.texture = locked_pattern_texture,
                             .source = {0, 0, locked_pattern_width, locked_pattern_height},
                             .wrap_u = ZUNE_BRUSH_WRAP_REPEAT,
                             .wrap_v = ZUNE_BRUSH_WRAP_REPEAT,
                             .filter = ZUNE_BRUSH_FILTER_NEAREST}}};

    ZuneDrawRectangleRoundedXYWH(demo_rp, 420, 350, 180, 80, 20.0f,
                                 &locked_pattern_brush);
  } else {
    printf("  WARNING: Could not create pattern texture for locked demo\n");
  }

  if (locked_pattern_texture) {
    DestroyTexture(locked_pattern_texture);
  }
  if (locked_pattern_pixels) {
    FreeMem(locked_pattern_pixels, locked_pattern_size);
  }

  /* Rounded rectangles with both fill and border */
  /* Solid fill with border */
  ZuneDrawRectangleRoundedStyledXYWH(demo_rp, 620, 50, 140, 80, 15, 3,
                                     ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), ZUNE_RED);

  /* Gradient fill with border */
  struct ZuneGradientStop styled_gradient_stops[] = {
      {0.0f, ZUNE_COLOR_ARGB32(255, 100, 150, 255)},
      {1.0f, ZUNE_COLOR_ARGB32(255, 50, 100, 200)},
  };

  struct ZuneBrush styled_gradient_brush = {
      .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
      .flags = 0,
      .internal = {0},
      .data = {.linear = {.start = ZUNE_POINT_LITERAL(0, 0),
                          .end = ZUNE_POINT_LITERAL(0, 80),
                          .stops = styled_gradient_stops,
                          .stop_count = 2}}};

  ZuneDrawRectangleRoundedStyledXYWH(demo_rp, 620, 150, 140, 80, 20, 4,
                                     &styled_gradient_brush, ZUNE_WHITE);

  /* Another solid fill with thick border */
  ZuneDrawRectangleRoundedStyledXYWH(demo_rp, 620, 250, 140, 80, 25, 1,
                                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 255, 200, 100)),
                                     ZUNE_COLOR_ARGB32(255, 150, 100, 50));

  printf("  All rectangles drawn using locked pixel access\n");

  /* Unlock the pixels */
  UnlockDrawingBoardPixels(demo_rp);

  printf("  Pixels unlocked\n");
}

void DemoUnlockedRectangles(void) {
  printf("  Drawing rectangles using standard rastport operations...\n");

  /* Draw the same pattern but without locking (will use FillPixelArray) */
  /* Regular filled rectangles */
  ZuneDrawRectangleXYWH(demo_rp, 50, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneDrawRectangleXYWH(demo_rp, 170, 50, 100, 80,
                        ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneDrawRectangleXYWH(demo_rp, 290, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

  /* Regular outlined rectangles */
  ZuneDrawRectangleOutlineXYWH(demo_rp, 50, 150, 100, 80, ZUNE_YELLOW);
  ZuneDrawRectangleOutlineXYWH(demo_rp, 170, 150, 100, 80, ZUNE_MAGENTA);
  ZuneDrawRectangleOutlineStyledXYWH(demo_rp, 290, 150, 100, 80, 5, ZUNE_CYAN);

  // /* Rounded filled rectangles */
  ZuneDrawRectangleRoundedXYWH(demo_rp, 50, 250, 100, 80, 15.0f,
                               ZUNE_BRUSH_SOLID(ZUNE_WHITE));
  ZuneDrawRectangleRoundedXYWH(demo_rp, 170, 250, 100, 80, 20.0f,
                               ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
  ZuneDrawRectangleRoundedXYWH(demo_rp, 290, 250, 100, 80, 25.0f,
                               ZUNE_BRUSH_SOLID(ZUNE_GRAY));

  // /* Rounded outlined rectangles */
  ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 50, 350, 100, 80, 10.0f,
                                      ZUNE_RED);
  ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 170, 350, 100, 80, 15.0f,
                                      ZUNE_GREEN);
  ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 290, 350, 100, 80, 20.0f,
                                      ZUNE_BLUE);

  // /* Overlapping rectangles */
  ZuneDrawRectangleXYWH(demo_rp, 420, 100, 150, 100,
                        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawRectangleRoundedXYWH(
      demo_rp, 450, 130, 150, 100, 30.0f,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 0, 255, 255)));

  /* Gradient brush test */
  static const struct ZuneGradientStop gradient_stops[] = {
      {0.0f, ZUNE_COLOR_ARGB32(255, 255, 0, 0)},
      {0.5f, ZUNE_COLOR_ARGB32(255, 255, 255, 0)},
      {1.0f, ZUNE_COLOR_ARGB32(255, 0, 0, 255)},
  };

  struct ZuneBrush gradient_brush = {
      .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
      .flags = 0,
      .internal = {0},
      .data = {.linear = {.start = ZUNE_POINT_LITERAL(0, 0),
                          .end = ZUNE_POINT_LITERAL(0, 180),
                          .stops = gradient_stops,
                          .stop_count = (UWORD)(sizeof(gradient_stops) /
                                                sizeof(gradient_stops[0]))}}};

  ZuneDrawRectangleRoundedXYWH(demo_rp, 420, 260, 180, 80, 28.0f,
                               &gradient_brush);
  ZuneDrawRectangleXYWH(demo_rp, 630, 260, 140, 80, &gradient_brush);

  /* Pattern/texture brush test */
  const ULONG pattern_width = 32;
  const ULONG pattern_height = 32;
  const ULONG pattern_pitch = pattern_width * 4;
  const ULONG pattern_size = pattern_height * pattern_pitch;
  APTR pattern_pixels = AllocMem(pattern_size, MEMF_PUBLIC);
  struct ZuneTexture *pattern_texture = NULL;

  if (pattern_pixels) {
    ULONG *pixels = (ULONG *)pattern_pixels;
    for (ULONG row = 0; row < pattern_height; ++row) {
      for (ULONG col = 0; col < pattern_width; ++col) {
        BOOL checker = (((row / 4) + (col / 4)) & 1) != 0;
        ULONG color = checker ? ZUNE_COLOR_ARGB32(255, 40, 40, 140)
                              : ZUNE_COLOR_ARGB32(255, 240, 240, 240);
        pixels[row * pattern_width + col] = color;
      }
    }

    pattern_texture =
        CreateTextureFromData(pattern_pixels, pattern_width, pattern_height, 32,
                              ZUNE_TEXTURE_FORMAT_ARGB32, pattern_pitch,
                              ZUNE_TEXTURE_WRAPPING);
  }

  if (pattern_texture) {
    struct ZuneBrush pattern_brush = {
        .type = ZUNE_BRUSH_TYPE_TEXTURE,
        .flags = 0,
        .internal = {0},
        .data = {.texture = {.texture = pattern_texture,
                             .source = {0, 0, pattern_width, pattern_height},
                             .wrap_u = ZUNE_BRUSH_WRAP_REPEAT,
                             .wrap_v = ZUNE_BRUSH_WRAP_REPEAT,
                             .filter = ZUNE_BRUSH_FILTER_NEAREST}}};

    ZuneDrawRectangleRoundedXYWH(demo_rp, 420, 360, 180, 80, 20.0f,
                                 &pattern_brush);
  } else {
    printf("  WARNING: Could not create pattern texture for demo\n");
  }

  if (pattern_texture) {
    DestroyTexture(pattern_texture);
  }
  if (pattern_pixels) {
    FreeMem(pattern_pixels, pattern_size);
  }

  printf("  All rectangles drawn using standard operations\n");
}

void DemoUnlockedAARectangles(void) {
  printf("  Drawing AA rectangles using standard rastport operations...\n");

  /* Draw various anti-aliased rectangles while locked */
  /* Rounded filled rectangles with AA */
  ZuneFillRectangleRoundedAAXYWH(demo_rp, 50, 50, 100, 80, 5,
                                 ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneFillRectangleRoundedAAXYWH(demo_rp, 170, 50, 100, 80, 20,
                                 ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneFillRectangleRoundedAAXYWH(demo_rp, 290, 50, 100, 80, 25, &ZUNE_BRUSH_LITERAL_PEN(1));

  /* Rounded outlined rectangles with AA */
  ZuneDrawRectangleRoundedOutlineAAXYWH(demo_rp, 50, 150, 100, 80, 5,
                                        ZUNE_LIGHTGRAY);
  ZuneDrawRectangleRoundedOutlineAAXYWH(demo_rp, 170, 150, 100, 80, 15,
                                        ZUNE_MAGENTA);
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 290, 150, 100, 80, 20, 5, ZUNE_CYAN);

  /* Rounded filled rectangles with border (styled AA) */
  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 50, 250, 100, 80, 15, 1,
                                       ZUNE_BRUSH_SOLID(ZUNE_WHITE),
                                       ZUNE_LIGHTGRAY);
  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 170, 250, 100, 80, 20, 20, ZUNE_BRUSH_SOLID(ZUNE_YELLOW), ZUNE_LIGHTGRAY);
  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 290, 250, 100, 80, 25, 1, ZUNE_BRUSH_SOLID(ZUNE_GRAY), ZUNE_RED);

  /* Gradient AA test rectangle */
  static const struct ZuneGradientStop aa_gradient_stops[] = {
      {0.0f, ZUNE_COLOR_ARGB32(255, 0, 128, 255)},
      {0.5f, ZUNE_COLOR_ARGB32(255, 0, 255, 128)},
      {1.0f, ZUNE_COLOR_ARGB32(255, 128, 255, 0)},
  };

  struct ZuneBrush aa_gradient_brush = {
      .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
      .flags = 0,
      .internal = {0},
      .data = {.linear = {.start = ZUNE_POINT_LITERAL(0, 0),
                          .end = ZUNE_POINT_LITERAL(180, 0),
                          .stops = aa_gradient_stops,
                          .stop_count =
                              (UWORD)(sizeof(aa_gradient_stops) /
                                      sizeof(aa_gradient_stops[0]))}}};

  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 420, 420, 180, 80, 18, 5,
                                 &aa_gradient_brush, ZUNE_RED);

  /* More rounded outlined rectangles with different line widths */
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 50, 350, 100, 80, 10, 1,
                                              ZUNE_RED);
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 170, 350, 100, 80, 15, 3,
                                              ZUNE_GREEN);
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 290, 350, 100, 80, 20, 4,
                                              ZUNE_BLUE);

  /* Overlapping AA rectangles to show smooth blending */
  // ZuneFillRectangleRoundedAAXYWH(
  //     demo_rp, 420, 100, 150, 100, 30,
  //     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(
      demo_rp, 450, 130, 150, 100, 35, 5,
      /*ZUNE_COLOR_ARGB32(128, 0, 255, 255),*/
      ZUNE_COLOR_ARGB32(255, 255, 0, 0));

  // /* Small AA rectangles to show edge smoothing */
  // ZuneFillRectangleRoundedAAXYWH(
  //     demo_rp, 420, 280, 40, 30, 8,
  //     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(
      demo_rp, 470, 290, 35, 25, 6, 2, ZUNE_COLOR_ARGB32(200, 64, 255, 128));
  ZuneFillRectangleRoundedStyledAAXYWH(
      demo_rp, 515, 300, 45, 35, 10, 3,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
      ZUNE_COLOR_ARGB32(255, 255, 255, 255));

  printf("  All AA rectangles drawn using unlocked pixel access\n");

  printf("  Pixels unlocked\n");
}

void DemoLockedAARectangles(void) {
  APTR pixels;
  ULONG pitch;

  printf("  Locking DrawingBoard pixels...\n");

  /* Lock the DrawingBoard for direct pixel access */
  pixels = LockDrawingBoardPixels(demo_rp, &pitch);
  if (!pixels) {
    printf("  ERROR: Cannot lock DrawingBoard pixels\n");
    return;
  }

  printf("  Pixels locked successfully (address: %p, pitch: %u)\n", pixels,
         pitch);
  printf("  Drawing AA rectangles using direct pixel manipulation...\n");

  /* Draw various anti-aliased rectangles while locked */
  /* Rounded filled rectangles with AA */
  ZuneFillRectangleRoundedAAXYWH(demo_rp, 50, 50, 100, 80, 15,
                                 ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneFillRectangleRoundedAAXYWH(demo_rp, 170, 50, 100, 80, 20,
                                 ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneFillRectangleRoundedAAXYWH(demo_rp, 290, 50, 100, 80, 25,
                                 ZUNE_BRUSH_SOLID(ZUNE_BLACK));

  /* Rounded outlined rectangles with AA */
  ZuneDrawRectangleRoundedOutlineAAXYWH(demo_rp, 50, 150, 100, 80, 5,
                                        ZUNE_LIGHTGRAY);
  ZuneDrawRectangleRoundedOutlineAAXYWH(demo_rp, 170, 150, 100, 80, 15,
                                        ZUNE_MAGENTA);
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 290, 150, 100, 80, 20, 5,
                                              ZUNE_CYAN);

  /* Rounded filled rectangles with border (styled AA) */
  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 50, 250, 100, 80, 15, 1,
                                       ZUNE_BRUSH_SOLID(ZUNE_WHITE),
                                       ZUNE_LIGHTGRAY);
  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 170, 250, 100, 80, 20, 4,
                                       ZUNE_BRUSH_SOLID(ZUNE_YELLOW),
                                       ZUNE_LIGHTGRAY);
  ZuneFillRectangleRoundedStyledAAXYWH(demo_rp, 290, 250, 100, 80, 25, 1,
                                       ZUNE_BRUSH_SOLID(ZUNE_GRAY),
                                       ZUNE_LIGHTGRAY);

  /* More rounded outlined rectangles with different line widths */
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 50, 350, 100, 80, 10, 1,
                                              ZUNE_RED);
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 170, 350, 100, 80, 15, 3,
                                              ZUNE_GREEN);
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(demo_rp, 290, 350, 100, 80, 20, 4,
                                              ZUNE_BLUE);

  /* Overlapping AA rectangles to show smooth blending */
  ZuneFillRectangleRoundedAAXYWH(
      demo_rp, 420, 100, 150, 100, 30,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(
      demo_rp, 450, 130, 150, 100, 35, 5,
      /*ZUNE_COLOR_ARGB32(128, 0, 255, 255),*/
      ZUNE_COLOR_ARGB32(255, 255, 0, 0));

  /* Small AA rectangles to show edge smoothing */
  ZuneFillRectangleRoundedAAXYWH(
      demo_rp, 420, 280, 40, 30, 8,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
  ZuneDrawRectangleRoundedOutlineStyledAAXYWH(
      demo_rp, 470, 290, 35, 25, 6, 2, ZUNE_COLOR_ARGB32(200, 64, 255, 128));
  ZuneFillRectangleRoundedStyledAAXYWH(
      demo_rp, 515, 300, 45, 35, 10, 3,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
      ZUNE_COLOR_ARGB32(255, 255, 255, 255));

  printf("  All AA rectangles drawn using locked pixel access\n");

  /* Unlock the pixels */
  UnlockDrawingBoardPixels(demo_rp);

  printf("  Pixels unlocked\n");
}

void DemoLockedCircles(void) {
  APTR pixels;
  ULONG pitch;

  printf("  Locking DrawingBoard pixels...\n");

  /* Lock the DrawingBoard for direct pixel access */
  pixels = LockDrawingBoardPixels(demo_rp, &pitch);
  if (!pixels) {
    printf("  ERROR: Cannot lock DrawingBoard pixels\n");
    return;
  }

  printf("  Pixels locked successfully (address: %p, pitch: %u)\n", pixels,
         pitch);
  printf("  Drawing circles using direct pixel manipulation...\n");

  /* Regular filled circles */
  ZuneDrawCircleAt(demo_rp, 120, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneDrawCircleAt(demo_rp, 240, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneDrawCircleAt(demo_rp, 360, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

  /* Regular outlined circles */
  ZuneDrawCircleOutlineAt(demo_rp, 120, 200, 40, ZUNE_YELLOW);
  ZuneDrawCircleOutlineAt(demo_rp, 240, 200, 40, ZUNE_MAGENTA);
  ZuneDrawCircleOutlineStyledAt(demo_rp, 360, 200, 40, 5.0, ZUNE_CYAN);

  /* Various sizes of filled circles */
  ZuneDrawCircleAt(demo_rp, 480, 90, 30, ZUNE_BRUSH_SOLID(ZUNE_WHITE));
  ZuneDrawCircleAt(demo_rp, 480, 150, 25, ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
  ZuneDrawCircleAt(demo_rp, 480, 200, 20, ZUNE_BRUSH_SOLID(ZUNE_GRAY));

  /* Various sizes of outlined circles */
  ZuneDrawCircleOutlineAt(demo_rp, 120, 320, 35, ZUNE_RED);
  ZuneDrawCircleOutlineStyledAt(demo_rp, 240, 320, 30, 3.0, ZUNE_GREEN);
  ZuneDrawCircleOutlineStyledAt(demo_rp, 360, 320, 25, 2.0, ZUNE_BLUE);

  /* Overlapping circles to test bounds checking */
  ZuneDrawCircleAt(demo_rp, 480, 280, 45,
                   ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawCircleOutlineStyledAt(demo_rp, 500, 300, 40, 4.0,
                                ZUNE_COLOR_ARGB32(128, 0, 255, 255));

  /* Small circles */
  ZuneDrawCircleAt(demo_rp, 520, 350, 15,
                   ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
  ZuneDrawCircleOutlineAt(demo_rp, 550, 370, 12,
                          ZUNE_COLOR_ARGB32(200, 64, 255, 128));

  printf("  All circles drawn using locked pixel access\n");

  /* Unlock the pixels */
  UnlockDrawingBoardPixels(demo_rp);

  printf("  Pixels unlocked\n");
}

void DemoUnlockedCircles(void) {
  printf("  Drawing circles using standard rastport operations...\n");

  /* Draw the same pattern but without locking (will use standard drawing
   * operations) */
  /* Regular filled circles */
  ZuneDrawCircleAt(demo_rp, 120, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneDrawCircleAt(demo_rp, 240, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneDrawCircleAt(demo_rp, 360, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

  /* Regular outlined circles */
  ZuneDrawCircleOutlineAt(demo_rp, 120, 200, 40, ZUNE_YELLOW);
  ZuneDrawCircleOutlineAt(demo_rp, 240, 200, 40, ZUNE_MAGENTA);
  ZuneDrawCircleOutlineStyledAt(demo_rp, 360, 200, 40, 5.0, ZUNE_CYAN);

  /* Various sizes of filled circles */
  ZuneDrawCircleAt(demo_rp, 480, 90, 30, ZUNE_BRUSH_SOLID(ZUNE_WHITE));
  ZuneDrawCircleAt(demo_rp, 480, 150, 25, ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
  ZuneDrawCircleAt(demo_rp, 480, 200, 20, ZUNE_BRUSH_SOLID(ZUNE_GRAY));

  /* Various sizes of outlined circles */
  ZuneDrawCircleOutlineAt(demo_rp, 120, 320, 35, ZUNE_RED);
  ZuneDrawCircleOutlineStyledAt(demo_rp, 240, 320, 30, 3.0, ZUNE_GREEN);
  ZuneDrawCircleOutlineStyledAt(demo_rp, 360, 320, 25, 2.0, ZUNE_BLUE);

  /* Overlapping circles */
  ZuneDrawCircleAt(demo_rp, 480, 280, 45,
                   ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawCircleOutlineStyledAt(demo_rp, 500, 300, 40, 4.0,
                                ZUNE_COLOR_ARGB32(128, 0, 255, 255));

  /* Small circles */
  ZuneDrawCircleAt(demo_rp, 520, 350, 15,
                   ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
  ZuneDrawCircleOutlineAt(demo_rp, 550, 370, 12,
                          ZUNE_COLOR_ARGB32(200, 64, 255, 128));

  printf("  All circles drawn using standard operations\n");
}

void DemoUnlockedAACircles(void) {
  APTR pixels;
  ULONG pitch;

  printf("  Drawing AA circles using standard rastport operations...\n");

  /* Anti-aliased filled circles */
  ZuneFillCircleAAAt(demo_rp, 120, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneFillCircleAAAt(demo_rp, 240, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneFillCircleAAAt(demo_rp, 360, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

  /* Anti-aliased outlined circles */
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 120, 200, 40, 1, ZUNE_YELLOW);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 240, 200, 40, 3, ZUNE_MAGENTA);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 360, 200, 40, 5, ZUNE_CYAN);

  /* Anti-aliased filled circles with borders (styled AA) */
  ZuneFillCircleStyledAAAt(demo_rp, 480, 90, 30, 1,
                           ZUNE_BRUSH_SOLID(ZUNE_WHITE), ZUNE_LIGHTGRAY);
  ZuneFillCircleStyledAAAt(demo_rp, 480, 150, 25, 3,
                           ZUNE_BRUSH_SOLID(ZUNE_YELLOW), ZUNE_GRAY);
  ZuneFillCircleStyledAAAt(demo_rp, 480, 210, 20, 2,
                           ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), ZUNE_RED);

  /* Various line widths for outlined AA circles */
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 120, 320, 35, 1, ZUNE_RED);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 240, 320, 30, 4, ZUNE_GREEN);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 360, 320, 25, 6, ZUNE_BLUE);

  /* Overlapping AA circles to show smooth blending */
  ZuneFillCircleAAAt(demo_rp, 480, 300, 45,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 500, 320, 40, 4,
                                  ZUNE_COLOR_ARGB32(255, 255, 0, 0));

  /* Small AA circles to show edge smoothing */
  ZuneFillCircleAAAt(demo_rp, 520, 370, 15,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 550, 390, 12, 2,
                                  ZUNE_COLOR_ARGB32(200, 64, 255, 128));
  ZuneFillCircleStyledAAAt(
      demo_rp, 580, 410, 18, 3,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
      ZUNE_COLOR_ARGB32(255, 255, 255, 255));

  printf("  All AA circles drawn using standard rastport operations\n");
}

void DemoLockedAACircles(void) {
  APTR pixels;
  ULONG pitch;

  printf("  Locking DrawingBoard pixels...\n");

  /* Lock the DrawingBoard for direct pixel access */
  pixels = LockDrawingBoardPixels(demo_rp, &pitch);
  if (!pixels) {
    printf("  ERROR: Cannot lock DrawingBoard pixels\n");
    return;
  }

  printf("  Pixels locked successfully (address: %p, pitch: %u)\n", pixels,
         pitch);
  printf("  Drawing AA circles using direct pixel manipulation...\n");

  /* Anti-aliased filled circles */
  ZuneFillCircleAAAt(demo_rp, 120, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_RED));
  ZuneFillCircleAAAt(demo_rp, 240, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
  ZuneFillCircleAAAt(demo_rp, 360, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

  /* Anti-aliased outlined circles */
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 120, 200, 40, 1, ZUNE_YELLOW);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 240, 200, 40, 3, ZUNE_MAGENTA);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 360, 200, 40, 5, ZUNE_CYAN);

  /* Anti-aliased filled circles with borders (styled AA) */
  ZuneFillCircleStyledAAAt(demo_rp, 480, 90, 30, 1,
                           ZUNE_BRUSH_SOLID(ZUNE_WHITE), ZUNE_LIGHTGRAY);
  ZuneFillCircleStyledAAAt(demo_rp, 480, 150, 25, 3,
                           ZUNE_BRUSH_SOLID(ZUNE_YELLOW), ZUNE_GRAY);
  ZuneFillCircleStyledAAAt(demo_rp, 480, 210, 20, 2,
                           ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), ZUNE_RED);

  /* Various line widths for outlined AA circles */
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 120, 320, 35, 1, ZUNE_RED);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 240, 320, 30, 4, ZUNE_GREEN);
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 360, 320, 25, 6, ZUNE_BLUE);

  /* Overlapping AA circles to show smooth blending */
  ZuneFillCircleAAAt(demo_rp, 480, 300, 45,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 500, 320, 40, 4,
                                  ZUNE_COLOR_ARGB32(255, 255, 0, 0));

  /* Small AA circles to show edge smoothing */
  ZuneFillCircleAAAt(demo_rp, 520, 370, 15,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
  ZuneDrawCircleOutlineStyledAAAt(demo_rp, 550, 390, 12, 2,
                                  ZUNE_COLOR_ARGB32(200, 64, 255, 128));
  ZuneFillCircleStyledAAAt(
      demo_rp, 580, 410, 18, 3,
      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
      ZUNE_COLOR_ARGB32(255, 255, 255, 255));

  printf("  All AA circles drawn using locked pixel access\n");

  /* Unlock the pixels */
  UnlockDrawingBoardPixels(demo_rp);

  printf("  Pixels unlocked\n");
}

void DemoLockedLines() {
  APTR pixels;
  ULONG pitch;

  printf("  Locking DrawingBoard pixels...\n");

  /* Lock the DrawingBoard for direct pixel access */
  pixels = LockDrawingBoardPixels(demo_rp, &pitch);
  if (!pixels) {
    printf("  ERROR: Cannot lock DrawingBoard pixels\n");
    return;
  }

  printf("  Drawing basic lines...\n");

  /* Basic horizontal lines with different colors */
  ZuneDrawLinePoints(demo_rp, 50, 50, 300, 50, ZUNE_RED);
  ZuneDrawLinePoints(demo_rp, 50, 70, 300, 70, ZUNE_GREEN);
  ZuneDrawLinePoints(demo_rp, 50, 90, 300, 90, ZUNE_BLUE);

  /* Vertical lines */
  ZuneDrawLinePoints(demo_rp, 350, 50, 350, 200, ZUNE_YELLOW);
  ZuneDrawLinePoints(demo_rp, 370, 50, 370, 200, ZUNE_MAGENTA);
  ZuneDrawLinePoints(demo_rp, 390, 50, 390, 200, ZUNE_CYAN);

  /* Diagonal lines */
  ZuneDrawLinePoints(demo_rp, 450, 50, 550, 150, ZUNE_WHITE);
  ZuneDrawLinePoints(demo_rp, 450, 150, 550, 50, ZUNE_LIGHTGRAY);

  printf("  Drawing styled lines with different thicknesses...\n");

  // /* Thick horizontal lines */
  ZuneDrawLineStyledPoints(demo_rp, 50, 250, 300, 250, 2, ZUNE_RED);
  ZuneDrawLineStyledPoints(demo_rp, 50, 270, 300, 270, 3, ZUNE_GREEN);
  ZuneDrawLineStyledPoints(demo_rp, 50, 295, 300, 295, 5, ZUNE_BLUE);

  /* Thick vertical lines */
  ZuneDrawLineStyledPoints(demo_rp, 350, 250, 350, 350, 2, ZUNE_YELLOW);
  ZuneDrawLineStyledPoints(demo_rp, 370, 250, 370, 350, 3, ZUNE_MAGENTA);
  ZuneDrawLineStyledPoints(demo_rp, 390, 250, 390, 350, 10, ZUNE_CYAN);

  /* Thick diagonal lines */
  ZuneDrawLineStyledPoints(demo_rp, 450, 250, 550, 350, 3, ZUNE_WHITE);
  ZuneDrawLineStyledPoints(demo_rp, 450, 350, 550, 250, 6, ZUNE_YELLOW);

  /* Unlock the pixels */
  UnlockDrawingBoardPixels(demo_rp);

  printf("  Pixels unlocked\n");
}

void DemoUnlockedLines() {
  printf("  Drawing lines to unlocked DrawingBoard...\n");

  /* Basic horizontal lines with different colors */
  ZuneDrawLinePoints(demo_rp, 50, 50, 300, 50, ZUNE_RED);
  ZuneDrawLinePoints(demo_rp, 50, 70, 300, 70, ZUNE_GREEN);
  ZuneDrawLinePoints(demo_rp, 50, 90, 300, 90, ZUNE_BLUE);

  /* Vertical lines */
  ZuneDrawLinePoints(demo_rp, 350, 50, 350, 200, ZUNE_YELLOW);
  ZuneDrawLinePoints(demo_rp, 370, 50, 370, 200, ZUNE_MAGENTA);
  ZuneDrawLinePoints(demo_rp, 390, 50, 390, 200, ZUNE_CYAN);

  /* Diagonal lines */
  ZuneDrawLinePoints(demo_rp, 450, 50, 550, 150, ZUNE_WHITE);
  ZuneDrawLinePoints(demo_rp, 450, 150, 550, 50, ZUNE_LIGHTGRAY);

  printf("  Drawing styled lines with different thicknesses...\n");

  /* Thick horizontal lines */
  ZuneDrawLineStyledPoints(demo_rp, 50, 250, 300, 250, 2, ZUNE_RED);
  ZuneDrawLineStyledPoints(demo_rp, 50, 270, 300, 270, 3, ZUNE_GREEN);
  ZuneDrawLineStyledPoints(demo_rp, 50, 295, 300, 295, 5, ZUNE_BLUE);

  /* Thick vertical lines */
  ZuneDrawLineStyledPoints(demo_rp, 350, 250, 350, 350, 2, ZUNE_YELLOW);
  ZuneDrawLineStyledPoints(demo_rp, 370, 250, 370, 350, 3, ZUNE_MAGENTA);
  ZuneDrawLineStyledPoints(demo_rp, 390, 250, 390, 350, 10, ZUNE_CYAN);

  /* Thick diagonal lines */
  ZuneDrawLineStyledPoints(demo_rp, 450, 250, 550, 350, 3, ZUNE_WHITE);
  ZuneDrawLineStyledPoints(demo_rp, 450, 350, 550, 250, 6, ZUNE_YELLOW);

  printf("  Unlocked line drawing completed\n");
}

void DemoLockedAALines() {
  APTR pixels;
  ULONG pitch;

  printf("  Locking DrawingBoard pixels...\n");

  /* Lock the DrawingBoard for direct pixel access */
  pixels = LockDrawingBoardPixels(demo_rp, &pitch);
  if (!pixels) {
    printf("  ERROR: Cannot lock DrawingBoard pixels\n");
    return;
  }

  printf("  Drawing anti-aliased lines...\n");

  // /* Basic anti-aliased lines */
  // ZuneDrawLineAAPoints(demo_rp, 50, 50, 300, 50, ZUNE_RED);
  // ZuneDrawLineAAPoints(demo_rp, 50, 70, 300, 70, ZUNE_GREEN);
  // ZuneDrawLineAAPoints(demo_rp, 50, 90, 300, 90, ZUNE_BLUE);

  /* Anti-aliased diagonal lines to show smoothing */
  ZuneDrawLineAAPoints(demo_rp, 50, 120, 200, 180, ZUNE_YELLOW);
  ZuneDrawLineAAPoints(demo_rp, 50, 180, 200, 120, ZUNE_MAGENTA);
  ZuneDrawLineAAPoints(demo_rp, 220, 120, 370, 180, ZUNE_CYAN);
  ZuneDrawLineAAPoints(demo_rp, 220, 180, 370, 120, ZUNE_WHITE);

  printf("  Drawing styled anti-aliased lines...\n");

  /* Thick anti-aliased lines */
  // ZuneDrawLineStyledAAPoints(demo_rp, 50, 250, 300, 250, 2, ZUNE_RED);
  // ZuneDrawLineStyledAAPoints(demo_rp, 50, 275, 300, 275, 3, ZUNE_GREEN);
  // ZuneDrawLineStyledAAPoints(demo_rp, 50, 305, 300, 305, 5, ZUNE_BLUE);

  // /* Thick anti-aliased diagonal lines */
  // ZuneDrawLineStyledAAPoints(demo_rp, 350, 250, 500, 350, 2, ZUNE_YELLOW);
  // ZuneDrawLineStyledAAPoints(demo_rp, 380, 250, 530, 350, 3, ZUNE_MAGENTA);
  // ZuneDrawLineStyledAAPoints(demo_rp, 410, 250, 560, 350, 5, ZUNE_CYAN);

  /* Unlock the pixels */
  UnlockDrawingBoardPixels(demo_rp);

  printf("  Pixels unlocked\n");
}

void DemoUnlockedAALines() {
  printf("  Drawing anti-aliased lines to unlocked DrawingBoard...\n");

  /* Basic anti-aliased lines */
  ZuneDrawLineAAPoints(demo_rp, 50, 50, 300, 50, ZUNE_RED);
  ZuneDrawLineAAPoints(demo_rp, 50, 70, 300, 70, ZUNE_GREEN);
  ZuneDrawLineAAPoints(demo_rp, 50, 90, 300, 90, ZUNE_BLUE);

  /* Anti-aliased diagonal lines to show smoothing */
  ZuneDrawLineAAPoints(demo_rp, 50, 120, 200, 180, ZUNE_YELLOW);
  ZuneDrawLineAAPoints(demo_rp, 50, 180, 200, 120, ZUNE_MAGENTA);
  ZuneDrawLineAAPoints(demo_rp, 220, 120, 370, 180, ZUNE_CYAN);
  ZuneDrawLineAAPoints(demo_rp, 220, 180, 370, 120, ZUNE_WHITE);

  printf("  Drawing styled anti-aliased lines...\n");

  /* Thick anti-aliased lines */
  ZuneDrawLineStyledAAPoints(demo_rp, 50, 250, 300, 250, 2, ZUNE_RED);
  ZuneDrawLineStyledAAPoints(demo_rp, 50, 275, 300, 275, 3, ZUNE_GREEN);
  ZuneDrawLineStyledAAPoints(demo_rp, 50, 305, 300, 305, 5, ZUNE_BLUE);

  /* Thick anti-aliased diagonal lines */
  ZuneDrawLineStyledAAPoints(demo_rp, 350, 250, 500, 350, 2, ZUNE_YELLOW);
  ZuneDrawLineStyledAAPoints(demo_rp, 380, 250, 530, 350, 3, ZUNE_MAGENTA);
  ZuneDrawLineStyledAAPoints(demo_rp, 410, 250, 560, 350, 5, ZUNE_CYAN);

  printf("  Unlocked anti-aliased line drawing completed\n");
}

void DemoTextures(void) {
  printf("  Testing texture creation and rendering...\n");

  struct ZuneTexture *test_texture = NULL;
  struct ZuneTexture *board_texture = NULL;
  APTR pixel_data = NULL;

  /* Test 1: Create a simple texture from data */
  printf("  Creating texture from pixel data...\n");

  /* Create a simple 64x64 red-to-blue gradient texture */
  ULONG texture_width = 64;
  ULONG texture_height = 64;
  ULONG data_size = texture_width * texture_height * 4; /* ARGB32 format */

  pixel_data = AllocMem(data_size, MEMF_PUBLIC);
  if (pixel_data) {
    ULONG *pixels = (ULONG *)pixel_data;
    ULONG i, j;

    /* Create a gradient pattern */
    for (i = 0; i < texture_height; i++) {
      for (j = 0; j < texture_width; j++) {
        UBYTE red = (UBYTE)(255 * j / texture_width);
        UBYTE blue = (UBYTE)(255 * i / texture_height);
        UBYTE green = (UBYTE)(128 + 127 * ((i + j) & 1));
        UBYTE alpha = 255;

        pixels[i * texture_width + j] =
            ZUNE_COLOR_ARGB32(alpha, red, green, blue);
      }
    }

    ULONG pitch = texture_width * 4; /* 4 bytes per pixel for ARGB32 */
    test_texture = CreateTextureFromData(
        pixel_data, texture_width, texture_height, 32,
        ZUNE_TEXTURE_FORMAT_ARGB32, pitch, ZUNE_TEXTURE_HARDWARE);

    if (test_texture) {
      printf("  Texture created successfully (%dx%d)\n", texture_width,
             texture_height);

      /* Test basic texture drawing */
      {
        struct ZunePoint pos = {50, 50};
        ZuneDrawTexture(demo_rp, test_texture, &pos);
      }

      /* Test scaled texture drawing */
      {
        struct ZuneRect dest_rect = {150, 50, 128, 128};
        ZuneDrawTextureScaled(demo_rp, test_texture, &dest_rect);
      }

      /* Test tinted texture drawing */
      {
        struct ZunePoint pos = {300, 50};
        ZuneDrawTextureTinted(demo_rp, test_texture, &pos,
                              ZUNE_COLOR_ARGB32(128, 255, 0, 0));
      }

      /* Test scaled and tinted texture drawing */
      {
        struct ZuneRect dest_rect = {450, 50, 96, 96};
        ZuneDrawTextureScaledTinted(demo_rp, test_texture, &dest_rect,
                                    ZUNE_COLOR_ARGB32(128, 0, 255, 0));
      }

      /* Test texture region drawing */
      {
        struct ZuneRect src_rect = {16, 16, 32, 32};   /* Source region */
        struct ZuneRect dest_rect = {50, 200, 64, 64}; /* Destination */
        ZuneDrawTextureRegion(demo_rp, test_texture, &src_rect, &dest_rect);
      }

      /* Test texture region tinted drawing */
      {
        struct ZuneRect src_rect = {0, 0, 32, 32};      /* Source region */
        struct ZuneRect dest_rect = {150, 200, 96, 96}; /* Destination */
        ZuneDrawTextureRegionTinted(demo_rp, test_texture, &src_rect,
                                    &dest_rect,
                                    ZUNE_COLOR_ARGB32(128, 0, 0, 255));
      }

      printf("  Basic texture drawing tests completed\n");
    } else {
      printf("  ERROR: Failed to create texture from data\n");
    }
  } else {
    printf("  ERROR: Failed to allocate pixel data\n");
  }

  /* Test 2: Create texture from DrawingBoard */
  printf("  Creating texture from DrawingBoard...\n");

  /* Use the existing window_rp to create a temp DrawingBoard */
  struct DrawingBoard *temp_board =
      CreateDrawingBoardForRenderPort(window_rp, 128, 128);
  if (temp_board) {
    /* Use window_rp with ZuneSetTarget to render to temp_board */
    struct RenderPort *temp_rp = window_rp;
    ZuneSetTarget(temp_rp, temp_board);

    if (temp_rp) {
      /* Draw some content on the temporary board */
      ClearDrawingBoard(temp_rp, ZUNE_DARKGRAY);
      ZuneDrawRectangleXYWH(temp_rp, 10, 10, 50, 50,
                            ZUNE_BRUSH_SOLID(ZUNE_RED));
      ZuneDrawCircleAt(temp_rp, 90, 90, 25, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
      ZuneDrawRectangleOutlineXYWH(temp_rp, 20, 60, 80, 30, ZUNE_BLUE);

      /* Create texture from the DrawingBoard */
      board_texture =
          CreateTextureFromDrawingBoard(temp_rp, ZUNE_TEXTURE_HARDWARE);

      if (board_texture) {
        printf("  DrawingBoard texture created successfully\n");

        /* Switch back to demo_board for drawing */
        ZuneSetTarget(window_rp, demo_board);

        /* Draw the board texture at different positions */
        {
          struct ZunePoint pos1 = {300, 200};
          ZuneDrawTexture(demo_rp, board_texture, &pos1);

          struct ZuneRect dest_rect = {450, 200, 64, 64};
          ZuneDrawTextureScaled(demo_rp, board_texture, &dest_rect);
        }

        printf("  DrawingBoard texture drawing completed\n");
      } else {
        printf("  ERROR: Failed to create texture from DrawingBoard\n");
        /* Switch back to demo_board even on failure */
        ZuneSetTarget(window_rp, demo_board);
      }
    }

    /* Don't destroy window_rp - it's our main RenderPort! Just destroy temp_board */
    DestroyDrawingBoard(temp_board);
  }

  /* Test 3: Texture pixel manipulation */
  if (test_texture && ZuneIsTextureValid(test_texture)) {
    printf("  Testing texture pixel manipulation...\n");

    ULONG pitch = 0;
    APTR lock_handle = LockTexturePixels(test_texture, &pitch);
    if (lock_handle) {
      /* Modify some pixels directly */
      {
        struct ZunePoint p1 = {32, 32};
        struct ZunePoint p2 = {31, 31};
        struct ZunePoint p3 = {33, 33};
        struct ZunePoint p4 = {32, 31};
        struct ZunePoint p5 = {32, 33};
        SetTexturePixel(test_texture, &p1, ZUNE_WHITE);
        SetTexturePixel(test_texture, &p2, ZUNE_WHITE);
        SetTexturePixel(test_texture, &p3, ZUNE_WHITE);
        SetTexturePixel(test_texture, &p4, ZUNE_WHITE);
        SetTexturePixel(test_texture, &p5, ZUNE_WHITE);
      }

      /* Test getting pixel colors */
      {
        struct ZunePoint p = {10, 10};
        ULONG pixel_color = GetTexturePixel(test_texture, &p);
        printf("  Pixel at (10,10): ARGB=0x%08X\n", (unsigned int)pixel_color);
      }

      UnlockTexturePixels(test_texture);

      /* Draw the modified texture */
      {
        struct ZunePoint pos = {50, 300};
        ZuneDrawTexture(demo_rp, test_texture, &pos);
      }

      printf("  Texture pixel manipulation completed\n");
    } else {
      printf("  ERROR: Failed to lock texture pixels\n");
    }
  }

  /* Test 4: Update texture data */
  if (test_texture && pixel_data) {
    printf("  Testing texture data update...\n");

    /* Modify the pixel data to create a different pattern */
    ULONG *pixels = (ULONG *)pixel_data;
    ULONG i, j;

    for (i = 0; i < texture_height; i++) {
      for (j = 0; j < texture_width; j++) {
        UBYTE intensity = (UBYTE)(255 * ((i + j) % 16) / 15);
        pixels[i * texture_width + j] =
            ZUNE_COLOR_ARGB32(255, intensity, intensity, intensity);
      }
    }

    /* Update the texture with new data */
    {
      struct ZuneRect update_rect = {0, 0, texture_width, texture_height};
      if (UpdateTextureData(test_texture, pixel_data, &update_rect)) {
        printf("  Texture data updated successfully\n");

        /* Draw the updated texture */
        {
          struct ZunePoint pos = {200, 300};
          ZuneDrawTexture(demo_rp, test_texture, &pos);
        }
      } else {
        printf("  ERROR: Failed to update texture data\n");
      }
    }
  }

  /* Cleanup */
  if (test_texture) {
    DestroyTexture(test_texture);
    printf("  Test texture destroyed\n");
  }

  if (board_texture) {
    DestroyTexture(board_texture);
    printf("  DrawingBoard texture destroyed\n");
  }

  if (pixel_data) {
    FreeMem(pixel_data, data_size);
  }

  printf("  Texture demo completed\n");
}

void DemoTexturesTiled(void) {
  printf("  Testing tiled texture rendering across background...\n");

  struct ZuneTexture *tile_texture = NULL;
  APTR pixel_data = NULL;

  /* Create a small tile texture for tiling */
  printf("  Creating small tile texture...\n");

  ULONG tile_width = 32;
  ULONG tile_height = 32;
  ULONG data_size = tile_width * tile_height * 4; /* ARGB32 format */

  pixel_data = AllocMem(data_size, MEMF_PUBLIC);
  if (pixel_data) {
    ULONG *pixels = (ULONG *)pixel_data;
    ULONG i, j;

    /* Create a checkerboard pattern */
    for (i = 0; i < tile_height; i++) {
      for (j = 0; j < tile_width; j++) {
        BOOL checker = ((i / 8) + (j / 8)) % 2;
        ULONG color;

        if (checker) {
          color = ZUNE_COLOR_ARGB32(255, 100, 150, 200); /* Light blue */
        } else {
          color = ZUNE_COLOR_ARGB32(255, 200, 100, 150); /* Light pink */
        }

        pixels[i * tile_width + j] = color;
      }
    }

    ULONG pitch = tile_width * 4; /* 4 bytes per pixel for ARGB32 */
    tile_texture = CreateTextureFromData(
        pixel_data, tile_width, tile_height, 32, ZUNE_TEXTURE_FORMAT_ARGB32,
        pitch, ZUNE_TEXTURE_HARDWARE | ZUNE_TEXTURE_WRAPPING);

    if (tile_texture) {
      printf("  Tile texture created successfully (%dx%d)\n", tile_width,
             tile_height);

      /* Method 1: Using the new ZuneDrawTextureTiled function */
      printf("  Drawing tiled background using ZuneDrawTextureTiled...\n");

      struct ZuneRect background_rect = {0, 0, DEMO_WIDTH, DEMO_HEIGHT};
      ZuneDrawTextureTiled(demo_rp, tile_texture, &background_rect);

      printf("  Tiled background drawing completed\n");

      /* Method 2: Manual tiling (for comparison) */
      printf("  Drawing partial manual tiling for comparison...\n");

      UWORD manual_width = 200;
      UWORD manual_height = 150;
      UWORD start_x = 50;
      UWORD start_y = 300;

      /* Calculate how many tiles we need for the manual area */
      UWORD tiles_x = (manual_width + tile_width - 1) / tile_width;
      UWORD tiles_y = (manual_height + tile_height - 1) / tile_height;

      printf("  Manual tiling %dx%d tiles in %dx%d area\n", tiles_x, tiles_y,
             manual_width, manual_height);

      /* Draw tiles manually in a smaller area for comparison */
      UWORD tx, ty;
      for (ty = 0; ty < tiles_y; ty++) {
        for (tx = 0; tx < tiles_x; tx++) {
          struct ZunePoint pos;
          pos.x = start_x + tx * tile_width;
          pos.y = start_y + ty * tile_height;

          /* Only draw if the tile is within our manual area */
          if (pos.x < start_x + manual_width &&
              pos.y < start_y + manual_height) {
            ZuneDrawTexture(demo_rp, tile_texture, &pos);
          }
        }
      }

      printf("  Manual tiled area drawing completed\n");

      /* Add some content on top to show the tiled background effect */
      printf("  Adding overlay content to demonstrate tiled background...\n");

      /* Draw some shapes on top of the tiled background */
      ZuneDrawRectangleXYWH(
          demo_rp, 100, 100, 200, 150,
          ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 255, 255)));
      ZuneDrawRectangleOutlineXYWH(demo_rp, 98, 98, 204, 154, ZUNE_BLACK);

      ZuneDrawCircleAt(demo_rp, 400, 200, 60,
                       ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(180, 255, 200, 100)));
      ZuneDrawCircleOutlineAt(demo_rp, 400, 200, 62, ZUNE_BLACK);

      /* Draw text-like rectangles to simulate text on tiled background */
      ZuneDrawRectangleXYWH(demo_rp, 120, 120, 160, 20,
                            ZUNE_BRUSH_SOLID(ZUNE_BLACK));
      ZuneDrawRectangleXYWH(demo_rp, 120, 150, 120, 20,
                            ZUNE_BRUSH_SOLID(ZUNE_BLACK));
      ZuneDrawRectangleXYWH(demo_rp, 120, 180, 140, 20,
                            ZUNE_BRUSH_SOLID(ZUNE_BLACK));

      printf("  Overlay content added\n");

    } else {
      printf("  ERROR: Failed to create tile texture\n");
    }
  } else {
    printf("  ERROR: Failed to allocate pixel data for tile\n");
  }

  /* Cleanup */
  if (tile_texture) {
    DestroyTexture(tile_texture);
    printf("  Tile texture destroyed\n");
  }

  if (pixel_data) {
    FreeMem(pixel_data, data_size);
  }

  printf("  Tiled texture demo completed\n");
}

void ShowResults(const char *label) {
  printf("  ShowResults('%s'): Blitting to window\n", label);

  /* 
   * Must set target to NULL (window) before blitting, because
   * BlitDrawingBoardToRenderPort checks dst->target_board to decide
   * whether to blit to a DrawingBoard or to the window's RastPort.
   */
  ZuneSetTarget(window_rp, NULL);

  /* Blit DrawingBoard to window */
  BlitDrawingBoardToRenderPortRects(demo_board, window_rp, 0, 0, 0, 0,
                                    DEMO_WIDTH, DEMO_HEIGHT);

  /* Switch back to DrawingBoard for next drawing operations */
  ZuneSetTarget(window_rp, demo_board);
}

void PerformanceTest(void) {
#define TEST_RECTANGLES 1000
#define TEST_LINES 2000
  ULONG start_time, end_time, locked_time, unlocked_time;
  int i;

  printf("  Performing performance test with %d rectangles and %d lines...\n",
         TEST_RECTANGLES, TEST_LINES);

  /* Rectangle Performance Tests */
  printf("  Testing rectangle drawing performance...\n");

  /* Clear the board */
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);

  /* Test locked rectangle performance */
  printf("    Testing locked rectangle drawing performance...\n");

  APTR pixels = LockDrawingBoardPixels(demo_rp, NULL);
  if (pixels) {
    start_time = 0; /* Would use timer.device in real implementation */

    for (i = 0; i < TEST_RECTANGLES; i++) {
      int x = (i * 7) % (DEMO_WIDTH - 50);
      int y = (i * 11) % (DEMO_HEIGHT - 50);
      ULONG color = ZUNE_COLOR_RGB24(i % 256, (i * 2) % 256, (i * 3) % 256);
      ZuneDrawRectangleXYWH(demo_rp, x, y, 30, 20, ZUNE_BRUSH_SOLID(color));
    }

    end_time = 0; /* Would use timer.device in real implementation */
    locked_time = end_time - start_time;

    UnlockDrawingBoardPixels(demo_rp);
    printf("    Locked rectangle drawing completed\n");
  }

  /* Clear and test unlocked rectangle performance */
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);
  printf("    Testing unlocked rectangle drawing performance...\n");

  start_time = 0; /* Would use timer.device in real implementation */

  for (i = 0; i < TEST_RECTANGLES; i++) {
    int x = (i * 7) % (DEMO_WIDTH - 50);
    int y = (i * 11) % (DEMO_HEIGHT - 50);
    ULONG color = ZUNE_COLOR_RGB24(i % 256, (i * 2) % 256, (i * 3) % 256);
    ZuneDrawRectangleXYWH(demo_rp, x, y, 30, 20, ZUNE_BRUSH_SOLID(color));
  }

  end_time = 0; /* Would use timer.device in real implementation */
  unlocked_time = end_time - start_time;

  printf("    Unlocked rectangle drawing completed\n");

  /* Line Performance Tests */
  printf("  Testing line drawing performance...\n");

  /* Clear the board */
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);

  /* Test locked line performance */
  printf("    Testing locked line drawing performance...\n");

  pixels = LockDrawingBoardPixels(demo_rp, NULL);
  if (pixels) {
    start_time = 0; /* Would use timer.device in real implementation */

    for (i = 0; i < TEST_LINES; i++) {
      int x1 = (i * 3) % DEMO_WIDTH;
      int y1 = (i * 5) % DEMO_HEIGHT;
      int x2 = ((i * 7) + 50) % DEMO_WIDTH;
      int y2 = ((i * 11) + 50) % DEMO_HEIGHT;
      ULONG color =
          ZUNE_COLOR_RGB24((i * 2) % 256, (i * 3) % 256, (i * 5) % 256);
      ZuneDrawLinePoints(demo_rp, x1, y1, x2, y2, color);
    }

    end_time = 0; /* Would use timer.device in real implementation */
    locked_time = end_time - start_time;

    UnlockDrawingBoardPixels(demo_rp);
    printf("    Locked line drawing completed\n");
  }

  /* Clear and test unlocked line performance */
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);
  printf("    Testing unlocked line drawing performance...\n");

  start_time = 0; /* Would use timer.device in real implementation */

  for (i = 0; i < TEST_LINES; i++) {
    int x1 = (i * 3) % DEMO_WIDTH;
    int y1 = (i * 5) % DEMO_HEIGHT;
    int x2 = ((i * 7) + 50) % DEMO_WIDTH;
    int y2 = ((i * 11) + 50) % DEMO_HEIGHT;
    ULONG color = ZUNE_COLOR_RGB24((i * 2) % 256, (i * 3) % 256, (i * 5) % 256);
    ZuneDrawLinePoints(demo_rp, x1, y1, x2, y2, color);
  }

  end_time = 0; /* Would use timer.device in real implementation */
  unlocked_time = end_time - start_time;

  printf("    Unlocked line drawing completed\n");

  /* Styled Line Performance Tests */
  printf("  Testing styled line drawing performance...\n");

  /* Clear the board */
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);

  /* Test locked styled line performance */
  printf("    Testing locked styled line drawing performance...\n");

  pixels = LockDrawingBoardPixels(demo_rp, NULL);
  if (pixels) {
    start_time = 0; /* Would use timer.device in real implementation */

    for (i = 0; i < TEST_LINES / 2; i++) {
      int x1 = (i * 3) % DEMO_WIDTH;
      int y1 = (i * 5) % DEMO_HEIGHT;
      int x2 = ((i * 7) + 100) % DEMO_WIDTH;
      int y2 = ((i * 11) + 100) % DEMO_HEIGHT;
      int thickness = (i % 5) + 1;
      ULONG color =
          ZUNE_COLOR_RGB24((i * 4) % 256, (i * 6) % 256, (i * 8) % 256);
      ZuneDrawLineStyledPoints(demo_rp, x1, y1, x2, y2, thickness, color);
    }

    end_time = 0; /* Would use timer.device in real implementation */
    locked_time = end_time - start_time;

    UnlockDrawingBoardPixels(demo_rp);
    printf("    Locked styled line drawing completed\n");
  }

  /* Clear and test unlocked styled line performance */
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);
  printf("    Testing unlocked styled line drawing performance...\n");

  start_time = 0; /* Would use timer.device in real implementation */

  for (i = 0; i < TEST_LINES / 2; i++) {
    int x1 = (i * 3) % DEMO_WIDTH;
    int y1 = (i * 5) % DEMO_HEIGHT;
    int x2 = ((i * 7) + 100) % DEMO_WIDTH;
    int y2 = ((i * 11) + 100) % DEMO_HEIGHT;
    int thickness = (i % 5) + 1;
    ULONG color = ZUNE_COLOR_RGB24((i * 4) % 256, (i * 6) % 256, (i * 8) % 256);
    ZuneDrawLineStyledPoints(demo_rp, x1, y1, x2, y2, thickness, color);
  }

  end_time = 0; /* Would use timer.device in real implementation */
  unlocked_time = end_time - start_time;

  printf("    Unlocked styled line drawing completed\n");

  /* Note: In a real implementation, you'd use timer.device for accurate timing
   */
  printf("  Performance comparison:\n");
  printf("    Locked method:   Fast direct pixel manipulation\n");
  printf("    Unlocked method: Standard FillPixelArray/Draw calls\n");
  printf("    The locked method should be significantly faster for many "
         "operations\n");
  printf("    Line drawing tests help verify thickness handling consistency\n");
}
