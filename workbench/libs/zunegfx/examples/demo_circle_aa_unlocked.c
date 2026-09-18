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
                   ZUNE_STYLE(.stroke_color = ZUNE_YELLOW,
                              .stroke_width = 1,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 200), 40,
                   ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA,
                              .stroke_width = 3,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 200), 40,
                   ZUNE_STYLE(.stroke_color = ZUNE_CYAN,
                              .stroke_width = 5,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* AA filled + border (styled) */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 90), 30,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_WHITE),
                              .stroke_color = ZUNE_LIGHTGRAY,
                              .stroke_width = 1,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 150), 25,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_YELLOW),
                              .stroke_color = ZUNE_GRAY,
                              .stroke_width = 3,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 210), 20,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY),
                              .stroke_color = ZUNE_RED,
                              .stroke_width = 2,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Various line widths */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(120, 320), 35,
                   ZUNE_STYLE(.stroke_color = ZUNE_RED,
                              .stroke_width = 1,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(240, 320), 30,
                   ZUNE_STYLE(.stroke_color = ZUNE_GREEN,
                              .stroke_width = 4,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(360, 320), 25,
                   ZUNE_STYLE(.stroke_color = ZUNE_BLUE,
                              .stroke_width = 6,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Overlapping AA */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(480, 300), 45,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)),
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(500, 320), 40,
                   ZUNE_STYLE(.stroke_color = ZUNE_COLOR_ARGB32(255, 255, 0, 0),
                              .stroke_width = 4,
                              .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Small AA circles */
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(520, 370), 15,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)),
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(550, 390), 12,
                   ZUNE_STYLE(.stroke_color = ZUNE_COLOR_ARGB32(200, 64, 255, 128),
                              .stroke_width = 2,
                              .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawCircle(rctx, ZUNE_POINT_PTR(580, 410), 18,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
                              .stroke_color = ZUNE_COLOR_ARGB32(255, 255, 255, 255),
                              .stroke_width = 3,
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
