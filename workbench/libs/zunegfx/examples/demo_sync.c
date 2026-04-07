/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Sync/Interop Demo

    Tests OpenGL/CyberGfx synchronization and blitting:
    - ZuneSync (FBO -> bitmap)
    - ZuneReload (bitmap -> FBO)
    - ZunePresent
    - ZuneBlit
    - ZuneCapture

    Best tested with OpenGL backend: demo_sync opengl

    Usage: demo_sync [cybergfx|opengl]
*/

#include "demo_common.h"

static void RunSyncDemo(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    ZuneSetFont(rctx, ctx->window->RPort->Font);

    /* Test 1: Pure ZuneGfx rendering + Present */
    ZuneDrawTextAt(rctx, 20, 10, "Test 1: ZuneGfx only", 20, ZUNE_WHITE);
    ZuneClearDrawingBoard(rctx, ZUNE_COLOR_RGB24(30, 30, 40));

    ZuneFillRectangleRoundedAAXYWH(rctx, 50, 30, 100, 80, 15, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillRectangleRoundedAAXYWH(rctx, 170, 30, 100, 80, 15, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillRectangleRoundedAAXYWH(rctx, 290, 30, 100, 80, 15, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    DemoPresent(ctx);
    printf("  Test 1 done (pure ZuneGfx). Press ENTER...\n");
    getchar();

    /* Test 2: ZuneSync - copy FBO to bitmap, then CyberGfx draws */
    ZuneDrawTextAt(rctx, 20, 10, "Test 2: ZuneSync + CyberGfx", 27, ZUNE_WHITE);
    ZuneClearDrawingBoard(rctx, ZUNE_COLOR_RGB24(30, 30, 40));
    ZuneFillRectangleRoundedAAXYWH(rctx, 50, 30, 100, 80, 15, ZUNE_BRUSH_SOLID(ZUNE_RED));

    printf("  Calling ZuneSync...\n");
    ZuneSync(rctx);

    /* Direct CyberGfx access after sync */
    if (ctx->board->rastport && ctx->CyberGfxBase) {
        FillPixelArray(ctx->board->rastport, 200, 40, 100, 60, 0xFFFFFF00);
        printf("  CyberGfx FillPixelArray done\n");
    }

    /* Reload bitmap back to FBO */
    printf("  Calling ZuneReload...\n");
    ZuneReload(rctx);

    /* More ZuneGfx drawing on top */
    ZuneFillRectangleRoundedAAXYWH(rctx, 350, 30, 100, 80, 15, ZUNE_BRUSH_SOLID(ZUNE_CYAN));

    DemoPresent(ctx);
    printf("  Test 2 done (Sync+CyberGfx+Reload). Press ENTER...\n");
    getchar();

    /* Test 3: ZuneBlit between DrawingBoards */
    ZuneDrawTextAt(rctx, 20, 10, "Test 3: ZuneBlit", 16, ZUNE_WHITE);
    ZuneClearDrawingBoard(rctx, ZUNE_COLOR_RGB24(30, 30, 40));

    /* Create a second DrawingBoard */
    struct DrawingBoard *src_board = ZuneCreateDrawingBoardForRenderContext(rctx, 200, 150, 0);
    if (src_board) {
        /* Draw content on source board */
        ZuneSetTarget(rctx, src_board);
        ZuneClearDrawingBoard(rctx, ZUNE_COLOR_ARGB32(255, 60, 60, 100));
        ZuneFillRectangleRoundedAAXYWH(rctx, 20, 20, 160, 110, 20, ZUNE_BRUSH_SOLID(ZUNE_YELLOW));
        ZuneDrawTextAt(rctx, 50, 65, "Source", 6, ZUNE_BLACK);

        /* Switch back to main board */
        ZuneSetTarget(rctx, ctx->board);

        /* Blit source to main at different positions */
        ZuneBlit(rctx, rctx, 0, 0, 50, 50, 200, 150);

        ZuneDrawTextAt(rctx, 50, 210, "Blitted from source board", 25, ZUNE_LIGHTGRAY);

        ZuneDestroyDrawingBoard(rctx, src_board);
    }

    DemoPresent(ctx);
    printf("  Test 3 done (Blit). Press ENTER...\n");
    getchar();

    /* Test 4: ZuneCapture */
    ZuneDrawTextAt(rctx, 20, 10, "Test 4: ZuneCapture from window", 31, ZUNE_WHITE);
    ZuneClearDrawingBoard(rctx, ZUNE_COLOR_RGB24(30, 30, 40));

    /* Capture a portion of the window's RastPort into the DrawingBoard */
    ZuneCapture(rctx, ctx->window->RPort, 0, 0, 50, 50, 200, 100);
    ZuneDrawTextAt(rctx, 50, 160, "Captured from window RastPort", 29, ZUNE_LIGHTGRAY);

    DemoPresent(ctx);
    printf("  Test 4 done (Capture). Press ENTER to exit...\n");
    getchar();
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Sync/Interop Demo - %s\n", DemoBackendName(backend));
    printf("=====================================\n\n");

    if (!DemoInit(&ctx, "Sync/Interop Demo", 500, 300, backend, 0))
        return 1;

    RunSyncDemo(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
