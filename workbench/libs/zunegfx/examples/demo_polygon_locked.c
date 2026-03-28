/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Polygon Demo (Locked DrawingBoard)

    Tests polygon fill with locked pixel access:
    - ZuneFillPolygon with solid, gradient, and texture brushes

    Usage: demo_polygon_locked [cybergfx|opengl]
*/

#include "demo_common.h"
#include <math.h>

static void DrawPolygons(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;
    int i;

    ZuneSetFont(rctx, ctx->window->RPort->Font);

    /* Solid filled polygons */
    ZuneDrawTextAt(rctx, 20, 20, "Solid filled:", 13, ZUNE_WHITE);

    /* Triangle */
    {
        struct ZunePoint tri[] = {{80, 50}, {30, 140}, {130, 140}};
        ZuneFillPolygon(rctx, tri, 3, ZUNE_BRUSH_SOLID(ZUNE_RED));
    }

    /* Square */
    {
        struct ZunePoint sq[] = {{170, 50}, {250, 50}, {250, 140}, {170, 140}};
        ZuneFillPolygon(rctx, sq, 4, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    }

    /* Pentagon */
    {
        struct ZunePoint pent[5];
        int cx = 330, cy = 95;
        for (i = 0; i < 5; i++) {
            float angle = (float)(i * 72 - 90) * 3.14159f / 180.0f;
            pent[i].x = cx + (WORD)(45 * cosf(angle));
            pent[i].y = cy + (WORD)(45 * sinf(angle));
        }
        ZuneFillPolygon(rctx, pent, 5, ZUNE_BRUSH_SOLID(ZUNE_BLUE));
    }

    /* Hexagon */
    {
        struct ZunePoint hex[6];
        int cx = 440, cy = 95;
        for (i = 0; i < 6; i++) {
            float angle = (float)(i * 60 - 30) * 3.14159f / 180.0f;
            hex[i].x = cx + (WORD)(45 * cosf(angle));
            hex[i].y = cy + (WORD)(45 * sinf(angle));
        }
        ZuneFillPolygon(rctx, hex, 6, ZUNE_BRUSH_SOLID(ZUNE_YELLOW));
    }

    /* Octagon */
    {
        struct ZunePoint oct[8];
        int cx = 560, cy = 95;
        for (i = 0; i < 8; i++) {
            float angle = (float)(i * 45 - 22) * 3.14159f / 180.0f;
            oct[i].x = cx + (WORD)(45 * cosf(angle));
            oct[i].y = cy + (WORD)(45 * sinf(angle));
        }
        ZuneFillPolygon(rctx, oct, 8, ZUNE_BRUSH_SOLID(ZUNE_MAGENTA));
    }

    /* Star shapes */
    ZuneDrawTextAt(rctx, 20, 155, "Stars:", 6, ZUNE_WHITE);

    /* 5-pointed star */
    {
        struct ZunePoint star[10];
        int cx = 80, cy = 220;
        for (i = 0; i < 10; i++) {
            float angle = (float)(i * 36 - 90) * 3.14159f / 180.0f;
            int r = (i % 2 == 0) ? 45 : 18;
            star[i].x = cx + (WORD)(r * cosf(angle));
            star[i].y = cy + (WORD)(r * sinf(angle));
        }
        ZuneFillPolygon(rctx, star, 10, ZUNE_BRUSH_SOLID(ZUNE_YELLOW));
    }

    /* 6-pointed star */
    {
        struct ZunePoint star[12];
        int cx = 200, cy = 220;
        for (i = 0; i < 12; i++) {
            float angle = (float)(i * 30 - 90) * 3.14159f / 180.0f;
            int r = (i % 2 == 0) ? 45 : 22;
            star[i].x = cx + (WORD)(r * cosf(angle));
            star[i].y = cy + (WORD)(r * sinf(angle));
        }
        ZuneFillPolygon(rctx, star, 12, ZUNE_BRUSH_SOLID(ZUNE_CYAN));
    }

    /* 8-pointed star */
    {
        struct ZunePoint star[16];
        int cx = 330, cy = 220;
        for (i = 0; i < 16; i++) {
            float angle = (float)(i * 22.5f - 90) * 3.14159f / 180.0f;
            int r = (i % 2 == 0) ? 45 : 20;
            star[i].x = cx + (WORD)(r * cosf(angle));
            star[i].y = cy + (WORD)(r * sinf(angle));
        }
        ZuneFillPolygon(rctx, star, 16,
                        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 255, 128, 0)));
    }

    /* Gradient-filled polygons */
    ZuneDrawTextAt(rctx, 20, 280, "Gradient filled:", 16, ZUNE_WHITE);

    {
        static const struct ZuneGradientStop stops[] = {
            {0.0f, ZUNE_COLOR_ARGB32(255, 255, 0, 0)},
            {1.0f, ZUNE_COLOR_ARGB32(255, 0, 0, 255)},
        };
        struct ZuneBrush brush = {
            .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
            .flags = 0, .internal = {0},
            .data = {.linear = {
                .start = ZUNE_POINT_LITERAL(30, 0),
                .end = ZUNE_POINT_LITERAL(180, 0),
                .stops = stops, .stop_count = 2
            }}
        };
        struct ZunePoint tri[] = {{100, 300}, {30, 400}, {180, 400}};
        ZuneFillPolygon(rctx, tri, 3, &brush);
    }

    {
        static const struct ZuneGradientStop stops[] = {
            {0.0f, ZUNE_COLOR_ARGB32(255, 0, 255, 0)},
            {0.5f, ZUNE_COLOR_ARGB32(255, 255, 255, 0)},
            {1.0f, ZUNE_COLOR_ARGB32(255, 0, 128, 255)},
        };
        struct ZuneBrush brush = {
            .type = ZUNE_BRUSH_TYPE_LINEAR_GRADIENT,
            .flags = 0, .internal = {0},
            .data = {.linear = {
                .start = ZUNE_POINT_LITERAL(0, 300),
                .end = ZUNE_POINT_LITERAL(0, 400),
                .stops = stops, .stop_count = 3
            }}
        };
        struct ZunePoint hex[6];
        int cx = 290, cy = 350;
        for (i = 0; i < 6; i++) {
            float angle = (float)(i * 60 - 30) * 3.14159f / 180.0f;
            hex[i].x = cx + (WORD)(50 * cosf(angle));
            hex[i].y = cy + (WORD)(50 * sinf(angle));
        }
        ZuneFillPolygon(rctx, hex, 6, &brush);
    }

    /* Complex shapes */
    ZuneDrawTextAt(rctx, 430, 155, "Complex shapes:", 15, ZUNE_WHITE);

    /* Arrow */
    {
        struct ZunePoint arrow[] = {
            {430, 200}, {530, 200}, {530, 180}, {580, 210},
            {530, 240}, {530, 220}, {430, 220}
        };
        ZuneFillPolygon(rctx, arrow, 7,
                        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 100, 180, 255)));
    }

    /* L-shape */
    {
        struct ZunePoint lshape[] = {
            {620, 170}, {680, 170}, {680, 200}, {650, 200},
            {650, 250}, {620, 250}
        };
        ZuneFillPolygon(rctx, lshape, 6,
                        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 255, 180, 100)));
    }

    /* Cross */
    {
        struct ZunePoint cross[] = {
            {470, 280}, {500, 280}, {500, 300}, {530, 300},
            {530, 330}, {500, 330}, {500, 350}, {470, 350},
            {470, 330}, {440, 330}, {440, 300}, {470, 300}
        };
        ZuneFillPolygon(rctx, cross, 12,
                        ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, 255, 80, 80)));
    }

    /* Texture-filled polygons (from testimage.png) */
    ZuneDrawTextAt(rctx, 430, 370, "Texture filled:", 15, ZUNE_WHITE);

    {
        struct ZuneTexture *img_tex = ZuneCreateTextureFromFile(rctx,
                                          "PROGDIR:testimage.png",
                                          ctx->window->WScreen,
                                          ZUNE_TEXTURE_WRAPPING);
        if (img_tex) {
            struct ZuneBrush tex_brush = {
                .type = ZUNE_BRUSH_TYPE_TEXTURE,
                .flags = 0, .internal = {0},
                .data = {.texture = {
                    .texture = img_tex,
                    .source = {0, 0, 64, 64},
                    .wrap_u = ZUNE_BRUSH_WRAP_REPEAT,
                    .wrap_v = ZUNE_BRUSH_WRAP_REPEAT,
                    .filter = ZUNE_BRUSH_FILTER_LINEAR
                }}
            };

            struct ZunePoint hex[6];
            int cx = 500, cy = 420;
            for (i = 0; i < 6; i++) {
                float angle = (float)(i * 60 - 30) * 3.14159f / 180.0f;
                hex[i].x = cx + (WORD)(40 * cosf(angle));
                hex[i].y = cy + (WORD)(40 * sinf(angle));
            }
            ZuneFillPolygon(rctx, hex, 6, &tex_brush);

            struct ZunePoint star[10];
            cx = 630; cy = 420;
            for (i = 0; i < 10; i++) {
                float angle = (float)(i * 36 - 90) * 3.14159f / 180.0f;
                int r = (i % 2 == 0) ? 40 : 16;
                star[i].x = cx + (WORD)(r * cosf(angle));
                star[i].y = cy + (WORD)(r * sinf(angle));
            }
            ZuneFillPolygon(rctx, star, 10, &tex_brush);

            ZuneDestroyTexture(rctx, img_tex);
        }
    }
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);
    APTR pixels;
    ULONG pitch;

    printf("Polygon Demo (Locked) - %s\n", DemoBackendName(backend));
    printf("========================================\n\n");

    if (!DemoInit(&ctx, "Polygon Demo (Locked)", 800, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 40, 40, 60));

    pixels = ZuneLockDrawingBoardPixels(ctx.rctx, &pitch);
    if (!pixels) {
        printf("ERROR: Cannot lock DrawingBoard pixels\n");
        DemoCleanup(&ctx);
        return 1;
    }

    DrawPolygons(&ctx);

    ZuneUnlockDrawingBoardPixels(ctx.rctx);

    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
