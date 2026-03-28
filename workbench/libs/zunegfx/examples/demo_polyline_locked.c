/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Polyline Demo (Locked DrawingBoard)

    Tests polyline functions with locked pixel access:
    - ZuneDrawPolyline
    - ZuneDrawPolylineStyled (with width)

    Usage: demo_polyline_locked [cybergfx|opengl]
*/

#include "demo_common.h"
#include <math.h>

static void DrawPolylines(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;
    int i;

    ZuneSetFont(rctx, ctx->window->RPort->Font);

    /* Zigzag */
    {
        struct ZunePoint zigzag[] = {
            {30, 50}, {80, 100}, {130, 50}, {180, 100},
            {230, 50}, {280, 100}, {330, 50}
        };
        ZuneDrawPolyline(rctx, zigzag, 7, ZUNE_RED);
    }

    /* Sawtooth */
    {
        struct ZunePoint saw[] = {
            {30, 130}, {60, 160}, {60, 130}, {90, 160},
            {90, 130}, {120, 160}, {120, 130}, {150, 160},
            {150, 130}, {180, 160}, {180, 130}
        };
        ZuneDrawPolyline(rctx, saw, 11, ZUNE_GREEN);
    }

    /* Staircase */
    {
        struct ZunePoint stairs[] = {
            {30, 200}, {60, 200}, {60, 185}, {90, 185},
            {90, 170}, {120, 170}, {120, 155}, {150, 155},
            {150, 140}, {180, 140}
        };
        ZuneDrawPolyline(rctx, stairs, 10, ZUNE_YELLOW);
    }

    /* Styled polylines with different widths */
    ZuneDrawTextAt(rctx, 20, 220, "Styled (varying width):", 23, ZUNE_WHITE);

    {
        struct ZunePoint pts[] = {{30, 250}, {100, 280}, {170, 250}, {240, 280}, {310, 250}};
        ZuneDrawPolylineStyled(rctx, pts, 5, 1, ZUNE_CYAN);
        ZuneDrawTextAt(rctx, 320, 260, "w=1", 3, ZUNE_CYAN);
    }
    {
        struct ZunePoint pts[] = {{30, 290}, {100, 320}, {170, 290}, {240, 320}, {310, 290}};
        ZuneDrawPolylineStyled(rctx, pts, 5, 2, ZUNE_MAGENTA);
        ZuneDrawTextAt(rctx, 320, 300, "w=2", 3, ZUNE_MAGENTA);
    }
    {
        struct ZunePoint pts[] = {{30, 330}, {100, 360}, {170, 330}, {240, 360}, {310, 330}};
        ZuneDrawPolylineStyled(rctx, pts, 5, 3, ZUNE_COLOR_ARGB32(255, 255, 128, 0));
        ZuneDrawTextAt(rctx, 320, 340, "w=3", 3, ZUNE_COLOR_ARGB32(255, 255, 128, 0));
    }
    {
        struct ZunePoint pts[] = {{30, 380}, {100, 410}, {170, 380}, {240, 410}, {310, 380}};
        ZuneDrawPolylineStyled(rctx, pts, 5, 5, ZUNE_COLOR_ARGB32(255, 128, 255, 0));
        ZuneDrawTextAt(rctx, 320, 390, "w=5", 3, ZUNE_COLOR_ARGB32(255, 128, 255, 0));
    }

    /* Shapes: star, spiral, sine, hexagon */
    ZuneDrawTextAt(rctx, 430, 20, "Polyline shapes:", 16, ZUNE_WHITE);

    /* Closed star */
    {
        struct ZunePoint star[11];
        int cx = 500, cy = 100;
        for (i = 0; i < 10; i++) {
            float angle = (float)(i * 36 - 90) * 3.14159f / 180.0f;
            int r = (i % 2 == 0) ? 50 : 20;
            star[i].x = cx + (WORD)(r * cosf(angle));
            star[i].y = cy + (WORD)(r * sinf(angle));
        }
        star[10] = star[0];
        ZuneDrawPolylineStyled(rctx, star, 11, 2, ZUNE_YELLOW);
    }

    /* Spiral */
    {
        struct ZunePoint spiral[40];
        int cx = 650, cy = 100;
        for (i = 0; i < 40; i++) {
            float angle = (float)(i * 20) * 3.14159f / 180.0f;
            float r = 5.0f + (float)i * 1.2f;
            spiral[i].x = cx + (WORD)(r * cosf(angle));
            spiral[i].y = cy + (WORD)(r * sinf(angle));
        }
        ZuneDrawPolyline(rctx, spiral, 40, ZUNE_COLOR_ARGB32(255, 100, 200, 255));
    }

    /* Sine wave */
    {
        struct ZunePoint sine[60];
        for (i = 0; i < 60; i++) {
            sine[i].x = 430 + i * 6;
            sine[i].y = 250 + (WORD)(40.0f * sinf((float)i * 0.15f));
        }
        ZuneDrawPolylineStyled(rctx, sine, 60, 2, ZUNE_COLOR_ARGB32(255, 255, 100, 200));
    }

    /* Hexagon */
    {
        struct ZunePoint hex[7];
        int cx = 670, cy = 370;
        for (i = 0; i < 6; i++) {
            float angle = (float)(i * 60 - 30) * 3.14159f / 180.0f;
            hex[i].x = cx + (WORD)(40 * cosf(angle));
            hex[i].y = cy + (WORD)(40 * sinf(angle));
        }
        hex[6] = hex[0];
        ZuneDrawPolylineStyled(rctx, hex, 7, 3, ZUNE_GREEN);
    }
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);
    APTR pixels;
    ULONG pitch;

    printf("Polyline Demo (Locked) - %s\n", DemoBackendName(backend));
    printf("========================================\n\n");

    if (!DemoInit(&ctx, "Polyline Demo (Locked)", 800, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 40, 40, 60));

    pixels = ZuneLockDrawingBoardPixels(ctx.rctx, &pitch);
    if (!pixels) {
        printf("ERROR: Cannot lock DrawingBoard pixels\n");
        DemoCleanup(&ctx);
        return 1;
    }

    DrawPolylines(&ctx);

    ZuneUnlockDrawingBoardPixels(ctx.rctx);

    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
