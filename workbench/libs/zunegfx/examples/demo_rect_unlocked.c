/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Rectangle Demo (Unlocked DrawingBoard)

    Tests all non-AA rectangle functions with standard rastport operations:
    - ZuneDrawRectangle (solid, gradient, texture brush)
    - ZuneDrawRectangleOutline
    - ZuneDrawRectangleOutlineStyled
    - ZuneDrawRectangleRounded (solid, gradient, texture brush)
    - ZuneDrawRectangleRoundedOutline
    - ZuneDrawRectangleRoundedOutlineStyled
    - ZuneDrawRectangleRoundedStyled (fill + border)

    Usage: demo_rect_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawRectangles(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Solid filled rectangles */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 50, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_RED)));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 50, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GREEN)));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 50, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_BLUE)));

    /* Outlined rectangles */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 150, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_YELLOW, .stroke_width = 1));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 150, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA, .stroke_width = 1));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 150, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_CYAN, .stroke_width = 5));

    /* Rounded filled rectangles */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 250, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_WHITE), .corner_radius = 15));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 250, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), .corner_radius = 20));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 250, 100, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GRAY), .corner_radius = 25));

    /* Rounded outlined rectangles */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(50, 350, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_RED,
                            .stroke_width = 1,
                            .corner_radius = 10));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(170, 350, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_GREEN,
                            .stroke_width = 1,
                            .corner_radius = 15));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(290, 350, 100, 80),
                 ZUNE_STYLE(.stroke_color = ZUNE_BLUE,
                            .stroke_width = 1,
                            .corner_radius = 20));

    /* Semi-transparent overlapping rectangles */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(420, 100, 150, 100),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0))));
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(450, 130, 150, 100),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 0, 255, 255)),
                            .corner_radius = 30));

    /* Gradient brush */
    struct ZuneGradientStop gradient_stops[] = {
        {0.0f, ZUNE_COLOR_ARGB32(255, 255, 128, 0)},
        {0.5f, ZUNE_COLOR_ARGB32(255, 255, 0, 128)},
        {1.0f, ZUNE_COLOR_ARGB32(255, 128, 0, 255)},
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
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(420, 250, 180, 80),
                 ZUNE_STYLE(.fill = &gradient_brush, .corner_radius = 20));

    /* Texture brush from testimage.png */
    {
        struct ZuneTexture *ptex = ZuneCreateTextureFromFile(rctx,
                                       "PROGDIR:testimage.png",
                                       ZUNE_TEXTURE_WRAPPING);
        if (ptex) {
            struct ZuneBrush tex_brush = {
                .type = ZUNE_BRUSH_TYPE_TEXTURE,
                .flags = 0, .internal = NULL,
                .data = {.texture = {
                    .texture = ptex,
                    .source = {0, 0, 64, 64},
                    .wrap_u = ZUNE_BRUSH_WRAP_REPEAT,
                    .wrap_v = ZUNE_BRUSH_WRAP_REPEAT,
                    .filter = ZUNE_BRUSH_FILTER_LINEAR
                }}
            };
            ZuneDrawRect(rctx, ZUNE_RECT_PTR(420, 350, 180, 80),
                 ZUNE_STYLE(.fill = &tex_brush, .corner_radius = 20));
            ZuneDestroyTexture(rctx, ptex);
        }
    }

    /* Rounded styled (fill + border) */
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(620, 50, 140, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY),
                            .stroke_color = ZUNE_RED,
                            .stroke_width = 3,
                            .corner_radius = 15));

    struct ZuneGradientStop styled_stops[] = {
        {0.0f, ZUNE_COLOR_ARGB32(255, 100, 150, 255)},
        {1.0f, ZUNE_COLOR_ARGB32(255, 50, 100, 200)},
    };
    struct ZuneBrush styled_brush = {
        .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
        .flags = 0, .internal = NULL,
        .data = {.linear = {
            .start = ZUNE_POINT_LITERAL(0, 0),
            .end = ZUNE_POINT_LITERAL(0, 80),
            .stops = styled_stops,
            .stop_count = 2
        }}
    };
    ZuneDrawRect(rctx, ZUNE_RECT_PTR(620, 150, 140, 80),
                 ZUNE_STYLE(.fill = &styled_brush,
                            .stroke_color = ZUNE_WHITE,
                            .stroke_width = 4,
                            .corner_radius = 20));

    ZuneDrawRect(rctx, ZUNE_RECT_PTR(620, 250, 140, 80),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 255, 200, 100)),
                            .stroke_color = ZUNE_COLOR_ARGB32(255, 150, 100, 50),
                            .stroke_width = 1,
                            .corner_radius = 25));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Rectangle Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("===========================================\n\n");

    if (!DemoInit(&ctx, "Rectangle Demo (Unlocked)", 800, 480, backend, 0))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawRectangles(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
