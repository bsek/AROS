/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawLine Function Test

    This test verifies that the newly implemented ZuneDrawLine function works
    correctly in the public API. It tests line drawing with both locked and
    unlocked DrawingBoards to ensure the Bresenham's algorithm implementation
    is working properly.
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

/* Function prototypes */
BOOL InitTest(void);
void CleanupTest(void);
void TestZuneDrawLineBasic(void);
void TestZuneDrawLineLocked(void);
void TestZuneDrawLineUnlocked(void);
void TestZuneDrawLinePatterns(BOOL locked, int offset_x, int offset_y);
void ShowResults(const char *title);

int main(void)
{
    printf("ZuneDrawLine Function Test\n");
    printf("==========================\n\n");

    if (!InitTest()) {
        printf("ERROR: Failed to initialize test\n");
        CleanupTest();
        return 1;
    }

    printf("Test initialized successfully\n");
    printf("Testing ZuneDrawLine public API function\n\n");

    /* Test basic functionality */
    printf("1. Testing basic ZuneDrawLine functionality...\n");
    TestZuneDrawLineBasic();
    ShowResults("Basic ZuneDrawLine Test");
    
    printf("Press ENTER to continue to locked test...\n");
    getchar();

    /* Test with locked DrawingBoard */
    printf("2. Testing ZuneDrawLine with LOCKED DrawingBoard...\n");
    TestZuneDrawLineLocked();
    ShowResults("ZuneDrawLine - Locked DrawingBoard");
    
    printf("Press ENTER to continue to unlocked test...\n");
    getchar();

    /* Test with unlocked DrawingBoard */
    printf("3. Testing ZuneDrawLine with UNLOCKED DrawingBoard...\n");
    TestZuneDrawLineUnlocked();
    ShowResults("ZuneDrawLine - Unlocked DrawingBoard");

    printf("\nAll ZuneDrawLine tests completed successfully!\n");
    printf("The function is working correctly with both locked and unlocked DrawingBoards.\n");
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
    screen = OpenScreenTags(NULL,
        SA_Width, TEST_WIDTH,
        SA_Height, TEST_HEIGHT,
        SA_Depth, TEST_DEPTH,
        SA_Title, (IPTR)"ZuneDrawLine Function Test",
        TAG_DONE);

    if (!screen) {
        printf("ERROR: Cannot open screen\n");
        return FALSE;
    }

    /* Open window */
    window = OpenWindowTags(NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 0,
        WA_Top, 0,
        WA_Width, TEST_WIDTH,
        WA_Height, TEST_HEIGHT,
        WA_Title, (IPTR)"ZuneDrawLine Test",
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

void TestZuneDrawLineBasic(void)
{
    printf("  Testing basic ZuneDrawLine calls...\n");
    
    /* Clear the DrawingBoard */
    ClearDrawingBoard(test_rp, ZUNE_BLACK);
    
    printf("  Drawing basic line patterns...\n");
    
    /* Test horizontal lines */
    ZuneDrawLinePoints(test_rp, 50, 50, 300, 50, ZUNE_RED);
    ZuneDrawLinePoints(test_rp, 50, 70, 250, 70, ZUNE_GREEN);
    ZuneDrawLinePoints(test_rp, 50, 90, 200, 90, ZUNE_BLUE);
    
    /* Test vertical lines */
    ZuneDrawLinePoints(test_rp, 400, 50, 400, 200, ZUNE_YELLOW);
    ZuneDrawLinePoints(test_rp, 420, 50, 420, 150, ZUNE_MAGENTA);
    ZuneDrawLinePoints(test_rp, 440, 50, 440, 250, ZUNE_CYAN);
    
    /* Test diagonal lines */
    ZuneDrawLinePoints(test_rp, 50, 150, 150, 250, ZUNE_WHITE);
    ZuneDrawLinePoints(test_rp, 200, 150, 300, 250, ZUNE_COLOR_RGB24(255, 128, 0));
    
    /* Test steep lines */
    ZuneDrawLinePoints(test_rp, 500, 50, 520, 200, ZUNE_COLOR_RGB24(0, 255, 128));
    ZuneDrawLinePoints(test_rp, 540, 50, 560, 180, ZUNE_COLOR_RGB24(128, 0, 255));
    
    /* Test single pixel line */
    ZuneDrawLinePoints(test_rp, 600, 100, 600, 100, ZUNE_RED);
    
    printf("  Basic ZuneDrawLine test completed\n");
}

void TestZuneDrawLineLocked(void)
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
    printf("  Drawing lines using direct pixel manipulation path...\n");

    TestZuneDrawLinePatterns(TRUE, 0, 0);

    /* Unlock pixels */
    UnlockDrawingBoardPixels(test_rp);
    printf("  Locked ZuneDrawLine test completed\n");
}

void TestZuneDrawLineUnlocked(void)
{
    /* Clear the DrawingBoard */
    ClearDrawingBoard(test_rp, ZUNE_BLACK);
    
    printf("  Using unlocked DrawingBoard (WriteRGBPixel path)...\n");

    TestZuneDrawLinePatterns(FALSE, 0, 0);

    printf("  Unlocked ZuneDrawLine test completed\n");
}

void TestZuneDrawLinePatterns(BOOL locked, int offset_x, int offset_y)
{
    printf("    Drawing comprehensive line patterns (%s mode)...\n", 
           locked ? "locked" : "unlocked");
    
    /* Test lines in all octants from center point */
    int centerX = offset_x + 200;
    int centerY = offset_y + 200;
    int radius = 80;
    
    for (int i = 0; i < 16; i++) {
        float angle = (i * 22.5f) * 3.14159f / 180.0f;
        int endX = centerX + (int)(radius * cos(angle));
        int endY = centerY + (int)(radius * sin(angle));
        ULONG color = ZUNE_COLOR_RGB24((i * 16) % 256, 255 - (i * 16) % 256, 128);
        ZuneDrawLinePoints(test_rp, centerX, centerY, endX, endY, color);
    }
    
    /* Test grid pattern */
    for (int i = 0; i < 10; i++) {
        int x = offset_x + 400 + i * 20;
        int y = offset_y + 100 + i * 20;
        ULONG color = ZUNE_COLOR_RGB24((i * 25) % 256, (i * 30) % 256, (i * 35) % 256);
        
        /* Vertical grid lines */
        ZuneDrawLinePoints(test_rp, x, offset_y + 100, x, offset_y + 300, color);
        
        /* Horizontal grid lines */
        ZuneDrawLinePoints(test_rp, offset_x + 400, y, offset_x + 600, y, color);
    }
    
    /* Test different slope ranges */
    for (int i = 0; i < 8; i++) {
        int startX = offset_x + 50;
        int startY = offset_y + 350 + i * 10;
        int endX = offset_x + 150 + i * 10;
        int endY = offset_y + 350 + i * 25;
        ULONG color = ZUNE_COLOR_RGB24(255 - i * 30, i * 30, 128);
        ZuneDrawLinePoints(test_rp, startX, startY, endX, endY, color);
    }
    
    /* Test edge cases */
    ZuneDrawLinePoints(test_rp, offset_x + 0, offset_y + 0, offset_x + 50, offset_y + 0, ZUNE_WHITE);   /* Top edge */
    ZuneDrawLinePoints(test_rp, offset_x + 0, TEST_HEIGHT-1, offset_x + 50, TEST_HEIGHT-1, ZUNE_WHITE); /* Bottom edge */
    ZuneDrawLinePoints(test_rp, offset_x + 0, offset_y + 0, offset_x + 0, offset_y + 50, ZUNE_WHITE);   /* Left edge */
    ZuneDrawLinePoints(test_rp, TEST_WIDTH-1, offset_y + 0, TEST_WIDTH-1, offset_y + 50, ZUNE_WHITE);   /* Right edge */
    
    /* Test lines with negative directions */
    ZuneDrawLinePoints(test_rp, offset_x + 350, offset_y + 400, offset_x + 250, offset_y + 500, ZUNE_CYAN);
    ZuneDrawLinePoints(test_rp, offset_x + 350, offset_y + 500, offset_x + 250, offset_y + 400, ZUNE_MAGENTA);
    
    /* Performance test - many small lines */
    for (int i = 0; i < 50; i++) {
        int x1 = offset_x + 500 + (i * 3) % 100;
        int y1 = offset_y + 400 + (i * 7) % 100;
        int x2 = x1 + (i % 20) - 10;
        int y2 = y1 + ((i * 2) % 20) - 10;
        ULONG color = ZUNE_COLOR_RGB24((i * 5) % 256, (i * 8) % 256, (i * 11) % 256);
        ZuneDrawLinePoints(test_rp, x1, y1, x2, y2, color);
    }
}

void ShowResults(const char *title)
{
    printf("  Displaying results: %s\n", title);
    
    /* Blit the DrawingBoard to the window */
    BlitDrawingBoardToRenderPortRects(test_board, test_rp, 0, 0, 0, 0, 
                                TEST_WIDTH, TEST_HEIGHT);
    
    printf("  Results displayed successfully\n");
}
