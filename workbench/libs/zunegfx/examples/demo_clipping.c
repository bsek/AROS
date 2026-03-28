/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Clipping Demo

    Tests clipping functions:
    - ZuneSetClipRegion / ZuneClearClipRegion
    - ZuneCreateCircleRegion
    - ZuneCreateRoundedRectRegion
    - ZuneCombineRegions

    Usage: demo_clipping [cybergfx|opengl]
*/

#include "demo_common.h"
#include <graphics/regions.h>

static void RunClippingDemo(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;

    ZuneSetFont(rctx, ctx->window->RPort->Font);

    /* No clipping - reference */
    ZuneDrawTextAt(rctx, 20, 10, "No clipping (reference):", 24, ZUNE_WHITE);
    ZuneDrawRectangleXYWH(rctx, 20, 30, 150, 100, ZUNE_BRUSH_SOLID(ZUNE_RED));
    ZuneDrawRectangleXYWH(rctx, 50, 50, 150, 100, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
    ZuneDrawRectangleXYWH(rctx, 80, 70, 150, 100, ZUNE_BRUSH_SOLID(ZUNE_BLUE));

    /* Rectangular clip region */
    ZuneDrawTextAt(rctx, 300, 10, "Rectangular clip:", 17, ZUNE_WHITE);
    {
        struct ZuneRect clip_rect = {300, 30, 120, 80};
        struct Region *region = ZuneCreateRoundedRectRegion(&clip_rect, 0);
        if (region) {
            ZuneSetClipRegion(rctx, region);
            ZuneDrawRectangleXYWH(rctx, 280, 20, 150, 100, ZUNE_BRUSH_SOLID(ZUNE_RED));
            ZuneDrawRectangleXYWH(rctx, 310, 40, 150, 100, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
            ZuneDrawRectangleXYWH(rctx, 340, 60, 150, 100, ZUNE_BRUSH_SOLID(ZUNE_BLUE));
            ZuneClearClipRegion(rctx);
            DisposeRegion(region);
        }
        /* Show clip boundary */
        ZuneDrawRectangleOutlineXYWH(rctx, 300, 30, 120, 80, ZUNE_YELLOW);
    }

    /* Circle clip region */
    ZuneDrawTextAt(rctx, 20, 155, "Circle clip:", 12, ZUNE_WHITE);
    {
        struct ZunePoint center = {120, 240};
        struct Region *region = ZuneCreateCircleRegion(&center, 60);
        if (region) {
            ZuneSetClipRegion(rctx, region);
            ZuneDrawRectangleXYWH(rctx, 40, 170, 160, 140, ZUNE_BRUSH_SOLID(ZUNE_RED));
            ZuneDrawRectangleXYWH(rctx, 70, 190, 160, 140, ZUNE_BRUSH_SOLID(ZUNE_GREEN));
            ZuneClearClipRegion(rctx);
            DisposeRegion(region);
        }
        ZuneDrawCircleOutlineAt(rctx, 120, 240, 60, ZUNE_YELLOW);
    }

    /* Rounded rect clip */
    ZuneDrawTextAt(rctx, 300, 155, "Rounded rect clip:", 18, ZUNE_WHITE);
    {
        struct ZuneRect clip_rect = {300, 175, 150, 100};
        struct Region *region = ZuneCreateRoundedRectRegion(&clip_rect, 20);
        if (region) {
            ZuneSetClipRegion(rctx, region);
            /* Draw stripes to show the rounded corners */
            for (int i = 0; i < 15; i++) {
                ULONG color = (i % 2) ? ZUNE_COLOR_ARGB32(255, 100, 100, 200)
                                      : ZUNE_COLOR_ARGB32(255, 200, 100, 100);
                ZuneDrawRectangleXYWH(rctx, 300, 175 + i * 7, 150, 7,
                                      ZUNE_BRUSH_SOLID(color));
            }
            ZuneClearClipRegion(rctx);
            DisposeRegion(region);
        }
        ZuneDrawRectangleRoundedOutlineXYWH(rctx, 300, 175, 150, 100, 20, ZUNE_YELLOW);
    }

    /* Combined regions using OrRegionRegion */
    ZuneDrawTextAt(rctx, 20, 320, "Combined (circle OR rect):", 26, ZUNE_WHITE);
    {
        struct ZunePoint center = {120, 400};
        struct Region *circle = ZuneCreateCircleRegion(&center, 50);
        struct ZuneRect rect = {80, 380, 120, 60};
        struct Region *rrect = ZuneCreateRoundedRectRegion(&rect, 0);

        if (circle && rrect) {
            /* Combine: OR the rectangle into the circle region */
            OrRegionRegion(rrect, circle);

            ZuneSetClipRegion(rctx, circle);

            /* Draw gradient background to show combined clip shape */
            for (int y = 340; y < 460; y++) {
                UBYTE v = (UBYTE)((y - 340) * 255 / 120);
                ZuneDrawRectangleXYWH(rctx, 50, y, 200, 1,
                                      ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(255, v, 100, (UBYTE)(255 - v))));
            }

            ZuneClearClipRegion(rctx);
        }
        if (circle) DisposeRegion(circle);
        if (rrect) DisposeRegion(rrect);
    }
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Clipping Demo - %s\n", DemoBackendName(backend));
    printf("=================================\n\n");

    if (!DemoInit(&ctx, "Clipping Demo", 500, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 30, 30, 50));
    RunClippingDemo(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
