/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Line Demo (Unlocked DrawingBoard)

    Tests non-AA line functions with standard rastport operations:
    - ZuneDrawLine
    - ZuneDrawLineStyled (with thickness)

    Usage: demo_line_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawLines(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Horizontal lines */
    ZuneDrawLinePoints(rctx, 50, 50, 300, 50, ZUNE_RED);
    ZuneDrawLinePoints(rctx, 50, 70, 300, 70, ZUNE_GREEN);
    ZuneDrawLinePoints(rctx, 50, 90, 300, 90, ZUNE_BLUE);

    /* Vertical lines */
    ZuneDrawLinePoints(rctx, 350, 50, 350, 200, ZUNE_YELLOW);
    ZuneDrawLinePoints(rctx, 370, 50, 370, 200, ZUNE_MAGENTA);
    ZuneDrawLinePoints(rctx, 390, 50, 390, 200, ZUNE_CYAN);

    /* Diagonal lines */
    ZuneDrawLinePoints(rctx, 450, 50, 550, 150, ZUNE_WHITE);
    ZuneDrawLinePoints(rctx, 450, 150, 550, 50, ZUNE_LIGHTGRAY);

    /* Thick horizontal lines */
    ZuneDrawLineStyledPoints(rctx, 50, 250, 300, 250, 2, ZUNE_RED);
    ZuneDrawLineStyledPoints(rctx, 50, 270, 300, 270, 3, ZUNE_GREEN);
    ZuneDrawLineStyledPoints(rctx, 50, 295, 300, 295, 5, ZUNE_BLUE);

    /* Thick vertical lines */
    ZuneDrawLineStyledPoints(rctx, 350, 250, 350, 350, 2, ZUNE_YELLOW);
    ZuneDrawLineStyledPoints(rctx, 370, 250, 370, 350, 3, ZUNE_MAGENTA);
    ZuneDrawLineStyledPoints(rctx, 390, 250, 390, 350, 10, ZUNE_CYAN);

    /* Thick diagonal lines */
    ZuneDrawLineStyledPoints(rctx, 450, 250, 550, 350, 3, ZUNE_WHITE);
    ZuneDrawLineStyledPoints(rctx, 450, 350, 550, 250, 6, ZUNE_YELLOW);
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Line Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("======================================\n\n");

    if (!DemoInit(&ctx, "Line Demo (Unlocked)", 640, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawLines(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
