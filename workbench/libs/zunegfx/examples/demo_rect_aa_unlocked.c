/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - AA Rectangle Demo (Unlocked DrawingBoard)

    Tests antialiased rectangle functions with standard rastport operations:
    - ZuneFillRectangleRoundedAA
    - ZuneFillRectangleRoundedStyledAA (fill + border)
    - ZuneDrawRectangleRoundedOutlineAA
    - ZuneDrawRectangleRoundedOutlineStyledAA

    Usage: demo_rect_aa_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawAARectangles(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Filled rounded AA with different radii */
    ZuneFillRectangleRoundedAAXYWH(rctx, 50, 50, 100, 80, 5, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillRectangleRoundedAAXYWH(rctx, 170, 50, 100, 80, 20, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillRectangleRoundedAAXYWH(rctx, 290, 50, 100, 80, 25, ZUNE_BRUSH_SOLID(ZUNE_BLACK));

    /* Outlined rounded AA */
    ZuneDrawRectangleRoundedOutlineAAXYWH(rctx, 50, 150, 100, 80, 5, ZUNE_LIGHTGRAY);
    ZuneDrawRectangleRoundedOutlineAAXYWH(rctx, 170, 150, 100, 80, 15, ZUNE_MAGENTA);
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, 290, 150, 100, 80, 20, 5, ZUNE_CYAN);

    /* Filled + border (styled AA) */
    ZuneFillRectangleRoundedStyledAAXYWH(rctx, 50, 250, 100, 80, 15, 1,
                                         ZUNE_BRUSH_SOLID(ZUNE_WHITE), ZUNE_LIGHTGRAY);
    ZuneFillRectangleRoundedStyledAAXYWH(rctx, 170, 250, 100, 80, 20, 4,
                                         ZUNE_BRUSH_SOLID(ZUNE_YELLOW), ZUNE_LIGHTGRAY);
    ZuneFillRectangleRoundedStyledAAXYWH(rctx, 290, 250, 100, 80, 25, 1,
                                         ZUNE_BRUSH_SOLID(ZUNE_GRAY), ZUNE_LIGHTGRAY);

    /* Outlined styled AA with different line widths */
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, 50, 350, 100, 80, 10, 1, ZUNE_RED);
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, 170, 350, 100, 80, 15, 3, ZUNE_GREEN);
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, 290, 350, 100, 80, 20, 4, ZUNE_BLUE);

    /* Overlapping AA */
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, 450, 130, 150, 100, 35, 5,
                                                ZUNE_COLOR_ARGB32(255, 255, 0, 0));

    /* Small AA rectangles */
    ZuneDrawRectangleRoundedOutlineStyledAAXYWH(rctx, 470, 290, 35, 25, 6, 2,
                                                ZUNE_COLOR_ARGB32(200, 64, 255, 128));
    ZuneFillRectangleRoundedStyledAAXYWH(rctx, 515, 300, 45, 35, 10, 3,
                                         ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
                                         ZUNE_COLOR_ARGB32(255, 255, 255, 255));

    /* Gradient AA */
    struct ZuneGradientStop gradient_stops[] = {
        {0.0f, ZUNE_COLOR_ARGB32(255, 0, 128, 255)},
        {0.5f, ZUNE_COLOR_ARGB32(255, 0, 255, 128)},
        {1.0f, ZUNE_COLOR_ARGB32(255, 128, 255, 0)},
    };
    struct ZuneBrush gradient_brush = {
        .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
        .flags = 0, .internal = {0},
        .data = {.linear = {
            .start = ZUNE_POINT_LITERAL(0, 0),
            .end = ZUNE_POINT_LITERAL(180, 0),
            .stops = gradient_stops,
            .stop_count = 3
        }}
    };
    ZuneFillRectangleRoundedStyledAAXYWH(rctx, 420, 350, 180, 80, 18, 5,
                                         &gradient_brush, ZUNE_RED);
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("AA Rectangle Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("============================================\n\n");

    if (!DemoInit(&ctx, "AA Rectangle Demo (Unlocked)", 640, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawAARectangles(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
