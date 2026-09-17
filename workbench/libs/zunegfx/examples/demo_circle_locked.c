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
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 90), 40,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_RED)));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 90), 40,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GREEN)));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 90), 40,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_BLUE)));

    /* Outlined circles */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 200), 40,
                   ZUNE_STYLE(.border_color = ZUNE_YELLOW, .border_width = 1));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 200), 40,
                   ZUNE_STYLE(.border_color = ZUNE_MAGENTA, .border_width = 1));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 200), 40,
                   ZUNE_STYLE(.border_color = ZUNE_CYAN, .border_width = 5));

    /* Various sizes */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 90), 30,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_WHITE)));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 150), 25,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY)));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 200), 20,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GRAY)));

    /* Outlined with different styles */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 320), 35,
                   ZUNE_STYLE(.border_color = ZUNE_RED, .border_width = 1));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 320), 30,
                   ZUNE_STYLE(.border_color = ZUNE_GREEN, .border_width = 3));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 320), 25,
                   ZUNE_STYLE(.border_color = ZUNE_BLUE, .border_width = 2));

    /* Overlapping semi-transparent */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 280), 45,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0))));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(500, 300), 40,
                   ZUNE_STYLE(.border_color = ZUNE_COLOR_ARGB32(128, 0, 255, 255),
                              .border_width = 4));

    /* Small circles */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(520, 350), 15,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64))));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(550, 370), 12,
                   ZUNE_STYLE(.border_color = ZUNE_COLOR_ARGB32(200, 64, 255, 128),
                              .border_width = 1));
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
