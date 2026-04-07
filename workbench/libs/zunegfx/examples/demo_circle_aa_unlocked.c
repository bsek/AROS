/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - AA Circle Demo (Unlocked DrawingBoard)

    Tests antialiased circle functions with standard rastport operations:
    - ZuneFillCircleAA
    - ZuneFillCircleStyledAA (fill + border)
    - ZuneDrawCircleOutlineStyledAA

    Usage: demo_circle_aa_unlocked [cybergfx|opengl]
*/

#include "demo_common.h"

static void DrawAACircles(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    /* AA filled circles */
    ZuneFillCircleAAAt(rctx, 120, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneFillCircleAAAt(rctx, 240, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneFillCircleAAAt(rctx, 360, 90, 40, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    /* AA outlined circles with different widths */
    ZuneDrawCircleOutlineStyledAAAt(rctx, 120, 200, 40, 1, ZUNE_YELLOW);
    ZuneDrawCircleOutlineStyledAAAt(rctx, 240, 200, 40, 3, ZUNE_MAGENTA);
    ZuneDrawCircleOutlineStyledAAAt(rctx, 360, 200, 40, 5, ZUNE_CYAN);

    /* AA filled + border (styled) */
    ZuneFillCircleStyledAAAt(rctx, 480, 90, 30, 1,
                             ZUNE_BRUSH_SOLID(ZUNE_WHITE), ZUNE_LIGHTGRAY);
    ZuneFillCircleStyledAAAt(rctx, 480, 150, 25, 3,
                             ZUNE_BRUSH_SOLID(ZUNE_YELLOW), ZUNE_GRAY);
    ZuneFillCircleStyledAAAt(rctx, 480, 210, 20, 2,
                             ZUNE_BRUSH_SOLID(ZUNE_LIGHTGRAY), ZUNE_RED);

    /* Various line widths */
    ZuneDrawCircleOutlineStyledAAAt(rctx, 120, 320, 35, 1, ZUNE_RED);
    ZuneDrawCircleOutlineStyledAAAt(rctx, 240, 320, 30, 4, ZUNE_GREEN);
    ZuneDrawCircleOutlineStyledAAAt(rctx, 360, 320, 25, 6, ZUNE_BLUE);

    /* Overlapping AA */
    ZuneFillCircleAAAt(rctx, 480, 300, 45,
                       ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(128, 255, 255, 0)));
    ZuneDrawCircleOutlineStyledAAAt(rctx, 500, 320, 40, 4,
                                    ZUNE_COLOR_ARGB32(255, 255, 0, 0));

    /* Small AA circles */
    ZuneFillCircleAAAt(rctx, 520, 370, 15,
                       ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 255, 128, 64)));
    ZuneDrawCircleOutlineStyledAAAt(rctx, 550, 390, 12, 2,
                                    ZUNE_COLOR_ARGB32(200, 64, 255, 128));
    ZuneFillCircleStyledAAAt(rctx, 580, 410, 18, 3,
                             ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, 128, 64, 255)),
                             ZUNE_COLOR_ARGB32(255, 255, 255, 255));
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("AA Circle Demo (Unlocked) - %s\n", DemoBackendName(backend));
    printf("==========================================\n\n");

    if (!DemoInit(&ctx, "AA Circle Demo (Unlocked)", 640, 480, backend, 0))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_DARKGRAY);
    DrawAACircles(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
