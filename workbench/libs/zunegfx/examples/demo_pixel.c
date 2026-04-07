/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Pixel Access Demo

    Tests direct pixel access functions:
    - ZuneDrawPixel
    - ZuneGetPixel
    - ZuneSetPixel
    - ZuneLockDrawingBoardPixels / ZuneUnlockDrawingBoardPixels

    Usage: demo_pixel [cybergfx|opengl]
*/

#include "demo_common.h"
#include <math.h>

static void RunPixelDemo(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    ZuneSetFont(rctx, ctx->window->RPort->Font);

    /* DrawPixel - individual pixels forming a pattern */
    ZuneDrawTextAt(rctx, 20, 10, "ZuneDrawPixel - gradient pattern:", 33, ZUNE_WHITE);
    {
        int x, y;
        for (y = 0; y < 100; y++) {
            for (x = 0; x < 200; x++) {
                UBYTE r = (UBYTE)(255 * x / 200);
                UBYTE g = (UBYTE)(255 * y / 100);
                UBYTE b = (UBYTE)(128);
                struct ZunePoint p = {(WORD)(20 + x), (WORD)(30 + y)};
                ZuneDrawPixel(rctx, &p, ZUNE_COLOR_ARGB32(255, r, g, b));
            }
        }
    }

    /* GetPixel - sample back and display */
    ZuneDrawTextAt(rctx, 250, 10, "ZuneGetPixel samples:", 21, ZUNE_WHITE);
    {
        struct ZunePoint p;
        char buf[60];
        int y = 30;

        p.x = 20; p.y = 30;
        ULONG c1 = ZuneGetPixel(rctx, &p);
        snprintf(buf, sizeof(buf), "(20,30) = 0x%08X", (unsigned int)c1);
        ZuneDrawTextAt(rctx, 250, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
        y += 16;

        p.x = 120; p.y = 80;
        ULONG c2 = ZuneGetPixel(rctx, &p);
        snprintf(buf, sizeof(buf), "(120,80) = 0x%08X", (unsigned int)c2);
        ZuneDrawTextAt(rctx, 250, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
        y += 16;

        p.x = 219; p.y = 129;
        ULONG c3 = ZuneGetPixel(rctx, &p);
        snprintf(buf, sizeof(buf), "(219,129) = 0x%08X", (unsigned int)c3);
        ZuneDrawTextAt(rctx, 250, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    }

    /* Locked pixel access - Mandelbrot set */
    ZuneDrawTextAt(rctx, 20, 145, "Locked pixels - Mandelbrot:", 27, ZUNE_WHITE);
    {
        APTR pixels;
        ULONG pitch;

        pixels = ZuneLockDrawingBoardPixels(rctx, &pitch);
        if (pixels) {
            int x, y;
            int w = 300, h = 200;
            int ox = 20, oy = 160;

            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    float cx = -2.0f + 3.0f * x / w;
                    float cy = -1.0f + 2.0f * y / h;
                    float zx = 0, zy = 0;
                    int iter = 0, max_iter = 32;

                    while (zx * zx + zy * zy < 4.0f && iter < max_iter) {
                        float tmp = zx * zx - zy * zy + cx;
                        zy = 2 * zx * zy + cy;
                        zx = tmp;
                        iter++;
                    }

                    ULONG color;
                    if (iter == max_iter) {
                        color = ZUNE_BLACK;
                    } else {
                        UBYTE v = (UBYTE)(255 * iter / max_iter);
                        color = ZUNE_COLOR_ARGB32(255, v, (UBYTE)(v / 2), (UBYTE)(255 - v));
                    }

                    struct ZunePoint p = {(WORD)(ox + x), (WORD)(oy + y)};
                    ZuneSetPixel(rctx, &p, color);
                }
            }

            ZuneUnlockDrawingBoardPixels(rctx);
            printf("  Mandelbrot rendered with locked pixels\n");
        } else {
            printf("  ERROR: Cannot lock pixels\n");
        }
    }

    /* SetPixel + GetPixel verification */
    ZuneDrawTextAt(rctx, 350, 145, "SetPixel + verify:", 18, ZUNE_WHITE);
    {
        struct ZunePoint p;
        char buf[60];
        int y = 165;

        /* Set specific pixels and read them back */
        p.x = 350; p.y = 165;
        ZuneSetPixel(rctx, &p, ZUNE_RED);
        ULONG got = ZuneGetPixel(rctx, &p);
        snprintf(buf, sizeof(buf), "Set RED, got 0x%08X", (unsigned int)got);
        ZuneDrawTextAt(rctx, 360, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
        y += 16;

        p.x = 350; p.y = 185;
        ZuneSetPixel(rctx, &p, ZUNE_GREEN);
        got = ZuneGetPixel(rctx, &p);
        snprintf(buf, sizeof(buf), "Set GREEN, got 0x%08X", (unsigned int)got);
        ZuneDrawTextAt(rctx, 360, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
        y += 16;

        p.x = 350; p.y = 205;
        ZuneSetPixel(rctx, &p, ZUNE_BLUE);
        got = ZuneGetPixel(rctx, &p);
        snprintf(buf, sizeof(buf), "Set BLUE, got 0x%08X", (unsigned int)got);
        ZuneDrawTextAt(rctx, 360, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    }
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Pixel Demo - %s\n", DemoBackendName(backend));
    printf("==============================\n\n");

    if (!DemoInit(&ctx, "Pixel Demo", 640, 400, backend, ZUNE_DRAWINGBOARD_LINEARMEM))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 30, 30, 50));
    RunPixelDemo(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
