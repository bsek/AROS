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
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 50, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_RED),
                            .corner_radius = 5,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 50, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GREEN),
                            .corner_radius = 20,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 50, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_BLACK),
                            .corner_radius = 25,
                            .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Outlined rounded AA */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 150, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_LIGHTGRAY,
                            .stroke_width = 1,
                            .corner_radius = 5,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 150, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA,
                            .stroke_width = 1,
                            .corner_radius = 15,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 150, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_CYAN,
                            .stroke_width = 5,
                            .corner_radius = 20,
                            .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Filled + border (styled AA) */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 250, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_WHITE),
                            .stroke_color = ZUNE_LIGHTGRAY,
                            .stroke_width = 1,
                            .corner_radius = 15,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 250, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_YELLOW),
                            .stroke_color = ZUNE_LIGHTGRAY,
                            .stroke_width = 4,
                            .corner_radius = 20,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 250, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GRAY),
                            .stroke_color = ZUNE_LIGHTGRAY,
                            .stroke_width = 1,
                            .corner_radius = 25,
                            .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Outlined styled AA with different line widths */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 350, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_RED,
                            .stroke_width = 1,
                            .corner_radius = 10,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 350, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_GREEN,
                            .stroke_width = 3,
                            .corner_radius = 15,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 350, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_BLUE,
                            .stroke_width = 4,
                            .corner_radius = 20,
                            .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Overlapping AA */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(420, 100, 150, 100),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)),
                            .corner_radius = 30,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(450, 130, 150, 100),
                 ZUNE_STYLE(.stroke_color = ZUNE_COLOR_ARGB32(255, 255, 0, 0),
                            .stroke_width = 5,
                            .corner_radius = 35,
                            .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Small AA rectangles */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(420, 280, 40, 30),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)),
                            .corner_radius = 8,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(470, 290, 35, 25),
                 ZUNE_STYLE(.stroke_color = ZUNE_COLOR_ARGB32(200, 64, 255, 128),
                            .stroke_width = 2,
                            .corner_radius = 6,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(515, 300, 45, 35),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
                            .stroke_color = ZUNE_COLOR_ARGB32(255, 255, 255, 255),
                            .stroke_width = 3,
                            .corner_radius = 10,
                            .flags = ZUNE_SHAPE_ANTIALIAS));

    /* Gradient AA */
    struct ZuneGradientStop gradient_stops[] = {
        {0.0f, ZUNE_COLOR_ARGB32(255, 0, 128, 255)},
        {0.5f, ZUNE_COLOR_ARGB32(255, 0, 255, 128)},
        {1.0f, ZUNE_COLOR_ARGB32(255, 128, 255, 0)},
    };
    struct ZuneBrush gradient_brush = {
        .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
        .flags = 0, .internal = NULL,
        .data = {.linear = {
            .start = ZUNE_POINT_LITERAL(0, 0),
            .end = ZUNE_POINT_LITERAL(180, 0),
            .stops = gradient_stops,
            .stop_count = 3
        }}
    };
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(420, 350, 180, 80),
                 ZUNE_STYLE(.fill = &gradient_brush,
                            .stroke_color = ZUNE_RED,
                            .stroke_width = 5,
                            .corner_radius = 18,
                            .flags = ZUNE_SHAPE_ANTIALIAS));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("AA Rectangle Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("============================================\n\n");

    if (!DemoInit(&ctx, "AA Rectangle Demo (Unlocked)", 640, 480, backend, 0))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawAARectangles(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
