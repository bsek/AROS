/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - AA Line Demo (Unlocked DrawingBoard)

    Tests antialiased line functions with standard rastport operations:

    Usage: demo_line_aa_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawAALines(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* Basic AA lines */
    ZuneDrawLinePoints(rctx, 50, 50, 300, 50,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_RED, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 70, 300, 70,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_GREEN, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 90, 300, 90,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_BLUE, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));

    /* AA diagonal lines to show smoothing */
    ZuneDrawLinePoints(rctx, 50, 120, 200, 180,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_YELLOW, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 180, 200, 120,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 220, 120, 370, 180,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_CYAN, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 220, 180, 370, 120,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_WHITE, .stroke_width = 1, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));

    /* Styled AA lines with different thicknesses */
    ZuneDrawLinePoints(rctx, 50, 250, 300, 250,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_RED, .stroke_width = 2, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 275, 300, 275,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_GREEN, .stroke_width = 3, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 50, 305, 300, 305,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_BLUE, .stroke_width = 5, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));

    /* Thick AA diagonal lines */
    ZuneDrawLinePoints(rctx, 350, 250, 500, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_YELLOW, .stroke_width = 2, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 380, 250, 530, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_MAGENTA, .stroke_width = 3, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
    ZuneDrawLinePoints(rctx, 410, 250, 560, 350,
                       ZUNE_STYLE(.stroke_color = ZUNE_STYLE(.stroke_color = ZUNE_CYAN, .stroke_width = 5, .flags = ZUNE_SHAPE_ANTIALIAS), .stroke_width = 1));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("AA Line Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("========================================\n\n");

    if (!DemoInit(&ctx, "AA Line Demo (Unlocked)", 640, 480, backend, 0))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawAALines(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
