/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Text Demo (Locked DrawingBoard)

    Tests text functions with locked pixel access:
    - ZuneSetFont
    - ZuneTextLength
    - ZuneTextFit
    - ZuneDrawText / ZuneDrawTextAt
    - ZuneDrawTextBackground / ZuneDrawTextBackgroundAt

    Usage: demo_text_locked [cybergfx|opengl]
*/

#include "demo_common.h"
#include <math.h>

static void DrawText(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;
    CONST_STRPTR test_string = "Hello, AROS! ZuneGfx text rendering works.";
    CONST_STRPTR long_string = "This is a longer string to test ZuneTextFit and see how many characters fit in a given width.";
    UWORD text_width, chars_fit;
    int y = 30;

    /* Use the screen's default font */
    ZuneSetFont(rctx, ctx->window->RPort->Font);

    /* Basic text drawing in different colors */
    ZuneDrawTextAt(rctx, 20, y, "=== Basic Text Drawing ===", 26, ZUNE_WHITE);
    y += 20;

    ZuneDrawTextAt(rctx, 20, y, test_string, strlen(test_string), ZUNE_RED);
    y += 16;
    ZuneDrawTextAt(rctx, 20, y, test_string, strlen(test_string), ZUNE_GREEN);
    y += 16;
    ZuneDrawTextAt(rctx, 20, y, test_string, strlen(test_string), ZUNE_BLUE);
    y += 16;
    ZuneDrawTextAt(rctx, 20, y, test_string, strlen(test_string), ZUNE_YELLOW);
    y += 16;
    ZuneDrawTextAt(rctx, 20, y, test_string, strlen(test_string),
                   ZUNE_COLOR_ARGB32(255, 255, 128, 0));
    y += 25;

    /* Text with background (JAM2) */
    ZuneDrawTextAt(rctx, 20, y, "=== Text With Background (JAM2) ===", 36, ZUNE_WHITE);
    y += 20;

    ZuneDrawTextBackgroundAt(rctx, 20, y, "White on Blue", 13, ZUNE_WHITE, ZUNE_BLUE);
    y += 16;
    ZuneDrawTextBackgroundAt(rctx, 20, y, "Black on Yellow", 15, ZUNE_BLACK, ZUNE_YELLOW);
    y += 16;
    ZuneDrawTextBackgroundAt(rctx, 20, y, "Red on Lightgray", 16, ZUNE_RED, ZUNE_LIGHTGRAY);
    y += 16;
    ZuneDrawTextBackgroundAt(rctx, 20, y, "Green on Darkgray", 17, ZUNE_GREEN, ZUNE_DARKGRAY);
    y += 25;

    /* Text measurement */
    ZuneDrawTextAt(rctx, 20, y, "=== Text Measurement ===", 24, ZUNE_WHITE);
    y += 20;

    text_width = ZuneTextLength(rctx, test_string, strlen(test_string));
    printf("   TextLength(\"%s\") = %d pixels\n", test_string, text_width);

    ZuneDrawRectangleOutlineXYWH(rctx, 19, y - 1, text_width + 2, 14, ZUNE_CYAN);
    ZuneDrawTextAt(rctx, 20, y, test_string, strlen(test_string), ZUNE_WHITE);
    y += 20;

    {
        char buf[80];
        snprintf(buf, sizeof(buf), "Measured width: %d pixels", text_width);
        ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    }
    y += 20;

    /* TextFit test */
    ZuneDrawTextAt(rctx, 20, y, "=== TextFit Test (max 200px) ===", 32, ZUNE_WHITE);
    y += 20;

    chars_fit = ZuneTextFit(rctx, long_string, strlen(long_string), 200);
    printf("   TextFit in 200px: %d chars of %d\n", chars_fit, (int)strlen(long_string));

    ZuneDrawRectangleXYWH(rctx, 20, y, 200, 14,
                          ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(80, 0, 128, 255)));
    ZuneDrawTextAt(rctx, 20, y, long_string, chars_fit, ZUNE_WHITE);
    y += 16;
    ZuneDrawTextAt(rctx, 20, y, long_string, strlen(long_string), ZUNE_GRAY);
    y += 20;

    {
        char buf[80];
        snprintf(buf, sizeof(buf), "%d of %d chars fit in 200 pixels",
                 chars_fit, (int)strlen(long_string));
        ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    }

    /* Right column: partial strings and single chars */
    y = 30;
    int x = 450;

    ZuneDrawTextAt(rctx, x, y, "=== Right Column ===", 20, ZUNE_WHITE);
    y += 20;

    ZuneDrawTextAt(rctx, x, y, "First 5:", 8, ZUNE_LIGHTGRAY);
    ZuneDrawTextAt(rctx, x + 80, y, test_string, 5, ZUNE_CYAN);
    y += 16;
    ZuneDrawTextAt(rctx, x, y, "First 10:", 9, ZUNE_LIGHTGRAY);
    ZuneDrawTextAt(rctx, x + 80, y, test_string, 10, ZUNE_CYAN);
    y += 16;
    ZuneDrawTextAt(rctx, x, y, "First 20:", 9, ZUNE_LIGHTGRAY);
    ZuneDrawTextAt(rctx, x + 80, y, test_string, 20, ZUNE_CYAN);
    y += 25;

    ZuneDrawTextAt(rctx, x, y, "Single chars:", 13, ZUNE_LIGHTGRAY);
    y += 16;
    {
        int cx = x;
        const char *letters = "ABCDEFGHIJKLM";
        int i;
        for (i = 0; i < 13; i++) {
            ULONG color = ZUNE_COLOR_ARGB32(255, (UBYTE)(255 - i * 19),
                                            (UBYTE)(i * 19), (UBYTE)(128 + i * 9));
            ZuneDrawTextAt(rctx, cx, y, &letters[i], 1, color);
            cx += ZuneTextLength(rctx, &letters[i], 1) + 2;
        }
    }
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);
    APTR pixels;
    ULONG pitch;

    printf("Text Demo (Locked) - %s\n", DemoBackendName(backend));
    printf("====================================\n\n");

    if (!DemoInit(&ctx, "Text Demo (Locked)", 800, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 40, 40, 60));

    pixels = ZuneLockDrawingBoardPixels(ctx.rctx, &pitch);
    if (!pixels) {
        printf("ERROR: Cannot lock DrawingBoard pixels\n");
        DemoCleanup(&ctx);
        return 1;
    }

    DrawText(&ctx);

    ZuneUnlockDrawingBoardPixels(ctx.rctx);

    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
