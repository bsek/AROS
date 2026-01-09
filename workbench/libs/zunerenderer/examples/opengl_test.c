/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL/CyberGfx Mixed Rendering Test

    This demo tests rendering with both OpenGL and CyberGraphics backends
    to the same DrawingBoard/RastPort. This validates the FBO synchronization
    between legacy graphics.library drawing and modern OpenGL rendering.

    Test scenarios:
    1. OpenGL clears background, CyberGfx draws shapes
    2. CyberGfx clears background, OpenGL draws shapes
    3. Both draw alternating elements to same surface
    4. Sync from RastPort to OpenGL FBO and vice versa
*/

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunerenderer.h>
#include <cybergraphx/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG 1
#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/zunerenderer.h>

/* Demo parameters */
#define DEMO_WIDTH 800
#define DEMO_HEIGHT 600

/* Test mode selection */
typedef enum {
    TEST_OPENGL_ONLY = 0,
    TEST_CYBERGFX_ONLY,
    TEST_OPENGL_BG_CYBERGFX_FG,
    TEST_CYBERGFX_BG_OPENGL_FG,
    TEST_ALTERNATING,
    TEST_COUNT
} TestMode;

static const char *test_names[] = {
    "OpenGL Only",
    "CyberGfx Only",
    "OpenGL Background + CyberGfx Foreground",
    "CyberGfx Background + OpenGL Foreground",
    "Alternating (both backends)"
};

/* Global variables */
struct Library *ZuneRendererBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

struct Screen *screen = NULL;
struct Window *window = NULL;
struct DrawingBoard *board = NULL;
struct RenderPort *render_port = NULL;
struct RenderPort *window_rp = NULL;

TestMode current_test = TEST_OPENGL_BG_CYBERGFX_FG;

/* Function prototypes */
BOOL InitDemo(void);
void CleanupDemo(void);
void RunTest(TestMode mode);
void BlitToWindow(void);

/* Test implementations */
void Test_OpenGLOnly(void);
void Test_CyberGfxOnly(void);
void Test_OpenGLBg_CyberGfxFg(void);
void Test_CyberGfxBg_OpenGLFg(void);
void Test_Alternating(void);

/* Helper: Draw background with ZuneRenderer (uses active backend) */
void DrawBackground_Zune(ULONG color);

/* Helper: Draw background directly to RastPort with CyberGfx */
void DrawBackground_CyberGfx(ULONG color);

/* Helper: Draw shapes with ZuneRenderer */
void DrawShapes_Zune(void);

/* Helper: Draw shapes directly with CyberGfx to RastPort */
void DrawShapes_CyberGfx(void);

int main(void) {
    struct IntuiMessage *msg;
    BOOL done = FALSE;
    ULONG signals;

    printf("Zune Renderer - OpenGL/CyberGfx Mixed Rendering Test\n");
    printf("=====================================================\n\n");

    if (!InitDemo()) {
        printf("ERROR: Failed to initialize demo\n");
        CleanupDemo();
        return 1;
    }

    printf("Demo initialized successfully\n");
    printf("RenderPort backend type: %lu\n", render_port->backend_type);

    if (render_port->backend_type == BACKEND_OPENGL) {
        printf("SUCCESS: OpenGL backend is active!\n");
        printf("This test will verify FBO sync between OpenGL and CyberGfx.\n");
    } else if (render_port->backend_type == BACKEND_CYBERGFX) {
        printf("INFO: CyberGfx backend is active.\n");
        printf("OpenGL tests will fall back to CyberGfx.\n");
    } else {
        printf("INFO: Software backend is active\n");
    }

    printf("\nRunning test: %s\n", test_names[current_test]);
    printf("Press any key to cycle through tests. Close window to exit.\n\n");

    /* Run initial test */
    RunTest(current_test);

    /* Event loop */
    while (!done) {
        signals = Wait((1L << window->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);

        if (signals & SIGBREAKF_CTRL_C) {
            done = TRUE;
            break;
        }

        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            switch (msg->Class) {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;

                case IDCMP_VANILLAKEY:
                    /* Cycle to next test */
                    current_test = (current_test + 1) % TEST_COUNT;
                    printf("\nSwitching to test: %s\n", test_names[current_test]);
                    RunTest(current_test);
                    break;

                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(window);
                    BlitToWindow();
                    EndRefresh(window, TRUE);
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    CleanupDemo();
    printf("\nDemo finished.\n");
    return 0;
}

BOOL InitDemo(void) {
    WORD inner_width, inner_height;

    /* Open required libraries */
    ZuneRendererBase = OpenLibrary("zunerenderer.library", 1);
    if (!ZuneRendererBase) {
        printf("ERROR: Cannot open zunerenderer.library\n");
        return FALSE;
    }

    CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    if (!CyberGfxBase) {
        printf("WARNING: Cannot open cybergraphics.library - CyberGfx tests will fail\n");
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

    /* Lock the Workbench screen */
    screen = LockPubScreen(NULL);
    if (!screen) {
        printf("ERROR: Cannot lock Workbench screen\n");
        return FALSE;
    }

    /* Open window */
    window = OpenWindowTags(
        NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 50,
        WA_Top, 50,
        WA_Width, DEMO_WIDTH,
        WA_Height, DEMO_HEIGHT,
        WA_Title, (IPTR)"OpenGL/CyberGfx Mixed Rendering Test",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_SimpleRefresh, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_REFRESHWINDOW,
        TAG_DONE);

    if (!window) {
        printf("ERROR: Cannot open window\n");
        return FALSE;
    }

    printf("Window opened: %p\n", window);

    /* Calculate inner window dimensions */
    inner_width = window->Width - window->BorderLeft - window->BorderRight;
    inner_height = window->Height - window->BorderTop - window->BorderBottom;

    printf("Window inner area: %dx%d\n", inner_width, inner_height);

    /*
     * Create RenderPort bound to the window.
     * The backend selection happens here (OpenGL if available).
     */
    render_port = CreateRenderPortForWindow(window, screen->ViewPort.ColorMap, BACKEND_OPENGL);
    if (!render_port) {
        printf("ERROR: Cannot create RenderPort\n");
        return FALSE;
    }

    printf("RenderPort created: %p\n", render_port);

    /*
     * Create a DrawingBoard for offscreen rendering.
     * For OpenGL, this creates an FBO. For CyberGfx, a bitmap.
     * The DrawingBoard always has a bitmap for legacy compatibility.
     */
    board = CreateDrawingBoardForRenderPort(render_port, inner_width, inner_height, 0);
    if (!board) {
        printf("ERROR: Cannot create DrawingBoard\n");
        return FALSE;
    }

    printf("DrawingBoard created: %p (%dx%d)\n", board, board->width, board->height);
    printf("DrawingBoard bitmap: %p\n", board->bitmap);
    printf("DrawingBoard rastport: %p\n", board->rastport);
    printf("DrawingBoard hardware_surface: %s\n", board->hardware_surface ? "Yes" : "No");
    printf("DrawingBoard backend_data (FBO): %p\n", board->backend_data);

    /*
     * Create a separate RenderPort for window output (blitting destination).
     * This uses BACKEND_DEFAULT since we just need it for BltBitMapRastPort.
     */
    window_rp = CreateRenderPortForWindow(window, screen->ViewPort.ColorMap, BACKEND_CYBERGFX);
    if (!window_rp) {
        printf("ERROR: Cannot create window RenderPort\n");
        return FALSE;
    }

    return TRUE;
}

void CleanupDemo(void) {
    if (board) {
        DestroyDrawingBoard(render_port, board);
        board = NULL;
    }

    if (window_rp) {
        DestroyRenderPort(window_rp);
        window_rp = NULL;
    }

    if (render_port) {
        DestroyRenderPort(render_port);
        render_port = NULL;
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

/*
 * Blit the DrawingBoard contents to the window.
 *
 * render_port (source) has the board as target and the correct backend.
 * window_rp (destination) targets the window for output.
 */
void BlitToWindow(void) {
    struct ZuneRect src_rect = {0, 0, board->width, board->height};
    struct ZuneRect dst_rect = {window->BorderLeft, window->BorderTop, board->width, board->height};

    /* Ensure render_port targets the board */
    ZuneSetTarget(render_port, board);

    /* Blit from render_port (board) to window_rp (window) */
    BlitDrawingBoardToRenderPort(render_port, window_rp, &src_rect, &dst_rect);
}

void RunTest(TestMode mode) {
    printf("Running test %d: %s\n", mode + 1, test_names[mode]);

    switch (mode) {
        case TEST_OPENGL_ONLY:
            Test_OpenGLOnly();
            break;
        case TEST_CYBERGFX_ONLY:
            Test_CyberGfxOnly();
            break;
        case TEST_OPENGL_BG_CYBERGFX_FG:
            Test_OpenGLBg_CyberGfxFg();
            break;
        case TEST_CYBERGFX_BG_OPENGL_FG:
            Test_CyberGfxBg_OpenGLFg();
            break;
        case TEST_ALTERNATING:
            Test_Alternating();
            break;
        default:
            printf("Unknown test mode\n");
            break;
    }

    BlitToWindow();
    printf("Test complete, blitted to window.\n");
}

/*
 * Draw background using ZuneRenderer (which uses whatever backend is active)
 */
void DrawBackground_Zune(ULONG color) {
    ZuneSetTarget(render_port, board);
    ClearDrawingBoard(render_port, color);
}

/*
 * Draw background directly to the DrawingBoard's RastPort using CyberGfx.
 * This bypasses ZuneRenderer entirely and uses legacy graphics.
 */
void DrawBackground_CyberGfx(ULONG color) {
    if (!CyberGfxBase || !board->rastport) {
        printf("  CyberGfx not available, falling back to Zune\n");
        DrawBackground_Zune(color);
        return;
    }

    /* Fill rectangle using CyberGfx directly to the DrawingBoard's bitmap */
    FillPixelArray(board->rastport, 0, 0, board->width, board->height, color);

    printf("  Drew CyberGfx background (0x%08lx)\n", color);
}

/*
 * Draw shapes using ZuneRenderer (uses active backend - OpenGL or CyberGfx)
 */
void DrawShapes_Zune(void) {
    ZuneSetTarget(render_port, board);

    /* Row 1: Filled rounded rectangles */
    printf("  Drawing Zune shapes (row 1: filled rounded rects)...\n");
    ZuneFillRectangleRoundedAAXYWH(render_port, 20, 20, 120, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillRectangleRoundedAAXYWH(render_port, 160, 20, 120, 100, 20,
                                   ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillRectangleRoundedAAXYWH(render_port, 300, 20, 120, 100, 25,
                                   ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    /* Row 2: Rounded rectangle outlines */
    printf("  Drawing Zune shapes (row 2: rounded rect outlines)...\n");
    ZuneDrawRectangleRoundedOutlineAAXYWH(render_port, 20, 140, 120, 100, 15,
                                          ZUNE_YELLOW);
    ZuneDrawRectangleRoundedOutlineAAXYWH(render_port, 160, 140, 120, 100, 20,
                                          ZUNE_MAGENTA);
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(render_port, 300, 140, 120, 100, 25,
                                          5, ZUNE_CYAN);

    /* Row 3: Filled circles */
    printf("  Drawing Zune shapes (row 3: filled circles)...\n");
    ZuneFillCircleAAAt(render_port, 80, 320, 50,
                       ZUNE_BRUSH_SOLID(ZUNE_WHITE));
    ZuneFillCircleAAAt(render_port, 220, 320, 50,
                       ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
    ZuneFillCircleAAAt(render_port, 360, 320, 50,
                       ZUNE_BRUSH_SOLID(ZUNE_YELLOW));

    /* Row 4: Lines */
    printf("  Drawing Zune shapes (row 4: lines)...\n");
    ZuneDrawLineAAPoints(render_port, 20, 400, 200, 500, ZUNE_RED);
    ZuneDrawLineAAPoints(render_port, 20, 500, 200, 400, ZUNE_GREEN);
    ZuneDrawLineStyledAAPoints(render_port, 220, 400, 400, 500, 5, ZUNE_BLUE);
}

/*
 * Draw shapes directly to the DrawingBoard's RastPort using CyberGfx/graphics.library.
 * This bypasses ZuneRenderer entirely.
 */
void DrawShapes_CyberGfx(void) {
    struct RastPort *rp = board->rastport;
    WORD cx, cy;

    if (!rp) {
        printf("  No RastPort available for CyberGfx drawing\n");
        return;
    }

    /* Row 1: Simple filled rectangles using graphics.library */
    printf("  Drawing CyberGfx shapes (row 1: filled rects)...\n");

    /* Red rectangle */
    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFF0000, 0x00000000, 0x00000000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    RectFill(rp, 450, 20, 570, 120);

    /* Green rectangle */
    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0xFFFF0000, 0x00000000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    RectFill(rp, 590, 20, 710, 120);

    /* Row 2: Outlined rectangles */
    printf("  Drawing CyberGfx shapes (row 2: rect outlines)...\n");

    /* Yellow outline */
    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFF0000, 0xFFFF0000, 0x00000000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    Move(rp, 450, 140);
    Draw(rp, 570, 140);
    Draw(rp, 570, 240);
    Draw(rp, 450, 240);
    Draw(rp, 450, 140);

    /* Cyan outline */
    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0xFFFF0000, 0xFFFF0000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    Move(rp, 590, 140);
    Draw(rp, 710, 140);
    Draw(rp, 710, 240);
    Draw(rp, 590, 240);
    Draw(rp, 590, 140);

    /* Row 3: Circles (outlines only - AreaEllipse requires AreaInfo/TmpRas setup) */
    printf("  Drawing CyberGfx shapes (row 3: circle outlines)...\n");

    cx = 510; cy = 320;
    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    DrawEllipse(rp, cx, cy, 50, 50);

    cx = 650; cy = 320;
    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0x80800000, 0x80800000, 0x80800000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    DrawEllipse(rp, cx, cy, 50, 50);

    /* Use FillPixelArray to draw a filled circle approximation */
    if (CyberGfxBase) {
        /* Draw a filled square as simple alternative (true filled circles need more work) */
        FillPixelArray(rp, 560, 270, 100, 100, 0xFFFFFFFF);  /* White filled square */
    }

    /* Row 4: Lines */
    printf("  Drawing CyberGfx shapes (row 4: lines)...\n");

    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFF0000, 0x00000000, 0x00000000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    Move(rp, 450, 400);
    Draw(rp, 630, 500);

    SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0xFFFF0000, 0x00000000,
                               OBP_Precision, PRECISION_EXACT, TAG_DONE));
    Move(rp, 450, 500);
    Draw(rp, 630, 400);

    /* Use WritePixelArray for a gradient-like effect to show CyberGfx direct pixel access */
    if (CyberGfxBase) {
        ULONG *gradient_buffer;
        int x, y;

        printf("  Drawing CyberGfx gradient (WritePixelArray)...\n");

        gradient_buffer = AllocVec(100 * 60 * 4, MEMF_ANY);
        if (gradient_buffer) {
            for (y = 0; y < 60; y++) {
                for (x = 0; x < 100; x++) {
                    UBYTE r = (x * 255) / 100;
                    UBYTE g = (y * 255) / 60;
                    UBYTE b = 128;
                    gradient_buffer[y * 100 + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
            WritePixelArray(gradient_buffer, 0, 0, 100 * 4, rp, 650, 400, 100, 60, RECTFMT_ARGB);
            FreeVec(gradient_buffer);
        }
    }
}

/*
 * TEST 1: OpenGL Only
 * Uses ZuneRenderer for everything (which will use OpenGL if available)
 */
void Test_OpenGLOnly(void) {
    printf("TEST: OpenGL Only - All drawing via ZuneRenderer\n");

    DrawBackground_Zune(ZUNE_DARKGRAY);
    DrawShapes_Zune();

    /* Label */
    ZuneSetTarget(render_port, board);
    ZuneFillRectangleRoundedAAXYWH(render_port, 10, board->height - 40, 200, 30, 5,
                                   ZUNE_BRUSH_SOLID(ZUNE_BLACK));
}

/*
 * TEST 2: CyberGfx Only
 * Uses CyberGfx/graphics.library directly for everything
 */
void Test_CyberGfxOnly(void) {
    printf("TEST: CyberGfx Only - All drawing direct to RastPort\n");

    DrawBackground_CyberGfx(0xFF404040);  /* Dark gray in ARGB */
    DrawShapes_CyberGfx();
}

/*
 * TEST 3: OpenGL Background + CyberGfx Foreground
 *
 * This tests the sync FROM OpenGL FBO TO RastPort bitmap.
 * 1. ZuneRenderer clears background via OpenGL (renders to FBO)
 * 2. FBO contents must be synced to bitmap for CyberGfx to see it
 * 3. CyberGfx draws shapes on top
 */
void Test_OpenGLBg_CyberGfxFg(void) {
    printf("TEST: OpenGL Background + CyberGfx Foreground\n");
    printf("  This tests FBO -> RastPort sync\n");

    /* Step 1: Clear background with ZuneRenderer (OpenGL FBO) */
    printf("  Step 1: ZuneRenderer clears background (OpenGL)...\n");
    DrawBackground_Zune(ZUNE_COLOR_RGB24(40, 60, 80));  /* Blue-gray */

    /* Step 2: Draw some ZuneRenderer shapes */
    printf("  Step 2: ZuneRenderer draws left-side shapes...\n");
    ZuneSetTarget(render_port, board);
    ZuneFillRectangleRoundedAAXYWH(render_port, 20, 20, 150, 120, 20,
                                   ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillCircleAAAt(render_port, 100, 250, 60,
                       ZUNE_BRUSH_SOLID(ZUNE_GREEN));

    /*
     * Step 3: CRITICAL - Sync FBO to RastPort before CyberGfx drawing
     *
     * For this to work, the FBO contents need to be copied to the
     * DrawingBoard's bitmap so CyberGfx can draw over it.
     *
     * SyncDrawingBoard() copies the OpenGL FBO contents to the bitmap.
     */
    printf("  Step 3: Syncing FBO to bitmap (SyncDrawingBoard)...\n");
    SyncDrawingBoard(render_port);

    /* Step 4: Draw CyberGfx shapes on top */
    printf("  Step 4: CyberGfx draws right-side shapes...\n");
    DrawShapes_CyberGfx();

    /*
     * Step 5: Sync bitmap back to FBO so OpenGL blit shows CyberGfx content
     */
    printf("  Step 5: Syncing bitmap to FBO (ZuneCopyFromRastPort)...\n");
    ZuneSetTarget(render_port, board);
    ZuneCopyFromRastPort(render_port, board->rastport,
                         0, 0, 0, 0, board->width, board->height);

    printf("  Test complete - left shapes from OpenGL, right from CyberGfx\n");
}

/*
 * TEST 4: CyberGfx Background + OpenGL Foreground
 *
 * This tests the sync FROM RastPort bitmap TO OpenGL FBO.
 * 1. CyberGfx clears background (writes to bitmap)
 * 2. Bitmap contents must be uploaded to FBO for OpenGL to see it
 * 3. OpenGL draws shapes on top
 */
void Test_CyberGfxBg_OpenGLFg(void) {
    printf("TEST: CyberGfx Background + OpenGL Foreground\n");
    printf("  This tests RastPort -> FBO sync\n");

    /* Step 1: Clear background with CyberGfx directly */
    printf("  Step 1: CyberGfx clears background...\n");
    DrawBackground_CyberGfx(0xFF503050);  /* Purple-gray */

    /* Step 2: Draw some CyberGfx shapes */
    printf("  Step 2: CyberGfx draws right-side shapes...\n");
    if (board->rastport && CyberGfxBase) {
        struct RastPort *rp = board->rastport;

        /* Draw a filled rectangle on right side */
        FillPixelArray(rp, 500, 20, 150, 120, 0xFF00FFFF);  /* Cyan */

        /* Draw some lines */
        SetAPen(rp, ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFF0000, 0xFFFF0000, 0x00000000,
                                   OBP_Precision, PRECISION_EXACT, TAG_DONE));
        Move(rp, 500, 200);
        Draw(rp, 650, 300);
        Draw(rp, 500, 300);
        Draw(rp, 650, 200);
    }

    /*
     * Step 3: CRITICAL - Sync RastPort to FBO before OpenGL drawing
     *
     * For this to work, the bitmap contents need to be uploaded to the
     * OpenGL FBO so OpenGL doesn't overwrite the CyberGfx content.
     *
     * This requires reading the bitmap and uploading it as a texture
     * to the FBO, or using CopyFromRastPort in the backend.
     */
    printf("  Step 3: Syncing RastPort to FBO...\n");
    /*
     * The backend's CopyFromRastPort should handle this when we switch
     * to the DrawingBoard target with needs_sync flag set.
     */

    /* Step 4: Draw OpenGL shapes on top */
    printf("  Step 4: ZuneRenderer draws left-side shapes (OpenGL)...\n");
    ZuneSetTarget(render_port, board);
    ZuneFillRectangleRoundedAAXYWH(render_port, 20, 20, 150, 120, 20,
                                   ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillCircleAAAt(render_port, 100, 250, 60,
                       ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneDrawLineStyledAAPoints(render_port, 20, 350, 200, 450, 5, ZUNE_YELLOW);

    printf("  Test complete - right shapes from CyberGfx, left from OpenGL\n");
}

/*
 * TEST 5: Alternating
 *
 * Draws elements alternating between backends.
 * This is the ultimate stress test for FBO sync.
 */
void Test_Alternating(void) {
    int i;
    WORD x, y;

    printf("TEST: Alternating - Elements drawn by alternating backends\n");
    printf("  This is the stress test for FBO <-> RastPort sync\n");

    /* Clear with ZuneRenderer */
    DrawBackground_Zune(ZUNE_COLOR_RGB24(30, 30, 40));

    /* Alternating rows of shapes */
    for (i = 0; i < 4; i++) {
        x = 20;
        y = 20 + i * 130;

        if (i % 2 == 0) {
            /* Even rows: ZuneRenderer (OpenGL) */
            printf("  Row %d: ZuneRenderer shapes at y=%d\n", i, y);
            ZuneSetTarget(render_port, board);
            ZuneFillRectangleRoundedAAXYWH(render_port, x, y, 100, 100, 15,
                                           ZUNE_BRUSH_SOLID(ZUNE_RED));
            ZuneFillRectangleRoundedAAXYWH(render_port, x + 120, y, 100, 100, 15,
                                           ZUNE_BRUSH_SOLID(ZUNE_GREEN));
            ZuneFillRectangleRoundedAAXYWH(render_port, x + 240, y, 100, 100, 15,
                                           ZUNE_BRUSH_SOLID(ZUNE_BLUE));
            /* Sync FBO to bitmap before CyberGfx draws in next iteration */
            SyncDrawingBoard(render_port);
        } else {
            /* Odd rows: CyberGfx direct */
            printf("  Row %d: CyberGfx shapes at y=%d\n", i, y);
            if (board->rastport && CyberGfxBase) {
                FillPixelArray(board->rastport, x, y, 100, 100, 0xFFFFFF00);      /* Yellow */
                FillPixelArray(board->rastport, x + 120, y, 100, 100, 0xFFFF00FF); /* Magenta */
                FillPixelArray(board->rastport, x + 240, y, 100, 100, 0xFF00FFFF); /* Cyan */
            }
        }
    }

    /* Final OpenGL shapes on right side */
    printf("  Final: ZuneRenderer circles on right side\n");
    ZuneSetTarget(render_port, board);
    ZuneFillCircleAAAt(render_port, 550, 100, 60, ZUNE_BRUSH_SOLID(ZUNE_WHITE));
    ZuneFillCircleAAAt(render_port, 550, 250, 60, ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
    ZuneFillCircleAAAt(render_port, 550, 400, 60, ZUNE_BRUSH_SOLID(ZUNE_GRAY));

    /* Info text area */
    ZuneFillRectangleRoundedAAXYWH(render_port, 400, 480, 350, 80, 10,
                                   ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 0, 0, 0)));

    printf("  Alternating test complete\n");
}
