/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Color Format Test

    This test verifies that the color format conversion functions work correctly
    for both ARGB32 and RGBA32 pixel formats when using locked DrawingBoards.
    It specifically tests the fix for the color format bug where BLACK was
    showing up as BLUE in locked DrawingBoards with RGBA32 format.
*/

#include <cybergraphx/cybergraphics.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunerenderer.h>
#include <stdio.h>
#include <stdlib.h>

#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/zunerenderer.h>

/* Test parameters */
#define TEST_WIDTH 800
#define TEST_HEIGHT 600
#define TEST_DEPTH 32

/* Color test patterns */
#define NUM_TEST_COLORS 8
static const ULONG test_colors[NUM_TEST_COLORS] = {
    ZUNE_BLACK,   // Should be black (0x00000000)
    ZUNE_WHITE,   // Should be white (0x00FFFFFF)
    ZUNE_RED,     // Should be red (0x00FF0000)
    ZUNE_GREEN,   // Should be green (0x0000FF00)
    ZUNE_BLUE,    // Should be blue (0x000000FF)
    ZUNE_YELLOW,  // Should be yellow (0x00FFFF00)
    ZUNE_MAGENTA, // Should be magenta (0x00FF00FF)
    ZUNE_CYAN     // Should be cyan (0x0000FFFF)
};

static const char *color_names[NUM_TEST_COLORS] = {
    "BLACK", "WHITE", "RED", "GREEN", "BLUE", "YELLOW", "MAGENTA", "CYAN"};

/* Global variables */
struct Library *ZuneRendererBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

struct Screen *screen = NULL;
struct Window *window = NULL;

/* Function prototypes */
BOOL InitTest(void);
void CleanupTest(void);
void TestPixelFormatColors(ULONG pixel_format, const char *format_name,
                           int offset_x, int offset_y);
void TestColorConversion(void);
void TestDirectPixelAccess(void);
void DisplayResults(const char *title);
BOOL CompareColors(ULONG expected, ULONG actual, const char *color_name);

int main(void) {
  printf("Zune Renderer - Color Format Test\n");
  printf("==================================\n\n");
  printf("This test verifies that color format conversion works correctly\n");
  printf(
      "for both ARGB32 and RGBA32 pixel formats in locked DrawingBoards.\n\n");

  if (!InitTest()) {
    printf("ERROR: Failed to initialize test\n");
    CleanupTest();
    return 1;
  }

  printf("Test initialized successfully\n");

  /* Test color conversion functions */
  printf("\n1. Testing color conversion functions...\n");
  TestColorConversion();

  /* Test ARGB32 pixel format */
  printf("\n2. Testing ARGB32 pixel format (left side)...\n");
  TestPixelFormatColors(PIXFMT_ARGB32, "ARGB32", 0, 0);

  /* Test RGBA32 pixel format */
  printf("\n3. Testing RGBA32 pixel format (right side)...\n");
  TestPixelFormatColors(PIXFMT_RGBA32, "RGBA32", 400, 0);

  /* Test direct pixel access */
  printf("\n4. Testing direct pixel access and readback...\n");
  TestDirectPixelAccess();

  DisplayResults("Color Format Test Results");

  printf("\nColor format test completed!\n");
  printf("Visual inspection: Both sides should show identical colors.\n");
  printf("If RGBA32 side shows wrong colors, the conversion functions need "
         "fixing.\n");
  printf("Press ENTER to exit.\n");
  getchar();

  CleanupTest();
  return 0;
}

BOOL InitTest(void) {
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
  screen = OpenScreenTags(
      NULL, SA_Width, TEST_WIDTH, SA_Height, TEST_HEIGHT, SA_Depth, TEST_DEPTH,
      SA_Title, (IPTR) "Color Format Test - ARGB32 vs RGBA32", TAG_DONE);

  if (!screen) {
    printf("ERROR: Cannot open screen\n");
    return FALSE;
  }

  /* Open window */
  window = OpenWindowTags(NULL, WA_CustomScreen, (IPTR)screen, WA_Left, 0,
                          WA_Top, 0, WA_Width, TEST_WIDTH, WA_Height,
                          TEST_HEIGHT, WA_Title, (IPTR) "Color Format Test",
                          WA_DragBar, TRUE, WA_CloseGadget, TRUE, WA_IDCMP,
                          IDCMP_CLOSEWINDOW, TAG_DONE);

  if (!window) {
    printf("ERROR: Cannot open window\n");
    return FALSE;
  }

  return TRUE;
}

void CleanupTest(void) {
  if (window) {
    CloseWindow(window);
    window = NULL;
  }

  if (screen) {
    CloseScreen(screen);
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

void TestPixelFormatColors(ULONG pixel_format, const char *format_name,
                           int offset_x, int offset_y) {
  printf("  Testing %s format...\n", format_name);

  /* Create DrawingBoard with specific pixel format */
  struct DrawingBoard *test_board = CreateDrawingBoard(
      400, TEST_HEIGHT, TEST_DEPTH, ZUNE_DRAWINGBOARD_HARDWARE);
  if (!test_board) {
    printf("  ERROR: Cannot create DrawingBoard for %s\n", format_name);
    return;
  }

  /* Force the pixel format for testing */
  test_board->pixel_format = pixel_format;

  /* Create RenderPort */
  struct RenderPort *test_rp = CreateRenderPortWithDrawingBoard(
      screen->ViewPort.ColorMap, test_board);
  if (!test_rp) {
    printf("  ERROR: Cannot create RenderPort for %s\n", format_name);
    DestroyDrawingBoard(test_board);
    return;
  }

  /* Clear to black */
  ClearDrawingBoard(test_rp, ZUNE_BLACK);

  /* Lock the DrawingBoard */
  APTR pixels = LockDrawingBoardPixels(test_rp, NULL);
  if (!pixels) {
    printf("  ERROR: Cannot lock pixels for %s format\n", format_name);
    DestroyRenderPort(test_rp);
    DestroyDrawingBoard(test_board);
    return;
  }

  printf("  Drawing test colors with %s format...\n", format_name);

  /* Draw color test rectangles */
  for (int i = 0; i < NUM_TEST_COLORS; i++) {
    int x = 50 + (i % 4) * 80;
    int y = 50 + (i / 4) * 100;

    /* Draw filled rectangle */
    ZuneDrawRectangleXYWH(test_rp, x, y, 60, 60,
                          ZUNE_BRUSH_SOLID(test_colors[i]));

    /* Draw outline rectangle with different color */
    ULONG outline_color =
        (test_colors[i] == ZUNE_BLACK) ? ZUNE_WHITE : ZUNE_BLACK;
    ZuneDrawRectangleOutlineXYWH(test_rp, x - 5, y - 5, 70, 70, outline_color);

    /* Draw a line through the center */
    ZuneDrawLinePoints(test_rp, x, y + 30, x + 60, y + 30, outline_color);

    /* Draw a small circle */
    ZuneDrawCircleAt(test_rp, x + 30, y + 15, 8.0f,
                     ZUNE_BRUSH_SOLID(outline_color));
  }

  /* Unlock pixels */
  UnlockDrawingBoardPixels(test_rp);

  /* Blit to screen */
  BlitDrawingBoardToRenderPortRects(test_board, test_rp, 0, 0, offset_x,
                                    offset_y, 400, TEST_HEIGHT);

  /* Cleanup */
  DestroyRenderPort(test_rp);
  DestroyDrawingBoard(test_board);

  printf("  %s format test completed\n", format_name);
}

void TestColorConversion(void) {
  printf("  Testing color conversion functions...\n");

  /* Test ARGB to RGBA conversion */
  for (int i = 0; i < NUM_TEST_COLORS; i++) {
    ULONG argb_color = test_colors[i];

    /* Simulate conversion to RGBA32 and back */
    ULONG rgba_converted =
        ((argb_color & 0x00FFFFFF) << 8) | ((argb_color >> 24) & 0xFF);
    ULONG argb_restored =
        ((rgba_converted & 0xFF) << 24) | ((rgba_converted >> 8) & 0x00FFFFFF);

    printf("    %s: ARGB=0x%08X -> RGBA=0x%08X -> ARGB=0x%08X %s\n",
           color_names[i], argb_color, rgba_converted, argb_restored,
           (argb_color == argb_restored) ? "[OK]" : "[FAIL]");

    if (argb_color != argb_restored) {
      printf("    ERROR: Color conversion roundtrip failed!\n");
    }
  }
}

void TestDirectPixelAccess(void) {
  printf("  Testing direct pixel read/write operations...\n");

  /* Test with ARGB32 */
  struct DrawingBoard *argb_board =
      CreateDrawingBoard(100, 100, TEST_DEPTH, ZUNE_DRAWINGBOARD_HARDWARE);
  if (argb_board) {
    argb_board->pixel_format = PIXFMT_ARGB32;
    struct RenderPort *argb_rp = CreateRenderPortWithDrawingBoard(
        screen->ViewPort.ColorMap, argb_board);
    if (argb_rp) {
      APTR pixels = LockDrawingBoardPixels(argb_rp, NULL);
      if (pixels) {
        printf("    Testing ARGB32 pixel access...\n");

        /* Write and read back test colors */
        for (int i = 0; i < NUM_TEST_COLORS; i++) {
          SetPixelAt(argb_rp, 10 + i, 10, test_colors[i]);
          ULONG read_color = GetPixelAt(argb_rp, 10 + i, 10);

          if (!CompareColors(test_colors[i], read_color, color_names[i])) {
            printf("      ARGB32 pixel access failed for %s\n", color_names[i]);
          }
        }

        UnlockDrawingBoardPixels(argb_rp);
      }
      DestroyRenderPort(argb_rp);
    }
    DestroyDrawingBoard(argb_board);
  }

  /* Test with RGBA32 */
  struct DrawingBoard *rgba_board =
      CreateDrawingBoard(100, 100, TEST_DEPTH, ZUNE_DRAWINGBOARD_HARDWARE);
  if (rgba_board) {
    rgba_board->pixel_format = PIXFMT_RGBA32;
    struct RenderPort *rgba_rp = CreateRenderPortWithDrawingBoard(
        screen->ViewPort.ColorMap, rgba_board);
    if (rgba_rp) {
      APTR pixels = LockDrawingBoardPixels(rgba_rp, NULL);
      if (pixels) {
        printf("    Testing RGBA32 pixel access...\n");

        /* Write and read back test colors */
        for (int i = 0; i < NUM_TEST_COLORS; i++) {
          SetPixelAt(rgba_rp, 10 + i, 10, test_colors[i]);
          ULONG read_color = GetPixelAt(rgba_rp, 10 + i, 10);

          if (!CompareColors(test_colors[i], read_color, color_names[i])) {
            printf("      RGBA32 pixel access failed for %s\n", color_names[i]);
          }
        }

        UnlockDrawingBoardPixels(rgba_rp);
      }
      DestroyRenderPort(rgba_rp);
    }
    DestroyDrawingBoard(rgba_board);
  }
}

void DisplayResults(const char *title) {
  printf("  Displaying results: %s\n", title);
  printf("  Left side: ARGB32 format | Right side: RGBA32 format\n");
  printf(
      "  Both sides should show identical colors if conversion is working\n");
}

BOOL CompareColors(ULONG expected, ULONG actual, const char *color_name) {
  if (expected == actual) {
    printf("      %s: Expected 0x%08X, Got 0x%08X [OK]\n", color_name, expected,
           actual);
    return TRUE;
  } else {
    printf("      %s: Expected 0x%08X, Got 0x%08X [FAIL]\n", color_name,
           expected, actual);
    return FALSE;
  }
}
