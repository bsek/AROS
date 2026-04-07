/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Circle Demo (Locked DrawingBoard)

    Tests non-AA circle functions with locked pixel access:
    - ZuneDrawCircle (solid brush)
    - ZuneDrawCircleOutline
    - ZuneDrawCircleOutlineStyled

    Usage: demo_circle_locked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawCircles(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Filled circles */
    ZuneDrawCircleAt(rctx, 120, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneDrawCircleAt(rctx, 240, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneDrawCircleAt(rctx, 360, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    /* Outlined circles */
    ZuneDrawCircleOutlineAt(rctx, 120, 200, 40, ZUNE_YELLOW);
    ZuneDrawCircleOutlineAt(rctx, 240, 200, 40, ZUNE_MAGENTA);
    ZuneDrawCircleOutlineStyledAt(rctx, 360, 200, 40, 5, ZUNE_CYAN);

    /* Various sizes */
    ZuneDrawCircleAt(rctx, 480, 90, 30, ZUNE_BRUSH_SOLID(ZUNE_WHITE));
    ZuneDrawCircleAt(rctx, 480, 150, 25, ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
    ZuneDrawCircleAt(rctx, 480, 200, 20, ZUNE_BRUSH_SOLID(ZUNE_GRAY));

    /* Outlined with different styles */
    ZuneDrawCircleOutlineAt(rctx, 120, 320, 35, ZUNE_RED);
    ZuneDrawCircleOutlineStyledAt(rctx, 240, 320, 30, 3, ZUNE_GREEN);
    ZuneDrawCircleOutlineStyledAt(rctx, 360, 320, 25, 2, ZUNE_BLUE);

    /* Overlapping semi-transparent */
    ZuneDrawCircleAt(rctx, 480, 280, 45,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
    ZuneDrawCircleOutlineStyledAt(rctx, 500, 300, 40, 4,
                                  ZUNE_COLOR_ARGB32(128, 0, 255, 255));

    /* Small circles */
    ZuneDrawCircleAt(rctx, 520, 350, 15,
                     ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
    ZuneDrawCircleOutlineAt(rctx, 550, 370, 12,
                            ZUNE_COLOR_ARGB32(200, 64, 255, 128));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);
    APTR pixels;
    ULONG pitch;

    printf("Circle Demo (Locked) - %s\n", DemoBackendName(backend));
    printf("======================================\n\n");

    if (!DemoInit(&ctx, "Circle Demo (Locked)", 640, 480, backend, ZUNE_DRAWINGBOARD_LINEARMEM))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);

    pixels = ZuneLockDrawingBoardPixels(ctx.rctx, &pitch);
    if (!pixels) {
        printf("ERROR: Cannot lock DrawingBoard pixels\n");
        DemoCleanup(&ctx);
        return 1;
    }

    DrawCircles(&ctx);

    ZuneUnlockDrawingBoardPixels(ctx.rctx);

    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
