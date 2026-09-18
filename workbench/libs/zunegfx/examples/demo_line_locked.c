/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Line Demo (Locked DrawingBoard)

    Tests non-AA line functions with locked pixel access:
    - ZuneDrawLine: width and antialiasing are style fields

    Usage: demo_line_locked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawLines(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Horizontal lines */
    ZuneDrawLinePoints(rctx, 50, 50, 300, 50,
                       ZUNE_STYLE(.stroke_color = ZUNE_RED, .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 70, 300, 70,
                       ZUNE_STYLE(.stroke_color = ZUNE_GREEN, .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 90, 300, 90,
                       ZUNE_STYLE(.stroke_color = ZUNE_BLUE, .stroke_width = 1));

    /* Vertical lines */
    ZuneDrawLinePoints(rctx, 350, 50, 350, 200,
                       ZUNE_STYLE(.stroke_color = ZUNE_YELLOW, .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 370, 50, 370, 200,
                       ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA, .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 390, 50, 390, 200,
                       ZUNE_STYLE(.stroke_color = ZUNE_CYAN, .stroke_width = 1));

    /* Diagonal lines */
    ZuneDrawLinePoints(rctx, 450, 50, 550, 150,
                       ZUNE_STYLE(.stroke_color = ZUNE_WHITE, .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 450, 150, 550, 50,
                       ZUNE_STYLE(.stroke_color = ZUNE_LIGHTGRAY, .stroke_width = 1));

    /* Thick horizontal lines */
    ZuneDrawLinePoints(rctx, 50, 250, 300, 250,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_RED, .stroke_width = 2), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 270, 300, 270,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_GREEN, .stroke_width = 3), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 295, 300, 295,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_BLUE, .stroke_width = 5), .stroke_width = 1));

    /* Thick vertical lines */
    ZuneDrawLinePoints(rctx, 350, 250, 350, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_YELLOW, .stroke_width = 2), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 370, 250, 370, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA, .stroke_width = 3), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 390, 250, 390, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_CYAN, .stroke_width = 10), .stroke_width = 1));

    /* Thick diagonal lines */
    ZuneDrawLinePoints(rctx, 450, 250, 550, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_WHITE, .stroke_width = 3), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 450, 350, 550, 250,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_YELLOW, .stroke_width = 6), .stroke_width = 1));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);
    APTR pixels;
    ULONG pitch;

    printf("Line Demo (Locked) - %s\n", DemoBackendName(backend));
    printf("====================================\n\n");

    if (!DemoInit(&ctx, "Line Demo (Locked)", 640, 480, backend, ZUNE_DRAWINGBOARD_LINEARMEM))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);

    pixels = ZuneLockDrawingBoardPixels(ctx.rctx, &pitch);
    if (!pixels) {
        printf("ERROR: Cannot lock DrawingBoard pixels\n");
        DemoCleanup(&ctx);
        return 1;
    }

    DrawLines(&ctx);

    ZuneUnlockDrawingBoardPixels(ctx.rctx);

    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
