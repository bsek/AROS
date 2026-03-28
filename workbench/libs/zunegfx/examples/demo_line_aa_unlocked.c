/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - AA Line Demo (Unlocked DrawingBoard)

    Tests antialiased line functions with standard rastport operations:
    - ZuneDrawLineAA
    - ZuneDrawLineStyledAA (with thickness)

    Usage: demo_line_aa_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawAALines(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Basic AA lines */
    ZuneDrawLineAAPoints(rctx, 50, 50, 300, 50, ZUNE_RED);
    ZuneDrawLineAAPoints(rctx, 50, 70, 300, 70, ZUNE_GREEN);
    ZuneDrawLineAAPoints(rctx, 50, 90, 300, 90, ZUNE_BLUE);

    /* AA diagonal lines to show smoothing */
    ZuneDrawLineAAPoints(rctx, 50, 120, 200, 180, ZUNE_YELLOW);
    ZuneDrawLineAAPoints(rctx, 50, 180, 200, 120, ZUNE_MAGENTA);
    ZuneDrawLineAAPoints(rctx, 220, 120, 370, 180, ZUNE_CYAN);
    ZuneDrawLineAAPoints(rctx, 220, 180, 370, 120, ZUNE_WHITE);

    /* Styled AA lines with different thicknesses */
    ZuneDrawLineStyledAAPoints(rctx, 50, 250, 300, 250, 2, ZUNE_RED);
    ZuneDrawLineStyledAAPoints(rctx, 50, 275, 300, 275, 3, ZUNE_GREEN);
    ZuneDrawLineStyledAAPoints(rctx, 50, 305, 300, 305, 5, ZUNE_BLUE);

    /* Thick AA diagonal lines */
    ZuneDrawLineStyledAAPoints(rctx, 350, 250, 500, 350, 2, ZUNE_YELLOW);
    ZuneDrawLineStyledAAPoints(rctx, 380, 250, 530, 350, 3, ZUNE_MAGENTA);
    ZuneDrawLineStyledAAPoints(rctx, 410, 250, 560, 350, 5, ZUNE_CYAN);
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("AA Line Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("========================================\n\n");

    if (!DemoInit(&ctx, "AA Line Demo (Unlocked)", 640, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawAALines(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
