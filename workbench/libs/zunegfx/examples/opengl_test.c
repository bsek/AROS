/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Minimal OpenGL Test

    Stripped down test to isolate rendering issues.
*/

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunegfx.h>
#include <cybergraphx/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>

#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/zunegfx.h>

#define DEMO_WIDTH 640
#define DEMO_HEIGHT 480

/* Global variables */
struct Library *ZuneGfxBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

struct Screen *screen = NULL;
struct Window *window = NULL;
struct DrawingBoard *board = NULL;
struct RenderPort *render_port = NULL;
struct LayerCompositor *compositor = NULL;

BOOL InitDemo(void);
void CleanupDemo(void);
void RunTest(void);

int main(void) {
    struct IntuiMessage *msg;
    BOOL done = FALSE;
    ULONG signals;

    printf("Minimal OpenGL Test\n");
    printf("===================\n\n");

    if (!InitDemo()) {
        printf("ERROR: Failed to initialize\n");
        CleanupDemo();
        return 1;
    }

    printf("Initialized. Backend type: %lu\n", render_port->backend_type);
    printf("Press any key to run test. Close window to exit.\n\n");

    RunTest();

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
                    RunTest();
                    break;

                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(window);
                    ZuneSetTarget(render_port, board);
                    ZunePresent(render_port, 0, 0,
                                window->BorderLeft, window->BorderTop,
                                board->width, board->height);
                    EndRefresh(window, TRUE);
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    CleanupDemo();
    printf("Done.\n");
    return 0;
}

BOOL InitDemo(void) {
    WORD inner_width, inner_height;

    ZuneGfxBase = OpenLibrary("zunegfx.library", 1);
    if (!ZuneGfxBase) {
        printf("ERROR: Cannot open zunegfx.library\n");
        return FALSE;
    }

    CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);

    if (!IntuitionBase || !GfxBase) {
        printf("ERROR: Cannot open system libraries\n");
        return FALSE;
    }

    screen = LockPubScreen(NULL);
    if (!screen) {
        printf("ERROR: Cannot lock screen\n");
        return FALSE;
    }

    /* Create and activate layer compositor for this screen */
    printf("Creating layer compositor...\n");
    compositor = CreateLayerCompositor(screen);
    if (compositor) {
        if (ActivateLayerCompositor(compositor)) {
            printf("Layer compositor ACTIVATED\n");
        } else {
            printf("WARNING: Failed to activate compositor\n");
            DestroyLayerCompositor(compositor);
            compositor = NULL;
        }
    } else {
        printf("WARNING: Failed to create compositor\n");
    }

    window = OpenWindowTags(NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 50,
        WA_Top, 50,
        WA_Width, DEMO_WIDTH,
        WA_Height, DEMO_HEIGHT,
        WA_Title, (IPTR)"Minimal OpenGL Test (Alpha Window)",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_SimpleRefresh, TRUE,
        WA_Alpha, TRUE,           /* Enable compositor for this window */
        WA_AlphaValue, 200,       /* Slightly transparent (255=opaque) */
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_REFRESHWINDOW,
        TAG_DONE);

    if (!window) {
        printf("ERROR: Cannot open window\n");
        return FALSE;
    }

    inner_width = window->Width - window->BorderLeft - window->BorderRight;
    inner_height = window->Height - window->BorderTop - window->BorderBottom;

    render_port = CreateRenderPortForWindow(window, screen->ViewPort.ColorMap, BACKEND_OPENGL);
    if (!render_port) {
        printf("ERROR: Cannot create RenderPort\n");
        return FALSE;
    }

    board = CreateDrawingBoardForRenderPort(render_port, inner_width, inner_height, 0);
    if (!board) {
        printf("ERROR: Cannot create DrawingBoard\n");
        return FALSE;
    }

    printf("DrawingBoard: %dx%d, backend_data=%p\n",
           board->width, board->height, board->backend_data);

    ZuneSetTarget(render_port, board);
    return TRUE;
}

void CleanupDemo(void) {
    if (board) {
        DestroyDrawingBoard(render_port, board);
        board = NULL;
    }
    if (render_port) {
        DestroyRenderPort(render_port);
        render_port = NULL;
    }
    if (window) {
        CloseWindow(window);
        window = NULL;
    }
    if (compositor) {
        DeactivateLayerCompositor(compositor);
        DestroyLayerCompositor(compositor);
        compositor = NULL;
    }
    if (screen) {
        UnlockPubScreen(NULL, screen);
        screen = NULL;
    }
    if (ZuneGfxBase) CloseLibrary(ZuneGfxBase);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
}

static int test_num = 0;

/*
 * TEST 1: OpenGL only - draw 3 rectangles with ZuneRenderer
 */
void Test1_OpenGLOnly(void) {
    printf("\n--- Test 1: OpenGL Only ---\n");

    ZuneSetTarget(render_port, board);

    printf("1. Clearing background...\n");
    ClearDrawingBoard(render_port, ZUNE_COLOR_RGB24(30, 30, 40));

    printf("2. Drawing 3 OpenGL rectangles...\n");
    ZuneFillRectangleRoundedAAXYWH(render_port, 50, 150, 100, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillRectangleRoundedAAXYWH(render_port, 200, 150, 100, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillRectangleRoundedAAXYWH(render_port, 350, 150, 100, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    printf("3. Presenting...\n");
    ZunePresent(render_port, 0, 0,
                window->BorderLeft, window->BorderTop,
                board->width, board->height);
}

/*
 * TEST 2: CyberGfx only - draw directly to bitmap, then ZuneReload
 */
void Test2_CyberGfxOnly(void) {
    printf("\n--- Test 2: CyberGfx Only (with ZuneReload) ---\n");

    if (!CyberGfxBase || !board->rastport) {
        printf("ERROR: CyberGfx not available\n");
        return;
    }

    printf("1. Drawing 3 CyberGfx rectangles directly to bitmap...\n");
    FillPixelArray(board->rastport, 0, 0, board->width, board->height, 0xFF1E1E28);
    FillPixelArray(board->rastport, 50, 150, 100, 100, 0xFFFFFF00);   /* Yellow */
    FillPixelArray(board->rastport, 200, 150, 100, 100, 0xFFFF00FF);  /* Magenta */
    FillPixelArray(board->rastport, 350, 150, 100, 100, 0xFF00FFFF);  /* Cyan */

    printf("2. ZuneReload (upload bitmap to FBO)...\n");
    ZuneReload(render_port);

    printf("3. Presenting...\n");
    ZunePresent(render_port, 0, 0,
                window->BorderLeft, window->BorderTop,
                board->width, board->height);
}

/*
 * TEST 3: Mixed - OpenGL background, then CyberGfx shapes, then ZuneReload
 */
void Test3_OpenGLThenCyberGfx(void) {
    printf("\n--- Test 3: OpenGL bg + CyberGfx shapes ---\n");

    ZuneSetTarget(render_port, board);

    printf("1. OpenGL clears background...\n");
    ClearDrawingBoard(render_port, ZUNE_COLOR_RGB24(30, 30, 40));

    printf("2. ZuneSync (copy FBO to bitmap)...\n");
    ZuneSync(render_port);

    printf("3. CyberGfx draws 3 rectangles to bitmap...\n");
    FillPixelArray(board->rastport, 50, 150, 100, 100, 0xFFFFFF00);   /* Yellow */
    FillPixelArray(board->rastport, 200, 150, 100, 100, 0xFFFF00FF);  /* Magenta */
    FillPixelArray(board->rastport, 350, 150, 100, 100, 0xFF00FFFF);  /* Cyan */

    printf("4. ZuneReload (upload bitmap to FBO)...\n");
    ZuneReload(render_port);

    printf("5. Presenting...\n");
    ZunePresent(render_port, 0, 0,
                window->BorderLeft, window->BorderTop,
                board->width, board->height);
}

/*
 * TEST 4: CyberGfx then OpenGL - the problematic case
 * CyberGfx draws, ZuneReload, then OpenGL draws on top
 */
void Test4_CyberGfxThenOpenGL(void) {
    printf("\n--- Test 4: CyberGfx shapes + OpenGL on top ---\n");

    printf("1. CyberGfx clears and draws row 1 (yellow, magenta, cyan)...\n");
    FillPixelArray(board->rastport, 0, 0, board->width, board->height, 0xFF1E1E28);
    FillPixelArray(board->rastport, 50, 50, 100, 100, 0xFFFFFF00);    /* Yellow */
    FillPixelArray(board->rastport, 200, 50, 100, 100, 0xFFFF00FF);   /* Magenta */
    FillPixelArray(board->rastport, 350, 50, 100, 100, 0xFF00FFFF);   /* Cyan */

    printf("2. ZuneReload (upload bitmap to FBO)...\n");
    ZuneReload(render_port);

    printf("3. OpenGL draws row 2 (red, green, blue)...\n");
    ZuneSetTarget(render_port, board);
    ZuneFillRectangleRoundedAAXYWH(render_port, 50, 250, 100, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillRectangleRoundedAAXYWH(render_port, 200, 250, 100, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillRectangleRoundedAAXYWH(render_port, 350, 250, 100, 100, 15,
                                   ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    printf("4. Presenting...\n");
    ZunePresent(render_port, 0, 0,
                window->BorderLeft, window->BorderTop,
                board->width, board->height);
}

void RunTest(void) {
    test_num = (test_num % 4) + 1;

    switch (test_num) {
        case 1: Test1_OpenGLOnly(); break;
        case 2: Test2_CyberGfxOnly(); break;
        case 3: Test3_OpenGLThenCyberGfx(); break;
        case 4: Test4_CyberGfxThenOpenGL(); break;
    }

    printf("--- Test %d Complete. Press key for next test. ---\n", test_num);
}
