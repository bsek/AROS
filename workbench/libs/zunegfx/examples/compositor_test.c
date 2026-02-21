/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Layer Compositor Test - Alpha Window Compositing

    This demo tests the hybrid layer compositor:
    - Opens a normal (opaque) background window
    - Opens an alpha window on top using WA_Alpha
    - The alpha window is composited by the layer compositor
    - Standard windows render normally through the layer system
*/

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunegfx.h>
#include <cybergraphx/cybergraphics.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG 1
#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/zunegfx.h>

/* Compositor functions are now exported from zunegfx.library via proto/zunegfx.h */

/* Demo parameters */
#define BG_WIDTH 640
#define BG_HEIGHT 480
#define ALPHA_WIDTH 300
#define ALPHA_HEIGHT 200

/* WA_Alpha tag - must match intuition.h */
#ifndef WA_Alpha
#define WA_Alpha        (WA_Dummy + 150)
#endif
#ifndef WA_AlphaValue
#define WA_AlphaValue   (WA_Dummy + 151)
#endif

/* Global variables */
struct Library *ZuneGfxBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *LayersBase = NULL;

struct Screen *screen = NULL;
struct Window *bg_window = NULL;      /* Background (opaque) window */
struct Window *alpha_window = NULL;   /* Foreground (alpha) window */

struct DrawingBoard *bg_board = NULL;
struct DrawingBoard *alpha_board = NULL;
struct RenderContext *bg_rp = NULL;
struct RenderContext *alpha_rp = NULL;

struct LayerCompositor *compositor = NULL;

UBYTE current_alpha = 180;  /* Current alpha value for alpha window */

/* Function prototypes */
BOOL InitDemo(void);
void CleanupDemo(void);
void DrawBackgroundWindow(void);
void DrawAlphaWindow(void);
void UpdateAlphaValue(UBYTE new_alpha);
void CompositeAlphaWindow(void);

int main(void) {
    struct IntuiMessage *msg;
    BOOL done = FALSE;
    ULONG signals;
    ULONG win_mask;

    D(bug("[CompositorTest] Layer Compositor - Alpha Window Test\n"));
    D(bug("[CompositorTest] =====================================\n\n"));
    D(bug("[CompositorTest] This test demonstrates hybrid compositing:\n"));
    D(bug("[CompositorTest] - Background window: Normal layer system (opaque)\n"));
    D(bug("[CompositorTest] - Alpha window: Hardware-composited with transparency\n\n"));

    if (!InitDemo()) {
        D(bug("[CompositorTest] ERROR: Failed to initialize demo\n"));
        CleanupDemo();
        return 1;
    }

    D(bug("[CompositorTest] Demo initialized successfully!\n"));
    D(bug("[CompositorTest] Press +/- to adjust alpha (current: %d)\n", current_alpha));
    D(bug("[CompositorTest] Press R to refresh\n"));
    D(bug("[CompositorTest] Close either window to exit.\n\n"));

    /*
     * Ensure windows are fully ready before drawing.
     * Process any pending refresh messages first.
     */
    {
        struct IntuiMessage *imsg;
        while ((imsg = (struct IntuiMessage *)GetMsg(bg_window->UserPort))) {
            ReplyMsg((struct Message *)imsg);
        }
        if (alpha_window && alpha_window->UserPort) {
            while ((imsg = (struct IntuiMessage *)GetMsg(alpha_window->UserPort))) {
                ReplyMsg((struct Message *)imsg);
            }
        }
    }

    /* Draw initial content */
    D(bug("[CompositorTest] Drawing initial content...\n"));

    /*
     * Force GL context setup by clearing the board first.
     * This ensures the FBO is created and bound before we draw.
     */
    D(bug("[CompositorTest] Initializing background board GL state...\n"));
    ZuneSetTarget(bg_rp, bg_board);
    ZuneClearDrawingBoard(bg_rp, ZUNE_BLACK);
    ZuneSync(bg_rp);
    D(bug("[CompositorTest] Background board initialized.\n"));

    D(bug("[CompositorTest] Initializing alpha board GL state...\n"));
    ZuneSetTarget(alpha_rp, alpha_board);
    ZuneClearDrawingBoard(alpha_rp, ZUNE_COLOR_ARGB32(0, 0, 0, 0));
    ZuneSync(alpha_rp);
    D(bug("[CompositorTest] Alpha board initialized.\n"));

    /* Draw both windows */
    DrawBackgroundWindow();
    DrawAlphaWindow();

    /* Composite the alpha window over the background */
    CompositeAlphaWindow();

    /* Calculate signal mask for both windows */
    win_mask = (1L << bg_window->UserPort->mp_SigBit);
    if (alpha_window && alpha_window->UserPort)
        win_mask |= (1L << alpha_window->UserPort->mp_SigBit);

    /* Event loop */
    while (!done) {
        signals = Wait(win_mask | SIGBREAKF_CTRL_C);

        if (signals & SIGBREAKF_CTRL_C) {
            done = TRUE;
            break;
        }

        /* Handle background window messages */
        while (bg_window && (msg = (struct IntuiMessage *)GetMsg(bg_window->UserPort))) {
            switch (msg->Class) {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;

                case IDCMP_VANILLAKEY:
                    switch (msg->Code) {
                        case '+':
                        case '=':
                            if (current_alpha < 245)
                                UpdateAlphaValue(current_alpha + 10);
                            break;
                        case '-':
                            if (current_alpha > 10)
                                UpdateAlphaValue(current_alpha - 10);
                            break;
                        case 'r':
                        case 'R':
                            DrawBackgroundWindow();
                            DrawAlphaWindow();
                            if (compositor)
                                ZuneCompositorRefresh(compositor);
                            break;
                    }
                    break;

                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(bg_window);
                    DrawBackgroundWindow();
                    EndRefresh(bg_window, TRUE);
                    break;

                case IDCMP_CHANGEWINDOW:
                    /* Background window moved/resized - re-composite.
                     * Reply to the message first to avoid holding up Intuition. */
                    ReplyMsg((struct Message *)msg);
                    CompositeAlphaWindow();
                    continue;  /* Skip the ReplyMsg at the end */
            }
            ReplyMsg((struct Message *)msg);
        }

        /* Handle alpha window messages */
        while (alpha_window && (msg = (struct IntuiMessage *)GetMsg(alpha_window->UserPort))) {
            switch (msg->Class) {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;

                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(alpha_window);
                    DrawAlphaWindow();
                    EndRefresh(alpha_window, TRUE);
                    break;

                case IDCMP_CHANGEWINDOW:
                    /* Alpha window moved/resized - re-composite.
                     * Reply to the message first to avoid holding up Intuition. */
                    ReplyMsg((struct Message *)msg);
                    CompositeAlphaWindow();
                    continue;  /* Skip the ReplyMsg at the end */
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    CleanupDemo();
    D(bug("[CompositorTest] Demo finished.\n"));
    return 0;
}

BOOL InitDemo(void) {
    WORD inner_width, inner_height;

    /* Open required libraries */
    ZuneGfxBase = OpenLibrary("zunegfx.library", 1);
    if (!ZuneGfxBase) {
        D(bug("[CompositorTest] ERROR: Cannot open zunegfx.library\n"));
        return FALSE;
    }

    CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    if (!CyberGfxBase) {
        D(bug("[CompositorTest] WARNING: Cannot open cybergraphics.library\n"));
    }

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    if (!IntuitionBase) {
        D(bug("[CompositorTest] ERROR: Cannot open intuition.library\n"));
        return FALSE;
    }

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!GfxBase) {
        D(bug("[CompositorTest] ERROR: Cannot open graphics.library\n"));
        return FALSE;
    }

    LayersBase = OpenLibrary("layers.library", 39);
    if (!LayersBase) {
        D(bug("[CompositorTest] ERROR: Cannot open layers.library\n"));
        return FALSE;
    }

    /* Lock the Workbench screen */
    screen = LockPubScreen(NULL);
    if (!screen) {
        D(bug("[CompositorTest] ERROR: Cannot lock Workbench screen\n"));
        return FALSE;
    }

    D(bug("[CompositorTest] Screen: %p (%dx%d)\n", screen, screen->Width, screen->Height));

    /*
     * Open BACKGROUND window (normal, opaque)
     * This window renders through the standard layer system.
     */
    D(bug("[CompositorTest] Opening background window (opaque)...\n"));
    bg_window = OpenWindowTags(
        NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 50,
        WA_Top, 50,
        WA_Width, BG_WIDTH,
        WA_Height, BG_HEIGHT,
        WA_Title, (IPTR)"Background Window (Opaque)",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_SimpleRefresh, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_REFRESHWINDOW | IDCMP_CHANGEWINDOW,
        TAG_DONE);

    if (!bg_window) {
        D(bug("[CompositorTest] ERROR: Cannot open background window\n"));
        return FALSE;
    }
    D(bug("[CompositorTest] Background window: %p\n", bg_window));

    /* Create RenderContext for background window */
    D(bug("[CompositorTest] Creating RenderContext for background window (BACKEND_OPENGL=%d)...\n", BACKEND_OPENGL));
    bg_rp = ZuneCreateRenderContextForWindow(bg_window, screen->ViewPort.ColorMap, BACKEND_OPENGL);
    if (!bg_rp) {
        D(bug("[CompositorTest] ERROR: Cannot create background RenderContext\n"));
        return FALSE;
    }
    D(bug("[CompositorTest] Background RenderContext: %p, backend_type=%d\n", bg_rp, bg_rp->backend_type));

    /*
     * WORKAROUND: Prime the GL context by doing a dummy window-based render.
     * Mesa's internal blitting pipeline needs to be initialized via glASwapBuffers
     * before FBO operations work correctly. Without this, the first FBO render
     * may not display properly.
     *
     * This clears directly to the window (not DrawingBoard) which triggers
     * glASwapBuffers and initializes Mesa's BltPipeResourceRastPort.
     */
    D(bug("[CompositorTest] Priming GL context with window render...\n"));
    ZuneSetTarget(bg_rp, NULL);  /* Target window directly */
    ZuneClearRenderContext(bg_rp, ZUNE_BLACK);
    D(bug("[CompositorTest] GL context primed.\n"));

    inner_width = bg_window->Width - bg_window->BorderLeft - bg_window->BorderRight;
    inner_height = bg_window->Height - bg_window->BorderTop - bg_window->BorderBottom;

    bg_board = ZuneCreateDrawingBoardForRenderContext(bg_rp, inner_width, inner_height, 0);
    if (!bg_board) {
        D(bug("[CompositorTest] ERROR: Cannot create background DrawingBoard\n"));
        return FALSE;
    }

    /*
     * Open ALPHA window (transparent)
     * This window will be composited by our LayerCompositor.
     * The WA_Alpha tag tells Intuition/Layers that this is a transparent window.
     */
    D(bug("[CompositorTest] Opening alpha window (transparent, alpha=%d)...\n", current_alpha));
    alpha_window = OpenWindowTags(
        NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 150,
        WA_Top, 150,
        WA_Width, ALPHA_WIDTH,
        WA_Height, ALPHA_HEIGHT,
        WA_Title, (IPTR)"Alpha Window",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, FALSE,
        WA_SimpleRefresh, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_CHANGEWINDOW,
        /* Alpha window tags */
        WA_Alpha, TRUE,
        WA_AlphaValue, current_alpha,
        TAG_DONE);

    if (!alpha_window) {
        D(bug("[CompositorTest] WARNING: Cannot open alpha window - trying without WA_Alpha\n"));
        /* Try without alpha tags */
        alpha_window = OpenWindowTags(
            NULL,
            WA_CustomScreen, (IPTR)screen,
            WA_Left, 150,
            WA_Top, 150,
            WA_Width, ALPHA_WIDTH,
            WA_Height, ALPHA_HEIGHT,
            WA_Title, (IPTR)"Alpha Window (fallback)",
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_DepthGadget, TRUE,
            WA_SimpleRefresh, TRUE,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_CHANGEWINDOW,
            TAG_DONE);
    }

    if (!alpha_window) {
        D(bug("[CompositorTest] ERROR: Cannot open alpha window at all\n"));
        return FALSE;
    }
    D(bug("[CompositorTest] Alpha window: %p, Layer: %p\n", alpha_window, alpha_window->WLayer));

    /* Test: Use OpenGL for both windows to see if shared contexts work */
    alpha_rp = ZuneCreateRenderContextForWindow(alpha_window, screen->ViewPort.ColorMap, BACKEND_OPENGL);
    if (!alpha_rp) {
        D(bug("[CompositorTest] ERROR: Cannot create alpha RenderContext\n"));
        return FALSE;
    }
    D(bug("[CompositorTest] Alpha RenderContext: %p, backend_type=%d\n", alpha_rp, alpha_rp->backend_type));

    inner_width = alpha_window->Width - alpha_window->BorderLeft - alpha_window->BorderRight;
    inner_height = alpha_window->Height - alpha_window->BorderTop - alpha_window->BorderBottom;

    alpha_board = ZuneCreateDrawingBoardForRenderContext(alpha_rp, inner_width, inner_height, ZUNE_DRAWINGBOARD_ALPHA);
    if (!alpha_board) {
        D(bug("[CompositorTest] ERROR: Cannot create alpha DrawingBoard\n"));
        return FALSE;
    }

    /*
     * Create the Layer Compositor for this screen AFTER windows are created.
     * This enables hardware-accelerated compositing for alpha windows.
     *
     * We use ZuneCreateLayerCompositorShared() with the zunegfx master GL context
     * to ensure the compositor shares the same pipe_screen as zunegfx windows.
     * This is critical for first-run scenarios where Mesa hasn't cached the
     * pipe_screen yet.
     */
    D(bug("[CompositorTest] Creating Layer Compositor...\n"));
    {
        APTR masterGLContext = ZuneGetMasterGLContext();
        D(bug("[CompositorTest] Got zunegfx master GL context: %p\n", masterGLContext));
        compositor = ZuneCreateLayerCompositorShared(screen, masterGLContext);
    }
    if (!compositor) {
        D(bug("[CompositorTest] WARNING: Cannot create compositor - alpha windows may not work correctly\n"));
    } else {
        D(bug("[CompositorTest] Compositor created: %p\n", compositor));

        /* Activate the compositor */
        if (ZuneActivateLayerCompositor(compositor)) {
            D(bug("[CompositorTest] Compositor activated!\n"));
        } else {
            D(bug("[CompositorTest] WARNING: Failed to activate compositor\n"));
        }

        /*
         * Register the alpha window with the compositor.
         * This tells the compositor to handle this window specially.
         */
        D(bug("[CompositorTest] Registering alpha window with compositor...\n"));

        /* Get the GL context from the RenderContext if available */
        APTR gl_context = NULL;  /* TODO: Get from alpha_rp->backend_data */

        struct CompositorWindow *cw = ZuneCompositorRegisterWindow(
            compositor,
            alpha_window,
            gl_context,
            alpha_board,
            current_alpha);

        if (cw) {
            D(bug("[CompositorTest] Alpha window registered with compositor!\n"));
        } else {
            D(bug("[CompositorTest] WARNING: Failed to register alpha window\n"));
        }
    }

    D(bug("[CompositorTest] Initialization complete!\n"));
    return TRUE;
}

void CleanupDemo(void) {
    if (compositor) {
        if (alpha_window)
            ZuneCompositorUnregisterWindow(compositor, alpha_window);
        ZuneDeactivateLayerCompositor(compositor);
        ZuneDestroyLayerCompositor(compositor);
        compositor = NULL;
    }

    if (alpha_board && alpha_rp) {
        ZuneDestroyDrawingBoard(alpha_rp, alpha_board);
        alpha_board = NULL;
    }

    if (alpha_rp) {
        ZuneDestroyRenderContext(alpha_rp);
        alpha_rp = NULL;
    }

    if (bg_board && bg_rp) {
        ZuneDestroyDrawingBoard(bg_rp, bg_board);
        bg_board = NULL;
    }

    if (bg_rp) {
        ZuneDestroyRenderContext(bg_rp);
        bg_rp = NULL;
    }

    if (alpha_window) {
        CloseWindow(alpha_window);
        alpha_window = NULL;
    }

    if (bg_window) {
        CloseWindow(bg_window);
        bg_window = NULL;
    }

    if (screen) {
        UnlockPubScreen(NULL, screen);
        screen = NULL;
    }

    if (LayersBase) {
        CloseLibrary(LayersBase);
        LayersBase = NULL;
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

/*
 * Draw content to the background (opaque) window
 */
void DrawBackgroundWindow(void) {
    WORD dst_x, dst_y;

    if (!bg_rp || !bg_board || !bg_window)
        return;

    D(bug("[CompositorTest] Drawing background window...\n"));

    ZuneSetTarget(bg_rp, bg_board);

    /* Clear to a nice gradient-like pattern */
    ZuneClearDrawingBoard(bg_rp, ZUNE_COLOR_RGB24(40, 80, 120));

    /* Draw a grid pattern to make transparency visible */
    for (int y = 0; y < bg_board->height; y += 40) {
        for (int x = 0; x < bg_board->width; x += 40) {
            if ((x + y) % 80 == 0) {
                ZuneFillRectangleRoundedAAXYWH(bg_rp, x + 2, y + 2, 36, 36, 5,
                    ZUNE_BRUSH_SOLID(ZUNE_COLOR_RGB24(60, 100, 140)));
            } else {
                ZuneFillRectangleRoundedAAXYWH(bg_rp, x + 2, y + 2, 36, 36, 5,
                    ZUNE_BRUSH_SOLID(ZUNE_COLOR_RGB24(80, 120, 160)));
            }
        }
    }

    /* Draw some distinctive shapes */
    ZuneFillCircleAAAt(bg_rp, 100, 100, 60, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillCircleAAAt(bg_rp, 250, 150, 80, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillCircleAAAt(bg_rp, 400, 200, 70, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    /* Label */
    ZuneFillRectangleRoundedAAXYWH(bg_rp, 10, bg_board->height - 50, 200, 40, 8,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 0, 0, 0)));

    /* Present to window */
    dst_x = bg_window->BorderLeft;
    dst_y = bg_window->BorderTop;
    ZunePresent(bg_rp, 0, 0, dst_x, dst_y, bg_board->width, bg_board->height);
}

/*
 * Draw content to the alpha (transparent) window
 */
void DrawAlphaWindow(void) {
    WORD dst_x, dst_y;
    static char title[64];  /* Must be static - SetWindowTitles keeps pointer */

    if (!alpha_rp || !alpha_board || !alpha_window)
        return;

    D(bug("[CompositorTest] Drawing alpha window (alpha=%d)...\n", current_alpha));

    ZuneSetTarget(alpha_rp, alpha_board);

    /* Clear to semi-transparent background */
    ZuneClearDrawingBoard(alpha_rp, ZUNE_COLOR_ARGB32(current_alpha, 255, 200, 100));

    /* Draw some shapes with varying alpha */
    ZuneFillRectangleRoundedAAXYWH(alpha_rp, 20, 20, 100, 80, 15,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 0, 0)));  /* Red */

    ZuneFillRectangleRoundedAAXYWH(alpha_rp, 140, 20, 100, 80, 15,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(150, 0, 255, 0)));  /* Green, more transparent */

    ZuneFillCircleAAAt(alpha_rp, 75, 140, 40,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(180, 0, 0, 255)));  /* Blue */

    ZuneFillCircleAAAt(alpha_rp, 190, 140, 40,
        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(100, 255, 255, 0)));  /* Yellow, very transparent */

    /* Present to window */
    dst_x = alpha_window->BorderLeft;
    dst_y = alpha_window->BorderTop;
    ZunePresent(alpha_rp, 0, 0, dst_x, dst_y, alpha_board->width, alpha_board->height);

    /* Update window title with current alpha */
    sprintf(title, "Alpha Window (alpha=%d)", current_alpha);
    SetWindowTitles(alpha_window, title, (STRPTR)~0);

    /* Mark dirty for compositor */
    if (compositor)
        ZuneCompositorMarkWindowDirty(compositor, alpha_window);
}

/*
 * Update the alpha value for the alpha window
 */
void UpdateAlphaValue(UBYTE new_alpha) {
    current_alpha = new_alpha;
    D(bug("[CompositorTest] Alpha changed to %d\n", current_alpha));

    /* Update compositor registration */
    if (compositor) {
        ZuneCompositorSetWindowAlpha(compositor, alpha_window, current_alpha);
    }

    /* Redraw the alpha window and composite */
    DrawAlphaWindow();
    CompositeAlphaWindow();

    /* Trigger compositor update */
    if (compositor) {
        ZuneCompositorUpdate(compositor);
    }
}

/*
 * Manual alpha compositing - blend the alpha window onto the screen
 * where it overlaps the background window.
 *
 * This reads the background pixels, blends with alpha window content,
 * and writes the result to the alpha window's screen area.
 */
void CompositeAlphaWindow(void) {
    WORD alpha_left, alpha_top, alpha_right, alpha_bottom;
    WORD bg_left, bg_top, bg_right, bg_bottom;
    WORD overlap_left, overlap_top, overlap_right, overlap_bottom;
    WORD overlap_width, overlap_height;
    WORD inner_left, inner_top;
    UBYTE *bg_pixels = NULL;
    UBYTE *alpha_pixels = NULL;
    UBYTE *result_pixels = NULL;
    ULONG x, y, i;

    if (!alpha_window || !bg_window || !CyberGfxBase || !alpha_board || !bg_board)
        return;

    D(bug("[CompositorTest] Compositing alpha window...\n"));

    /* Get alpha window screen coordinates (inner area only) */
    inner_left = alpha_window->BorderLeft;
    inner_top = alpha_window->BorderTop;
    alpha_left = alpha_window->LeftEdge + inner_left;
    alpha_top = alpha_window->TopEdge + inner_top;
    alpha_right = alpha_left + alpha_board->width;
    alpha_bottom = alpha_top + alpha_board->height;

    /* Get background window screen coordinates (inner area only) */
    bg_left = bg_window->LeftEdge + bg_window->BorderLeft;
    bg_top = bg_window->TopEdge + bg_window->BorderTop;
    bg_right = bg_left + bg_board->width;
    bg_bottom = bg_top + bg_board->height;

    /* Calculate overlap region */
    overlap_left = (alpha_left > bg_left) ? alpha_left : bg_left;
    overlap_top = (alpha_top > bg_top) ? alpha_top : bg_top;
    overlap_right = (alpha_right < bg_right) ? alpha_right : bg_right;
    overlap_bottom = (alpha_bottom < bg_bottom) ? alpha_bottom : bg_bottom;

    overlap_width = overlap_right - overlap_left;
    overlap_height = overlap_bottom - overlap_top;

    if (overlap_width <= 0 || overlap_height <= 0) {
        D(bug("[CompositorTest] No overlap between windows\n"));
        return;
    }

    D(bug("[CompositorTest] Overlap region: %dx%d at %d,%d\n",
          overlap_width, overlap_height, overlap_left, overlap_top));

    /* Allocate buffers */
    bg_pixels = AllocVec(overlap_width * overlap_height * 4, MEMF_ANY);
    alpha_pixels = AllocVec(overlap_width * overlap_height * 4, MEMF_ANY);
    result_pixels = AllocVec(overlap_width * overlap_height * 4, MEMF_ANY);

    if (!bg_pixels || !alpha_pixels || !result_pixels) {
        D(bug("[CompositorTest] Failed to allocate composite buffers\n"));
        goto cleanup;
    }

    /* Read background pixels from the background DrawingBoard (not the screen!) */
    {
        /* bg_left/bg_top are now inner area screen coords, so this is simple */
        WORD bg_src_x = overlap_left - bg_left;
        WORD bg_src_y = overlap_top - bg_top;

        D(bug("[CompositorTest] Reading bg from board at %d,%d size %dx%d\n",
              bg_src_x, bg_src_y, overlap_width, overlap_height));

        /* Bounds check */
        if (!bg_board || !bg_board->rastport ||
            bg_src_x < 0 || bg_src_y < 0 ||
            bg_src_x + overlap_width > bg_board->width ||
            bg_src_y + overlap_height > bg_board->height) {
            D(bug("[CompositorTest] Cannot read from bg_board - out of bounds!\n"));
            goto cleanup;
        }

        ReadPixelArray(bg_pixels, 0, 0, overlap_width * 4,
                       bg_board->rastport, bg_src_x, bg_src_y,
                       overlap_width, overlap_height, RECTFMT_ARGB);
    }

    /* Read alpha window pixels from its DrawingBoard */
    {
        WORD alpha_src_x = overlap_left - alpha_left;
        WORD alpha_src_y = overlap_top - alpha_top;

        D(bug("[CompositorTest] Reading alpha from board at %d,%d size %dx%d\n",
              alpha_src_x, alpha_src_y, overlap_width, overlap_height));

        /* Bounds check */
        if (!alpha_board->rastport ||
            alpha_src_x < 0 || alpha_src_y < 0 ||
            alpha_src_x + overlap_width > alpha_board->width ||
            alpha_src_y + overlap_height > alpha_board->height) {
            D(bug("[CompositorTest] Cannot read from alpha_board - out of bounds!\n"));
            goto cleanup;
        }

        ReadPixelArray(alpha_pixels, 0, 0, overlap_width * 4,
                       alpha_board->rastport, alpha_src_x, alpha_src_y,
                       overlap_width, overlap_height, RECTFMT_ARGB);
    }

    /* Alpha blend: result = window_alpha * fg + (1 - window_alpha) * bg
     * Use window-level alpha (current_alpha) for the entire window,
     * not per-pixel alpha - this is more efficient and matches
     * traditional window transparency behavior. */
    UWORD alpha = current_alpha;
    UWORD inv_alpha = 255 - alpha;

    for (y = 0; y < overlap_height; y++) {
        for (x = 0; x < overlap_width; x++) {
            i = (y * overlap_width + x) * 4;

            /* ARGB format: A is first byte, then R, G, B */
            UBYTE fg_r = alpha_pixels[i + 1];
            UBYTE fg_g = alpha_pixels[i + 2];
            UBYTE fg_b = alpha_pixels[i + 3];

            UBYTE bg_r = bg_pixels[i + 1];
            UBYTE bg_g = bg_pixels[i + 2];
            UBYTE bg_b = bg_pixels[i + 3];

            /* Blend using window-level alpha */
            result_pixels[i + 0] = 255;  /* Result is opaque */
            result_pixels[i + 1] = (fg_r * alpha + bg_r * inv_alpha) / 255;
            result_pixels[i + 2] = (fg_g * alpha + bg_g * inv_alpha) / 255;
            result_pixels[i + 3] = (fg_b * alpha + bg_b * inv_alpha) / 255;
        }
    }

    /* Write blended result to alpha window's inner area.
     * The overlap coordinates are in screen space, so we convert to
     * window-relative coordinates for the RastPort. */
    {
        WORD dst_x = overlap_left - alpha_window->LeftEdge;
        WORD dst_y = overlap_top - alpha_window->TopEdge;
        WORD src_x = 0, src_y = 0;
        WORD write_width = overlap_width;
        WORD write_height = overlap_height;

        /* Clamp to inner area to avoid corrupting window borders/title */
        if (dst_x < alpha_window->BorderLeft) {
            WORD diff = alpha_window->BorderLeft - dst_x;
            src_x += diff;
            dst_x = alpha_window->BorderLeft;
            write_width -= diff;
        }
        if (dst_y < alpha_window->BorderTop) {
            WORD diff = alpha_window->BorderTop - dst_y;
            src_y += diff;
            dst_y = alpha_window->BorderTop;
            write_height -= diff;
        }

        if (write_width > 0 && write_height > 0) {
            WritePixelArray(result_pixels, src_x, src_y, overlap_width * 4,
                            alpha_window->RPort, dst_x, dst_y,
                            write_width, write_height, RECTFMT_ARGB);
        }
    }

    D(bug("[CompositorTest] Compositing complete\n"));

cleanup:
    if (bg_pixels) FreeVec(bg_pixels);
    if (alpha_pixels) FreeVec(alpha_pixels);
    if (result_pixels) FreeVec(result_pixels);
}
