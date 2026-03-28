/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Batching Demo

    Tests batching/performance functions:
    - ZuneBeginBatch / ZuneEndBatch
    - ZuneFlushBatch
    - ZuneIsBatchingEnabled
    - ZuneGetBatchCount

    Usage: demo_batch [cybergfx|opengl]
*/

#include "demo_common.h"

static void RunBatchDemo(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;
    char buf[80];
    int y;

    ZuneSetFont(rctx, ctx->window->RPort->Font);
    ZuneDrawTextAt(rctx, 20, 10, "Batching Demo", 13, ZUNE_WHITE);

    y = 35;

    /* Show initial state */
    snprintf(buf, sizeof(buf), "Batching enabled: %s",
             ZuneIsBatchingEnabled(rctx) ? "YES" : "NO");
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    y += 16;

    snprintf(buf, sizeof(buf), "Batch count: %lu", (unsigned long)ZuneGetBatchCount(rctx));
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    y += 25;

    /* Begin batch */
    ZuneDrawTextAt(rctx, 20, y, "--- BeginBatch ---", 18, ZUNE_CYAN);
    y += 16;
    ZuneBeginBatch(rctx);

    snprintf(buf, sizeof(buf), "Batching enabled: %s",
             ZuneIsBatchingEnabled(rctx) ? "YES" : "NO");
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    y += 20;

    /* Draw multiple shapes in batch */
    ZuneDrawTextAt(rctx, 20, y, "Drawing 50 rectangles in batch...", 33, ZUNE_WHITE);
    y += 16;

    for (int i = 0; i < 50; i++) {
        UBYTE r = (UBYTE)(50 + i * 4);
        UBYTE g = (UBYTE)(200 - i * 3);
        UBYTE b = (UBYTE)(100 + i * 2);
        ZuneDrawRectangleXYWH(rctx, 20 + i * 10, y, 30, 20,
                              ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, r, g, b)));
    }
    y += 30;

    snprintf(buf, sizeof(buf), "Batch count after 50 rects: %lu",
             (unsigned long)ZuneGetBatchCount(rctx));
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    y += 20;

    /* Flush mid-batch */
    ZuneDrawTextAt(rctx, 20, y, "--- FlushBatch (mid-batch) ---", 30, ZUNE_CYAN);
    y += 16;
    ZuneFlushBatch(rctx);

    snprintf(buf, sizeof(buf), "Batch count after flush: %lu",
             (unsigned long)ZuneGetBatchCount(rctx));
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    y += 20;

    /* More drawing after flush */
    ZuneDrawTextAt(rctx, 20, y, "Drawing 30 circles...", 21, ZUNE_WHITE);
    y += 16;

    for (int i = 0; i < 30; i++) {
        UBYTE r = (UBYTE)(255 - i * 8);
        UBYTE g = (UBYTE)(i * 8);
        ZuneDrawCircleAt(rctx, 35 + i * 18, y + 15, 8,
                         ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(200, r, g, 128)));
    }
    y += 40;

    snprintf(buf, sizeof(buf), "Batch count after 30 circles: %lu",
             (unsigned long)ZuneGetBatchCount(rctx));
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
    y += 20;

    /* End batch */
    ZuneDrawTextAt(rctx, 20, y, "--- EndBatch ---", 16, ZUNE_CYAN);
    y += 16;
    ZuneEndBatch(rctx);

    snprintf(buf, sizeof(buf), "Batching enabled: %s, count: %lu",
             ZuneIsBatchingEnabled(rctx) ? "YES" : "NO",
             (unsigned long)ZuneGetBatchCount(rctx));
    ZuneDrawTextAt(rctx, 20, y, buf, strlen(buf), ZUNE_LIGHTGRAY);
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Batching Demo - %s\n", DemoBackendName(backend));
    printf("=================================\n\n");

    if (!DemoInit(&ctx, "Batching Demo", 640, 480, backend))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 30, 30, 50));
    RunBatchDemo(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
