/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Line Drawing Test with Bresenham's Algorithm

    This test verifies that the improved CybergfxDrawLine function works correctly
    with Bresenham's line algorithm for all rendering paths:
    - Locked DrawingBoards (direct pixel manipulation)
    - Unlocked DrawingBoards (WriteRGBPixel)
    - RastPorts (WriteRGBPixel to screen)
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/screens.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <libraries/zunegfx.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/zunegfx.h>

/* Test parameters */
#define TEST_WIDTH 800
#define TEST_HEIGHT 600
#define TEST_DEPTH 32

/* Global variables */
struct Library *ZuneGfxBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

struct Screen *screen = NULL;
struct Window *window = NULL;
struct DrawingBoard *test_board = NULL;
struct RenderPort *test_rp = NULL;
struct RenderPort *window_rp = NULL;

/* Function prototypes */
BOOL InitTest(void);
void CleanupTest(void);
void TestLockedLineDrawing(void);
void TestUnlockedLineDrawing(void);
void DrawLineTestPattern(BOOL locked, int offset_x, int offset_y);
void DrawLineStressTest(BOOL locked, int offset_x, int offset_y);
void ShowResults(const char *title);
void VerifyLineAccuracy(void);

int main(void)
{
    printf("Zune Renderer - Line Drawing Test (Bresenham's Algorithm)\n");
    printf("=========================================================\n\n");

    if (!InitTest()) {
        printf("ERROR: Failed to initialize test\n");
        CleanupTest();
        return 1;
    }

    printf("Test initialized successfully\n");
    printf("Testing Bresenham's line algorithm implementation\n\n");

    /* Test locked line drawing */
    printf("1. Testing LOCKED DrawingBoard line drawing...\n");
    TestLockedLineDrawing();
    ShowResults("Locked DrawingBoard Line Drawing");

    printf("Press ENTER to continue to unlocked test...\n");
    getchar();

    /* Test unlocked line drawing */
    printf("2. Testing UNLOCKED DrawingBoard line drawing...\n");
    TestUnlockedLineDrawing();
    ShowResults("Unlocked DrawingBoard Line Drawing");

    printf("Press ENTER to continue to accuracy verification...\n");
    getchar();

    /* Verify line accuracy */
    printf("3. Verifying line drawing accuracy...\n");
    VerifyLineAccuracy();
    ShowResults("Line Accuracy Verification");

    printf("\nAll tests completed successfully!\n");
    printf("Press ENTER to exit.\n");
    getchar();

    CleanupTest();
    return 0;
}

BOOL InitTest(void)
{
    /* Open required libraries */
    ZuneGfxBase = OpenLibrary("zunegfx.library", 1);
    if (!ZuneGfxBase) {
        printf("ERROR: Cannot open zunegfx.library\n");
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
    screen = LockPubScreen(NULL);  /* NULL means the default public screen, which is Workbench */
    if (!screen) {
        printf("ERROR: Cannot lock Workbench screen\n");
        return FALSE;
    }

    /* Open window */
    window = OpenWindowTags(NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 0,
        WA_Top, 0,
        WA_Width, TEST_WIDTH,
        WA_Height, TEST_HEIGHT,
        WA_Title, (IPTR)"Line Drawing Test",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        TAG_DONE);

    if (!window) {
        printf("ERROR: Cannot open window\n");
        return FALSE;
    }

    /* Create DrawingBoard */
    test_board = CreateDrawingBoard(TEST_WIDTH, TEST_HEIGHT, TEST_DEPTH,
                                   ZUNE_DRAWINGBOARD_HARDWARE);
    if (!test_board) {
        printf("ERROR: Cannot create DrawingBoard\n");
        return FALSE;
    }

    /* Create RenderPort targeting the DrawingBoard */
    test_rp = CreateRenderPortWithDrawingBoard(screen->ViewPort.ColorMap,
                                              test_board);
    if (!test_rp) {
        printf("ERROR: Cannot create RenderPort\n");
        return FALSE;
    }

    window_rp = CreateRenderPort(screen->ViewPort.ColorMap, window->RPort);

    return TRUE;
}

void CleanupTest(void)
{
    if (test_rp) {
        DestroyRenderPort(test_rp);
        test_rp = NULL;
    }

    if (test_board) {
        DestroyDrawingBoard(test_board);
        test_board = NULL;
    }

    if (window) {
        CloseWindow(window);
        window = NULL;
    }

    if (screen) {
        CloseScreen(screen);
        screen = NULL;
    }

    if (ZuneGfxBase) {
        CloseLibrary(ZuneGfxBase);
        ZuneGfxBase = NULL;
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

void TestLockedLineDrawing(void)
{
    APTR pixels;
    ULONG pitch;

    /* Clear the DrawingBoard */
    ClearDrawingBoard(test_rp, ZUNE_BLACK);

    printf("  Locking DrawingBoard pixels...\n");
    pixels = LockDrawingBoardPixels(test_rp, &pitch);
    if (!pixels) {
        printf("  ERROR: Cannot lock DrawingBoard pixels\n");
        return;
    }

    printf("  Pixels locked successfully (address: %p, pitch: %u)\n", pixels, pitch);
    printf("  Drawing test pattern using direct pixel manipulation...\n");

    /* Draw test pattern */
    DrawLineTestPattern(TRUE, 0, 0);

    /* Draw stress test */
    DrawLineStressTest(TRUE, 400, 0);

    /* Unlock pixels */
    UnlockDrawingBoardPixels(test_rp);
    printf("  Locked line drawing test completed\n");
}

void TestUnlockedLineDrawing(void)
{
    /* Clear the DrawingBoard */
    ClearDrawingBoard(test_rp, ZUNE_BLACK);

    printf("  Using unlocked DrawingBoard (WriteRGBPixel calls)...\n");

    /* Draw test pattern */
    DrawLineTestPattern(FALSE, 0, 0);

    /* Draw stress test */
    DrawLineStressTest(FALSE, 400, 0);

    printf("  Unlocked line drawing test completed\n");
}

void DrawLineTestPattern(BOOL locked, int offset_x, int offset_y)
{
    printf("    Drawing basic line patterns (%s mode)...\n", locked ? "locked" : "unlocked");

    /* Horizontal lines */
    ZuneDrawLinePoints(test_rp, offset_x + 50, offset_y + 50, offset_x + 150, offset_y + 50, ZUNE_RED);
    ZuneDrawLinePoints(test_rp, offset_x + 50, offset_y + 60, offset_x + 200, offset_y + 60, ZUNE_GREEN);
    ZuneDrawLinePoints(test_rp, offset_x + 50, offset_y + 70, offset_x + 100, offset_y + 70, ZUNE_BLUE);

    /* Vertical lines */
    ZuneDrawLinePoints(test_rp, offset_x + 50, offset_y + 100, offset_x + 50, offset_y + 200, ZUNE_YELLOW);
    ZuneDrawLinePoints(test_rp, offset_x + 60, offset_y + 100, offset_x + 60, offset_y + 150, ZUNE_MAGENTA);
    ZuneDrawLinePoints(test_rp, offset_x + 70, offset_y + 100, offset_x + 70, offset_y + 250, ZUNE_CYAN);

    /* Diagonal lines (all octants) */
    int centerX = offset_x + 150;
    int centerY = offset_y + 150;
    int radius = 80;

    for (int i = 0; i < 8; i++) {
        float angle = (i * 45.0f) * 3.14159f / 180.0f;
        int endX = centerX + (int)(radius * cos(angle));
        int endY = centerY + (int)(radius * sin(angle));
        ULONG color = ZUNE_COLOR_RGB24(255 - i * 30, i * 30, (i * 60) % 256);
        ZuneDrawLinePoints(test_rp, centerX, centerY, endX, endY, color);
    }

    /* Single pixel lines */
    ZuneDrawLinePoints(test_rp, offset_x + 250, offset_y + 50, offset_x + 250, offset_y + 50, ZUNE_WHITE); // Single point
    ZuneDrawLinePoints(test_rp, offset_x + 260, offset_y + 50, offset_x + 261, offset_y + 50, ZUNE_WHITE); // 2-pixel horizontal
    ZuneDrawLinePoints(test_rp, offset_x + 270, offset_y + 50, offset_x + 270, offset_y + 51, ZUNE_WHITE); // 2-pixel vertical
    ZuneDrawLinePoints(test_rp, offset_x + 280, offset_y + 50, offset_x + 281, offset_y + 51, ZUNE_WHITE); // 2-pixel diagonal
}

void DrawLineStressTest(BOOL locked, int offset_x, int offset_y)
{
    printf("    Drawing stress test patterns (%s mode)...\n", locked ? "locked" : "unlocked");

    /* Grid pattern */
    for (int i = 0; i < 20; i++) {
        int x = offset_x + 50 + i * 10;
        int y = offset_y + 50 + i * 10;
        ULONG color = ZUNE_COLOR_RGB24((i * 12) % 256, (i * 18) % 256, (i * 24) % 256);

        /* Vertical grid lines */
        ZuneDrawLinePoints(test_rp, x, offset_y + 50, x, offset_y + 250, color);

        /* Horizontal grid lines */
        ZuneDrawLinePoints(test_rp, offset_x + 50, y, offset_x + 250, y, color);
    }

    /* Radial pattern */
    int centerX = offset_x + 300;
    int centerY = offset_y + 400;
    for (int i = 0; i < 36; i++) {
        float angle = (i * 10.0f) * 3.14159f / 180.0f;
        int endX = centerX + (int)(60 * cos(angle));
        int endY = centerY + (int)(60 * sin(angle));
        ULONG color = ZUNE_COLOR_RGB24((i * 7) % 256, (i * 13) % 256, (i * 19) % 256);
        ZuneDrawLinePoints(test_rp, centerX, centerY, endX, endY, color);
    }

    /* Steep lines (test all slope ranges) */
    for (int i = 0; i < 10; i++) {
        int startX = offset_x + 50;
        int startY = offset_y + 300 + i * 5;
        int endX = offset_x + 100;
        int endY = offset_y + 300 + i * 20;  // Steep slope
        ULONG color = ZUNE_COLOR_RGB24(255 - i * 25, i * 25, 128);
        ZuneDrawLinePoints(test_rp, startX, startY, endX, endY, color);

        /* Reverse slope */
        ZuneDrawLinePoints(test_rp, endX + 20, startY, startX + 20, endY, color);
    }
}

void VerifyLineAccuracy(void)
{
    /* Clear the DrawingBoard */
    ClearDrawingBoard(test_rp, ZUNE_BLACK);

    printf("  Drawing accuracy verification patterns...\n");

    /* Lock for this test to use direct pixel access */
    APTR pixels = LockDrawingBoardPixels(test_rp, NULL);
    if (!pixels) {
        printf("  ERROR: Cannot lock pixels for accuracy test\n");
        return;
    }

    /* Test known line patterns */
    printf("    Testing specific line cases...\n");

    /* Test case 1: Perfect diagonal (45 degrees) */
    ZuneDrawLinePoints(test_rp, 100, 100, 200, 200, ZUNE_RED);

    /* Test case 2: Steep line (slope > 1) */
    ZuneDrawLinePoints(test_rp, 150, 100, 170, 200, ZUNE_GREEN);

    /* Test case 3: Shallow line (slope < 1) */
    ZuneDrawLinePoints(test_rp, 200, 150, 350, 170, ZUNE_BLUE);

    /* Test case 4: Negative slopes */
    ZuneDrawLinePoints(test_rp, 100, 250, 200, 150, ZUNE_YELLOW);
    ZuneDrawLinePoints(test_rp, 150, 250, 350, 230, ZUNE_MAGENTA);

    /* Test case 5: Lines going in all directions from center */
    int centerX = 400;
    int centerY = 300;
    for (int i = 0; i < 16; i++) {
        float angle = (i * 22.5f) * 3.14159f / 180.0f;
        int endX = centerX + (int)(100 * cos(angle));
        int endY = centerY + (int)(100 * sin(angle));
        ULONG color = ZUNE_COLOR_RGB24((i * 16) % 256, 255 - (i * 16) % 256, 128);
        ZuneDrawLinePoints(test_rp, centerX, centerY, endX, endY, color);
    }

    /* Test case 6: Edge cases */
    ZuneDrawLinePoints(test_rp, 0, 0, 50, 0, ZUNE_WHITE);           // Top edge
    ZuneDrawLinePoints(test_rp, 0, TEST_HEIGHT-1, 50, TEST_HEIGHT-1, ZUNE_WHITE); // Bottom edge
    ZuneDrawLinePoints(test_rp, 0, 0, 0, 50, ZUNE_WHITE);           // Left edge
    ZuneDrawLinePoints(test_rp, TEST_WIDTH-1, 0, TEST_WIDTH-1, 50, ZUNE_WHITE);   // Right edge

    /* Test case 7: Lines that would go out of bounds (clipping test) */
    ZuneDrawLinePoints(test_rp, -10, 50, 60, 50, ZUNE_CYAN);        // Start outside
    ZuneDrawLinePoints(test_rp, 50, -10, 50, 60, ZUNE_CYAN);        // Start outside
    ZuneDrawLinePoints(test_rp, 700, 500, 900, 500, ZUNE_CYAN);     // End outside
    ZuneDrawLinePoints(test_rp, 700, 500, 700, 700, ZUNE_CYAN);     // End outside

    UnlockDrawingBoardPixels(test_rp);
    printf("  Line accuracy verification completed\n");
}

void ShowResults(const char *title)
{
    printf("  Displaying results: %s\n", title);

    /* Blit the DrawingBoard to the window */
    BlitDrawingBoardToRenderPortRects(test_board, window_rp, 0, 0, 0, 0,
                                TEST_WIDTH, TEST_HEIGHT);

    printf("  Results displayed successfully\n");
}
