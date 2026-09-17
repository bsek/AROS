/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - AA Circle Demo (Unlocked DrawingBoard)

    Tests antialiased circle functions with standard rastport operations:
    - ZuneFillCircleAA
    - ZuneFillCircleStyledAA (fill + border)
    - ZuneDrawCircleOutlineStyledAA

    Usage: demo_circle_aa_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawAACircles(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* AA filled circles */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 90), 40,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_RED),
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 90), 40,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GREEN),
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 90), 40,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_BLUE),
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* AA outlined circles with different widths */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 200), 40,
                   ZUNE_STYLE(.border_color = ZUNE_YELLOW,
                              .border_width = 1,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 200), 40,
                   ZUNE_STYLE(.border_color = ZUNE_MAGENTA,
                              .border_width = 3,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 200), 40,
                   ZUNE_STYLE(.border_color = ZUNE_CYAN,
                              .border_width = 5,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* AA filled + border (styled) */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 90), 30,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_WHITE),
                              .border_color = ZUNE_LIGHTGRAY,
                              .border_width = 1,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 150), 25,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_YELLOW),
                              .border_color = ZUNE_GRAY,
                              .border_width = 3,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 210), 20,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY),
                              .border_color = ZUNE_RED,
                              .border_width = 2,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Various line widths */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 320), 35,
                   ZUNE_STYLE(.border_color = ZUNE_RED,
                              .border_width = 1,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 320), 30,
                   ZUNE_STYLE(.border_color = ZUNE_GREEN,
                              .border_width = 4,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 320), 25,
                   ZUNE_STYLE(.border_color = ZUNE_BLUE,
                              .border_width = 6,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Overlapping AA */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 300), 45,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)),
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(500, 320), 40,
                   ZUNE_STYLE(.border_color = ZUNE_COLOR_ARGB32(255, 255, 0, 0),
                              .border_width = 4,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Small AA circles */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(520, 370), 15,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)),
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(550, 390), 12,
                   ZUNE_STYLE(.border_color = ZUNE_COLOR_ARGB32(200, 64, 255, 128),
                              .border_width = 2,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(580, 410), 18,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
                              .border_color = ZUNE_COLOR_ARGB32(255, 255, 255, 255),
                              .border_width = 3,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("AA Circle Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("==========================================\n\n");

    if (!DemoInit(&ctx, "AA Circle Demo (Unlocked)", 640, 480, backend, 0))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawAACircles(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
