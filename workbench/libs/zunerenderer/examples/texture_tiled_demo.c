/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Tiled Texture Rendering Demo

    This demo demonstrates various tiled texture rendering capabilities of
    the Zune Renderer library, showing how to create seamless tiled backgrounds
    and patterns.
*/

#include "clib/exec_protos.h"
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/displayinfo.h>
#include <graphics/regions.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunerenderer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/zunerenderer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************/
/* Demo Configuration */
/*****************************************************************************/

#define DEMO_WIDTH 640
#define DEMO_HEIGHT 480
#define DEMO_DEPTH 24

/*****************************************************************************/
/* Global Variables */
/*****************************************************************************/

struct Library *ZuneRendererBase = NULL;
struct Screen *demo_screen = NULL;
struct Window *demo_window = NULL;
struct DrawingBoard *demo_board = NULL;
struct RenderPort *demo_rp = NULL;

/*****************************************************************************/
/* Function Prototypes */
/*****************************************************************************/

BOOL InitDemo(void);
void CleanupDemo(void);
void DemoBasicTiling(void);
void DemoPatternTiling(void);
void DemoPartialTiling(void);
void DemoMultiTextureTiling(void);
struct ZuneTexture *CreateCheckerboardTexture(UWORD size, ULONG color1,
                                              ULONG color2);
struct ZuneTexture *CreateGradientTexture(UWORD width, UWORD height);
struct ZuneTexture *CreateBrickTexture(UWORD width, UWORD height);
struct ZuneTexture *CreateDotPatternTexture(UWORD size);
void ShowResults(void);

/*****************************************************************************/
/* Main Program */
/*****************************************************************************/

int main(void) {
  printf("Zune Renderer - Tiled Texture Rendering Demo\n");
  printf("============================================\n\n");

  if (!InitDemo()) {
    printf("ERROR: Failed to initialize demo\n");
    CleanupDemo();
    return 1;
  }

  printf("Demo initialized successfully\n");

  /* Demo 1: Basic tiling with simple patterns */
  printf("\n1. Basic checkerboard tiling...\n");
  ClearDrawingBoard(demo_rp, ZUNE_BLACK);
  DemoBasicTiling();
  ShowResults();
  getchar();

  /* Demo 2: Pattern-based tiling */
  printf("\n2. Advanced pattern tiling...\n");
  ClearDrawingBoard(demo_rp, ZUNE_DARKGRAY);
  DemoPatternTiling();
  ShowResults();
  getchar();

  /* Demo 3: Partial area tiling */
  printf("\n3. Partial area tiling...\n");
  ClearDrawingBoard(demo_rp, ZUNE_WHITE);
  DemoPartialTiling();
  ShowResults();
  getchar();

  /* Demo 4: Multiple texture tiling */
  printf("\n4. Multi-texture tiling...\n");
  ClearDrawingBoard(demo_rp, ZUNE_GRAY);
  DemoMultiTextureTiling();
  ShowResults();
  getchar();

  printf("\nTiled texture demo completed successfully. Press ENTER to exit.\n");
  getchar();

  CleanupDemo();
  return 0;
}

/*****************************************************************************/
/* Initialization and Cleanup */
/*****************************************************************************/

BOOL InitDemo(void) {
  /* Open the Zune Renderer library */
  ZuneRendererBase = OpenLibrary("zunerenderer.library", 0);
  if (!ZuneRendererBase) {
    printf("ERROR: Failed to open zunerenderer.library\n");
    return FALSE;
  }

  printf("ZuneRenderer library opened successfully\n");

  /* Open screen for display */
  demo_screen = LockPubScreen(
      NULL); /* NULL means the default public screen, which is Workbench */
  if (!demo_screen) {
    printf("ERROR: Cannot lock Workbench screen\n");
    return FALSE;
  }

  if (!demo_screen) {
    printf("ERROR: Failed to open screen\n");
    return FALSE;
  }

  printf("Screen opened successfully (%dx%dx%d)\n", DEMO_WIDTH, DEMO_HEIGHT,
         DEMO_DEPTH);

  /* Open a window on the screen */
  demo_window =
      OpenWindowTags(NULL, WA_CustomScreen, demo_screen, WA_Left, 0, WA_Top, 0,
                     WA_Width, DEMO_WIDTH, WA_Height, DEMO_HEIGHT, WA_Title,
                     "Tiled Texture Demo", WA_DragBar, FALSE, WA_Borderless,
                     TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE, TAG_DONE);

  if (!demo_window) {
    printf("ERROR: Failed to open window\n");
    return FALSE;
  }

  printf("Window opened successfully\n");

  /* Create a DrawingBoard for off-screen rendering */
  demo_board = CreateDrawingBoard(DEMO_WIDTH, DEMO_HEIGHT, DEMO_DEPTH,
                                  ZUNE_DRAWINGBOARD_TEMP);
  if (!demo_board) {
    printf("ERROR: Failed to create DrawingBoard\n");
    return FALSE;
  }

  printf("DrawingBoard created successfully\n");

  /* Create a RenderPort associated with the DrawingBoard */
  demo_rp = CreateRenderPortWithDrawingBoard(demo_screen->ViewPort.ColorMap,
                                             demo_board);
  if (!demo_rp) {
    printf("ERROR: Failed to create RenderPort\n");
    return FALSE;
  }

  printf("RenderPort created successfully\n");

  return TRUE;
}

void CleanupDemo(void) {
  if (demo_rp) {
    DestroyRenderPort(demo_rp);
    demo_rp = NULL;
    printf("RenderPort destroyed\n");
  }

  if (demo_board) {
    DestroyDrawingBoard(demo_board);
    demo_board = NULL;
    printf("DrawingBoard destroyed\n");
  }

  if (demo_window) {
    CloseWindow(demo_window);
    demo_window = NULL;
    printf("Window closed\n");
  }

  if (demo_screen) {
    CloseScreen(demo_screen);
    demo_screen = NULL;
    printf("Screen closed\n");
  }

  if (ZuneRendererBase) {
    CloseLibrary(ZuneRendererBase);
    ZuneRendererBase = NULL;
    printf("ZuneRenderer library closed\n");
  }
}

/*****************************************************************************/
/* Demo Functions */
/*****************************************************************************/

void DemoBasicTiling(void) {
  printf("  Creating basic tiled patterns...\n");

  /* Create a simple checkerboard texture */
  struct ZuneTexture *checker_texture = CreateCheckerboardTexture(
      32, ZUNE_COLOR_ARGB32(255, 60, 60, 60), /* Dark gray */
      ZUNE_COLOR_ARGB32(255, 200, 200, 200)   /* Light gray */
  );

  if (checker_texture) {
    printf("  Checkerboard texture created (32x32)\n");

    /* Tile across entire background */
    struct ZuneRect full_screen = {0, 0, DEMO_WIDTH, DEMO_HEIGHT};
    ZuneDrawTextureTiled(demo_rp, checker_texture, &full_screen);

    /* Add some overlay content to show the tiled background */
    struct ZuneRect title_bg = {50, 20, 540, 60};
    ZuneDrawRectangleXYWH(
        demo_rp, title_bg.x, title_bg.y, title_bg.width, title_bg.height,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 0, 100, 200)));

    ZuneDrawRectangleOutlineXYWH(demo_rp, title_bg.x - 2, title_bg.y - 2,
                                 title_bg.width + 4, title_bg.height + 4,
                                 ZUNE_WHITE);

    /* Add some decorative elements */
    ZuneDrawCircleAt(demo_rp, 150, 200, 40,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(180, 255, 150, 50)));
    ZuneDrawCircleOutlineAt(demo_rp, 150, 200, 42, ZUNE_BLACK);

    ZuneDrawRectangleRoundedXYWH(
        demo_rp, 300, 150, 200, 100, 15,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(160, 50, 255, 150)));
    ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, 298, 148, 204, 104, 17,
                                        ZUNE_WHITE);

    DestroyTexture(checker_texture);
    printf("  Basic tiling demo completed\n");
  } else {
    printf("  ERROR: Failed to create checkerboard texture\n");
  }
}

void DemoPatternTiling(void) {
  printf("  Creating advanced pattern tiles...\n");

  /* Create a brick pattern texture */
  struct ZuneTexture *brick_texture = CreateBrickTexture(64, 32);

  if (brick_texture) {
    printf("  Brick texture created (64x32)\n");

    /* Tile across entire background */
    struct ZuneRect full_screen = {0, 0, DEMO_WIDTH, DEMO_HEIGHT};
    ZuneDrawTextureTiled(demo_rp, brick_texture, &full_screen);

    /* Add architectural elements on top */
    struct ZuneRect window1 = {100, 100, 80, 120};
    ZuneDrawRectangleXYWH(
        demo_rp, window1.x, window1.y, window1.width, window1.height,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(220, 100, 150, 255)));
    ZuneDrawRectangleOutlineXYWH(demo_rp, window1.x - 3, window1.y - 3,
                                 window1.width + 6, window1.height + 6,
                                 ZUNE_DARKGRAY);

    struct ZuneRect window2 = {220, 120, 80, 100};
    ZuneDrawRectangleXYWH(
        demo_rp, window2.x, window2.y, window2.width, window2.height,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(220, 255, 255, 150)));
    ZuneDrawRectangleOutlineXYWH(demo_rp, window2.x - 3, window2.y - 3,
                                 window2.width + 6, window2.height + 6,
                                 ZUNE_DARKGRAY);

    /* Add a door */
    struct ZuneRect door = {400, 180, 60, 140};
    ZuneDrawRectangleRoundedXYWH(
        demo_rp, door.x, door.y, door.width, door.height, 8,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 139, 69, 19)));
    ZuneDrawRectangleRoundedOutlineXYWH(demo_rp, door.x - 2, door.y - 2,
                                        door.width + 4, door.height + 4, 10,
                                        ZUNE_BLACK);

    /* Door handle */
    ZuneDrawCircleAt(demo_rp, door.x + door.width - 15,
                     door.y + door.height / 2, 3,
                     ZUNE_BRUSH_SOLID(ZUNE_YELLOW));

    DestroyTexture(brick_texture);
    printf("  Pattern tiling demo completed\n");
  } else {
    printf("  ERROR: Failed to create brick texture\n");
  }
}

void DemoPartialTiling(void) {
  printf("  Creating partial area tiling...\n");

  /* Create a dot pattern texture */
  struct ZuneTexture *dot_texture = CreateDotPatternTexture(24);

  if (dot_texture) {
    printf("  Dot pattern texture created (24x24)\n");

    /* Create several tiled areas */
    struct ZuneRect areas[] = {
        {50, 50, 200, 150},   /* Top-left area */
        {300, 80, 250, 120},  /* Top-right area */
        {100, 250, 180, 100}, /* Bottom-left area */
        {350, 280, 200, 80}   /* Bottom-right area */
    };

    ULONG area_colors[] = {
        ZUNE_COLOR_ARGB32(180, 255, 200, 200), /* Light red tint */
        ZUNE_COLOR_ARGB32(180, 200, 255, 200), /* Light green tint */
        ZUNE_COLOR_ARGB32(180, 200, 200, 255), /* Light blue tint */
        ZUNE_COLOR_ARGB32(180, 255, 255, 200)  /* Light yellow tint */
    };

    int i;
    for (i = 0; i < 4; i++) {
      /* First fill area with background color */
      ZuneDrawRectangleXYWH(demo_rp, areas[i].x, areas[i].y, areas[i].width,
                            areas[i].height, ZUNE_BRUSH_SOLID(area_colors[i]));

      /* Then tile the dot pattern on top */
      ZuneDrawTextureTiled(demo_rp, dot_texture, &areas[i]);

      /* Add border */
      ZuneDrawRectangleOutlineXYWH(demo_rp, areas[i].x - 2, areas[i].y - 2,
                                   areas[i].width + 4, areas[i].height + 4,
                                   ZUNE_BLACK);
    }

    /* Add labels */
    struct ZuneRect label1 = {areas[0].x + 10,
                              areas[0].y + areas[0].height + 10, 80, 20};
    ZuneDrawRectangleXYWH(demo_rp, label1.x, label1.y, label1.width,
                          label1.height, ZUNE_BRUSH_SOLID(ZUNE_DARKGRAY));

    struct ZuneRect label2 = {areas[1].x + 10,
                              areas[1].y + areas[1].height + 10, 80, 20};
    ZuneDrawRectangleXYWH(demo_rp, label2.x, label2.y, label2.width,
                          label2.height, ZUNE_BRUSH_SOLID(ZUNE_DARKGRAY));

    DestroyTexture(dot_texture);
    printf("  Partial tiling demo completed\n");
  } else {
    printf("  ERROR: Failed to create dot pattern texture\n");
  }
}

void DemoMultiTextureTiling(void) {
  printf("  Creating multi-texture tiling...\n");

  /* Create multiple small textures */
  struct ZuneTexture *checker =
      CreateCheckerboardTexture(16, ZUNE_RED, ZUNE_YELLOW);
  struct ZuneTexture *gradient = CreateGradientTexture(32, 32);
  struct ZuneTexture *dots = CreateDotPatternTexture(20);

  if (checker && gradient && dots) {
    printf("  Multiple textures created successfully\n");

    /* Divide screen into sections and tile different textures */

    /* Left section - checkerboard */
    struct ZuneRect left_section = {0, 0, DEMO_WIDTH / 3, DEMO_HEIGHT};
    ZuneDrawTextureTiled(demo_rp, checker, &left_section);

    /* Middle section - gradient */
    struct ZuneRect middle_section = {DEMO_WIDTH / 3, 0, DEMO_WIDTH / 3,
                                      DEMO_HEIGHT};
    ZuneDrawTextureTiled(demo_rp, gradient, &middle_section);

    /* Right section - dots */
    struct ZuneRect right_section = {2 * DEMO_WIDTH / 3, 0, DEMO_WIDTH / 3,
                                     DEMO_HEIGHT};
    ZuneDrawTextureTiled(demo_rp, dots, &right_section);

    /* Add section dividers */
    ZuneDrawLinePoints(demo_rp, DEMO_WIDTH / 3, 0, DEMO_WIDTH / 3, DEMO_HEIGHT,
                       ZUNE_WHITE);
    ZuneDrawLinePoints(demo_rp, 2 * DEMO_WIDTH / 3, 0, 2 * DEMO_WIDTH / 3,
                       DEMO_HEIGHT, ZUNE_WHITE);

    /* Add section labels */
    struct ZuneRect label_bg = {10, 10, 150, 30};
    ZuneDrawRectangleXYWH(demo_rp, label_bg.x, label_bg.y, label_bg.width,
                          label_bg.height,
                          ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 0, 0, 0)));

    label_bg.x = DEMO_WIDTH / 3 + 10;
    ZuneDrawRectangleXYWH(demo_rp, label_bg.x, label_bg.y, label_bg.width,
                          label_bg.height,
                          ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 0, 0, 0)));

    label_bg.x = 2 * DEMO_WIDTH / 3 + 10;
    ZuneDrawRectangleXYWH(demo_rp, label_bg.x, label_bg.y, label_bg.width,
                          label_bg.height,
                          ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 0, 0, 0)));

    printf("  Multi-texture tiling demo completed\n");
  } else {
    printf("  ERROR: Failed to create one or more textures\n");
  }

  /* Cleanup */
  if (checker)
    DestroyTexture(checker);
  if (gradient)
    DestroyTexture(gradient);
  if (dots)
    DestroyTexture(dots);
}

/*****************************************************************************/
/* Texture Creation Helpers */
/*****************************************************************************/

struct ZuneTexture *CreateCheckerboardTexture(UWORD size, ULONG color1,
                                              ULONG color2) {
  ULONG data_size = size * size * 4; /* ARGB32 format */
  APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);

  if (!pixel_data)
    return NULL;

  ULONG *pixels = (ULONG *)pixel_data;
  UWORD i, j;

  for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++) {
      BOOL checker = ((i / (size / 4)) + (j / (size / 4))) % 2;
      pixels[i * size + j] = checker ? color1 : color2;
    }
  }

  ULONG pitch = size * 4;
  struct ZuneTexture *texture = CreateTextureFromData(
      pixel_data, size, size, 32, ZUNE_TEXTURE_FORMAT_ARGB32, pitch,
      ZUNE_TEXTURE_HARDWARE);

  FreeMem(pixel_data, data_size);
  return texture;
}

struct ZuneTexture *CreateGradientTexture(UWORD width, UWORD height) {
  ULONG data_size = width * height * 4; /* ARGB32 format */
  APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);

  if (!pixel_data)
    return NULL;

  ULONG *pixels = (ULONG *)pixel_data;
  UWORD i, j;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      UBYTE red = (UBYTE)(255 * j / width);
      UBYTE green = (UBYTE)(255 * i / height);
      UBYTE blue = (UBYTE)(255 * (i + j) / (width + height));
      pixels[i * width + j] = ZUNE_COLOR_ARGB32(255, red, green, blue);
    }
  }

  ULONG pitch = width * 4;
  struct ZuneTexture *texture = CreateTextureFromData(
      pixel_data, width, height, 32, ZUNE_TEXTURE_FORMAT_ARGB32, pitch,
      ZUNE_TEXTURE_HARDWARE);

  FreeMem(pixel_data, data_size);
  return texture;
}

struct ZuneTexture *CreateBrickTexture(UWORD width, UWORD height) {
  ULONG data_size = width * height * 4; /* ARGB32 format */
  APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);

  if (!pixel_data)
    return NULL;

  ULONG *pixels = (ULONG *)pixel_data;
  UWORD i, j;

  ULONG brick_color = ZUNE_COLOR_ARGB32(255, 180, 100, 60); /* Brick red */
  ULONG mortar_color =
      ZUNE_COLOR_ARGB32(255, 200, 200, 200); /* Light gray mortar */

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      BOOL is_mortar = FALSE;

      /* Horizontal mortar lines */
      if (i % (height / 2) == 0 || i % (height / 2) == 1) {
        is_mortar = TRUE;
      }

      /* Vertical mortar lines with brick offset */
      UWORD offset = (i / (height / 2)) % 2 ? width / 4 : 0;
      UWORD adjusted_j = (j + offset) % width;
      if (adjusted_j % (width / 2) == 0 || adjusted_j % (width / 2) == 1) {
        is_mortar = TRUE;
      }

      pixels[i * width + j] = is_mortar ? mortar_color : brick_color;
    }
  }

  ULONG pitch = width * 4;
  struct ZuneTexture *texture = CreateTextureFromData(
      pixel_data, width, height, 32, ZUNE_TEXTURE_FORMAT_ARGB32, pitch,
      ZUNE_TEXTURE_HARDWARE);

  FreeMem(pixel_data, data_size);
  return texture;
}

struct ZuneTexture *CreateDotPatternTexture(UWORD size) {
  ULONG data_size = size * size * 4; /* ARGB32 format */
  APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);

  if (!pixel_data)
    return NULL;

  ULONG *pixels = (ULONG *)pixel_data;
  UWORD i, j;

  ULONG bg_color =
      ZUNE_COLOR_ARGB32(100, 255, 255, 255); /* Semi-transparent white */
  ULONG dot_color = ZUNE_COLOR_ARGB32(255, 50, 50, 200); /* Blue dots */

  UWORD center = size / 2;
  UWORD dot_radius = size / 6;

  for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++) {
      /* Calculate distance from center (using simple Manhattan distance) */
      WORD dx = (j > center) ? (j - center) : (center - j);
      WORD dy = (i > center) ? (i - center) : (center - i);
      UWORD distance = dx + dy;

      pixels[i * size + j] = (distance <= dot_radius) ? dot_color : bg_color;
    }
  }

  ULONG pitch = size * 4;
  struct ZuneTexture *texture = CreateTextureFromData(
      pixel_data, size, size, 32, ZUNE_TEXTURE_FORMAT_ARGB32, pitch,
      ZUNE_TEXTURE_HARDWARE);

  FreeMem(pixel_data, data_size);
  return texture;
}

/*****************************************************************************/
/* Display Functions */
/*****************************************************************************/

void ShowResults(void) {
  printf("  Blitting DrawingBoard to screen...\n");
  BlitDrawingBoardToRenderPortRects(demo_board, demo_rp, 0, 0, 0, 0, DEMO_WIDTH,
                                    DEMO_HEIGHT);
}
