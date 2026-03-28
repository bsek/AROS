/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Rectangle Demo (Locked DrawingBoard)

    Tests all non-AA rectangle functions with locked pixel access:
    - ZuneDrawRectangle (solid, gradient, texture brush)
    - ZuneDrawRectangleOutline
    - ZuneDrawRectangleOutlineStyled
    - ZuneDrawRectangleRounded (solid, gradient, texture brush)
    - ZuneDrawRectangleRoundedOutline
    - ZuneDrawRectangleRoundedOutlineStyled
    - ZuneDrawRectangleRoundedStyled (fill + border)

    Usage: demo_rect_locked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawRectangles(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Solid filled rectangles */
    ZuneDrawRectangleXYWH(rctx, 50, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneDrawRectangleXYWH(rctx, 170, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneDrawRectangleXYWH(rctx, 290, 50, 100, 80, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    /* Outlined rectangles */
    ZuneDrawRectangleOutlineXYWH(rctx, 50, 150, 100, 80, ZUNE_YELLOW);
    ZuneDrawRectangleOutlineXYWH(rctx, 170, 150, 100, 80, ZUNE_MAGENTA);
    ZuneDrawRectangleOutlineStyledXYWH(rctx, 290, 150, 100, 80, 5, ZUNE_CYAN);

    /* Rounded filled rectangles */
    ZuneDrawRectangleRoundedXYWH(rctx, 50, 250, 100, 80, 15, ZUNE_BRUSH_SOLID(ZUNE_WHITE));
    ZuneDrawRectangleRoundedXYWH(rctx, 170, 250, 100, 80, 20, ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY));
    ZuneDrawRectangleRoundedXYWH(rctx, 290, 250, 100, 80, 25, ZUNE_BRUSH_SOLID(ZUNE_GRAY));

    /* Rounded outlined rectangles */
    ZuneDrawRectangleRoundedOutlineXYWH(rctx, 50, 350, 100, 80, 10, ZUNE_RED);
    ZuneDrawRectangleRoundedOutlineXYWH(rctx, 170, 350, 100, 80, 15, ZUNE_GREEN);
    ZuneDrawRectangleRoundedOutlineXYWH(rctx, 290, 350, 100, 80, 20, ZUNE_BLUE);

    /* Semi-transparent overlapping rectangles */
    ZuneDrawRectangleXYWH(rctx, 420, 100, 150, 100,
                          ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
    ZuneDrawRectangleRoundedXYWH(rctx, 450, 130, 150, 100, 30,
                                 ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 0, 255, 255)));

    /* Gradient brush */
    struct ZuneGradientStop gradient_stops[] = {
        {0.0f, ZUNE_COLOR_ARGB32(255, 255, 128, 0)},
        {0.5f, ZUNE_COLOR_ARGB32(255, 255, 0, 128)},
        {1.0f, ZUNE_COLOR_ARGB32(255, 128, 0, 255)},
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
    ZuneDrawRectangleRoundedXYWH(rctx, 420, 250, 180, 80, 20, &gradient_brush);

    /* Texture brush from testimage.png */
    {
        struct ZuneTexture *ptex = ZuneCreateTextureFromFile(rctx,
                                       "PROGDIR:testimage.png",
                                       ctx->window->WScreen,
                                       ZUNE_TEXTURE_WRAPPING);
        if (ptex) {
            struct ZuneBrush tex_brush = {
                .type = ZUNE_BRUSH_TYPE_TEXTURE,
                .flags = 0, .internal = {0},
                .data = {.texture = {
                    .texture = ptex,
                    .source = {0, 0, 64, 64},
                    .wrap_u = ZUNE_BRUSH_WRAP_REPEAT,
                    .wrap_v = ZUNE_BRUSH_WRAP_REPEAT,
                    .filter = ZUNE_BRUSH_FILTER_LINEAR
                }}
            };
            ZuneDrawRectangleRoundedXYWH(rctx, 420, 350, 180, 80, 20, &tex_brush);
            ZuneDestroyTexture(rctx, ptex);
        }
    }

    /* Rounded styled (fill + border) */
    ZuneDrawRectangleRoundedStyledXYWH(rctx, 620, 50, 140, 80, 15, 3,
                                       ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), ZUNE_RED);

    struct ZuneGradientStop styled_stops[] = {
        {0.0f, ZUNE_COLOR_ARGB32(255, 100, 150, 255)},
        {1.0f, ZUNE_COLOR_ARGB32(255, 50, 100, 200)},
    };
    struct ZuneBrush styled_brush = {
        .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
        .flags = 0, .internal = {0},
        .data = {.linear = {
            .start = ZUNE_POINT_LITERAL(0, 0),
            .end = ZUNE_POINT_LITERAL(0, 80),
            .stops = styled_stops,
            .stop_count = 2
        }}
    };
    ZuneDrawRectangleRoundedStyledXYWH(rctx, 620, 150, 140, 80, 20, 4,
                                       &styled_brush, ZUNE_WHITE);

    ZuneDrawRectangleRoundedStyledXYWH(rctx, 620, 250, 140, 80, 25, 1,
                                       ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 255, 200, 100)),
                                       ZUNE_COLOR_ARGB32(255, 150, 100, 50));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);
    APTR pixels;
    ULONG pitch;

    printf("Rectangle Demo (Locked) - %s\n", DemoBackendName(backend));
    printf("=========================================\n\n");

    if (!DemoInit(&ctx, "Rectangle Demo (Locked)", 800, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);

    pixels = ZuneLockDrawingBoardPixels(ctx.rctx, &pitch);
    if (!pixels) {
        printf("ERROR: Cannot lock DrawingBoard pixels\n");
        DemoCleanup(&ctx);
        return 1;
    }

    DrawRectangles(&ctx);

    ZuneUnlockDrawingBoardPixels(ctx.rctx);

    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
