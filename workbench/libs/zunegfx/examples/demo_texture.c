/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Texture Demo

    Tests all texture management and rendering functions:
    - ZuneCreateTexture, ZuneCreateTextureFromData
    - ZuneCreateTextureFromDrawingBoard, ZuneCreateTextureFromFile
    - ZuneDestroyTexture, ZuneIsTextureValid
    - ZuneUpdateTextureData
    - ZuneLockTexturePixels, ZuneUnlockTexturePixels
    - ZuneGetTexturePixel, ZuneSetTexturePixel
    - ZuneDrawTextureStyled: position, scaling, source region, tint, tiling

    Usage: demo_texture [cybergfx|opengl]
*/

#include "demo_common.h"

static void RunTextureDemo(struct DemoContext *ctx)
{
    struct RenderContext *rctx = ctx->rctx;
    ULONG tex_w = 64, tex_h = 64;
    ULONG data_size = tex_w * tex_h * 4;
    APTR pixel_data = AllocMem(data_size, MEMF_PUBLIC);
    struct ZuneTexture *tex = NULL;

    ZuneSetFont(rctx, ctx->window->RPort->Font);
    ZuneDrawTextAt(rctx, 20, 10, "Texture Demo", 12,
                   ZUNE_TEXTSTYLE(.color = ZUNE_WHITE));

    if (!pixel_data) {
        printf("ERROR: Cannot allocate pixel data\n");
        return;
    }

    /* Create gradient pattern */
    {
        ULONG *px = (ULONG *)pixel_data;
        ULONG i, j;
        for (i = 0; i < tex_h; i++)
            for (j = 0; j < tex_w; j++) {
                UBYTE r = (UBYTE)(255 * j / tex_w);
                UBYTE b = (UBYTE)(255 * i / tex_h);
                UBYTE g = (UBYTE)(128 + 127 * ((i + j) & 1));
                px[i * tex_w + j] = ZUNE_COLOR_ARGB32(255, r, g, b);
            }
    }

    tex = ZuneCreateTextureFromData(rctx, pixel_data, tex_w, tex_h, 32,
                                    ZUNE_TEXTURE_FORMAT_ARGB32, tex_w * 4,
                                    ZUNE_TEXTURE_HARDWARE);
    if (!tex) {
        printf("ERROR: Cannot create texture\n");
        FreeMem(pixel_data, data_size);
        return;
    }

    printf("  Texture created: %dx%d, valid=%d\n", tex_w, tex_h, ZuneIsTextureValid(tex));

    /* Plain: a dest with no extent takes the texture's own size */
    ZuneDrawTextAt(rctx, 20, 30, "Plain:", 6,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(20, 45, 0, 0), NULL);

    /* Scaled: a dest extent different from the source scales */
    ZuneDrawTextAt(rctx, 100, 30, "Scaled:", 7,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(100, 45, 128, 128), NULL);

    /* Tinted */
    ZuneDrawTextAt(rctx, 250, 30, "Tinted:", 7,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(250, 45, 0, 0),
                          ZUNE_TEXSTYLE(.tint = ZUNE_COLOR_ARGB32(128, 255, 0, 0)));

    /* Scaled and tinted at once */
    ZuneDrawTextAt(rctx, 330, 30, "ScaledTinted:", 13,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(330, 45, 96, 96),
                          ZUNE_TEXSTYLE(.tint = ZUNE_COLOR_ARGB32(128, 0, 255, 0)));

    /* A source sub-rect */
    ZuneDrawTextAt(rctx, 450, 30, "Region:", 7,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(450, 45, 64, 64),
                          ZUNE_TEXSTYLE(.source = ZUNE_RECT_PTR(16, 16, 32, 32)));

    /* Region and tint together */
    ZuneDrawTextAt(rctx, 530, 30, "RegionTinted:", 13,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(530, 45, 96, 96),
                          ZUNE_TEXSTYLE(.source = ZUNE_RECT_PTR(0, 0, 32, 32),
                                        .tint = ZUNE_COLOR_ARGB32(128, 0, 0, 255)));

    /* Tiled across the dest */
    ZuneDrawTextAt(rctx, 20, 190, "Tiled:", 6,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(20, 205, 200, 100),
                          ZUNE_TEXSTYLE(.flags = ZUNE_TEXTURE_TILE));

    /* Pixel manipulation */
    ZuneDrawTextAt(rctx, 250, 190, "Pixel manipulation:", 19,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    {
        ULONG pitch = 0;
        APTR lock = ZuneLockTexturePixels(rctx, tex, &pitch);
        if (lock) {
            struct ZunePoint p;
            /* Draw a white cross at center */
            for (int i = 0; i < 10; i++) {
                p.x = 27 + i; p.y = 32;
                ZuneSetTexturePixel(rctx, tex, &p, ZUNE_WHITE);
                p.x = 32; p.y = 27 + i;
                ZuneSetTexturePixel(rctx, tex, &p, ZUNE_WHITE);
            }

            p.x = 10; p.y = 10;
            ULONG sampled = ZuneGetTexturePixel(rctx, tex, &p);
            printf("  GetTexturePixel(10,10) = 0x%08X\n", (unsigned int)sampled);

            ZuneUnlockTexturePixels(rctx, tex);

            ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(250, 205, 0, 0), NULL);
        }
    }

    /* UpdateTextureData */
    ZuneDrawTextAt(rctx, 350, 190, "Updated data:", 13,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    {
        ULONG *px = (ULONG *)pixel_data;
        ULONG i, j;
        for (i = 0; i < tex_h; i++)
            for (j = 0; j < tex_w; j++) {
                UBYTE v = (UBYTE)(255 * ((i + j) % 16) / 15);
                px[i * tex_w + j] = ZUNE_COLOR_ARGB32(255, v, v, v);
            }
        struct ZuneRect rect = {0, 0, tex_w, tex_h};
        ZuneUpdateTextureData(rctx, tex, pixel_data, &rect);

        ZuneDrawTextureStyled(rctx, tex, ZUNE_RECT_PTR(350, 205, 0, 0), NULL);
    }

    /* CreateTextureFromFile */
    ZuneDrawTextAt(rctx, 20, 320, "FromFile (testimage.png):", 24,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    {
        struct ZuneTexture *ftex = ZuneCreateTextureFromFile(rctx,
                                       "PROGDIR:testimage.png",
                                       ZUNE_TEXTURE_HARDWARE);
        if (ftex) {
            printf("  TextureFromFile: valid=%d\n", ZuneIsTextureValid(ftex));

            ZuneDrawTextureStyled(rctx, ftex, ZUNE_RECT_PTR(20, 335, 0, 0), NULL);
            ZuneDrawTextureStyled(rctx, ftex, ZUNE_RECT_PTR(160, 335, 96, 96), NULL);
            ZuneDrawTextureStyled(rctx, ftex, ZUNE_RECT_PTR(270, 335, 64, 64),
                                  ZUNE_TEXSTYLE(.tint = ZUNE_COLOR_ARGB32(128, 255, 0, 0)));

            ZuneDestroyTexture(rctx, ftex);
        } else {
            ZuneDrawTextAt(rctx, 20, 340, "FAILED - testimage.png not found", 32,
                   ZUNE_TEXTSTYLE(.color = ZUNE_RED));
            printf("  TextureFromFile: FAILED (testimage.png not found)\n");
        }
    }

    /* CreateTextureFromDrawingBoard */
    ZuneDrawTextAt(rctx, 400, 320, "FromDrawingBoard:", 17,
                   ZUNE_TEXTSTYLE(.color = ZUNE_LIGHTGRAY));
    {
        struct DrawingBoard *tmp = ZuneCreateDrawingBoardForRenderContext(rctx, 128, 128, 0);
        if (tmp) {
            ZuneSetTarget(rctx, tmp);
            ZuneClearDrawingBoard(rctx, ZUNE_DARKGRAY);
            ZuneDrawRect(rctx, ZUNE_RECT_PTR(10, 10, 50, 50),
                 ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_RED)));
            ZuneDrawCircle(rctx, ZUNE_POINT_PTR(90, 90), 25,
                   ZUNE_STYLE(.fill = ZUNE_BRUSH_SOLID(ZUNE_GREEN)));

            struct ZuneTexture *btex = ZuneCreateTextureFromDrawingBoard(rctx, ZUNE_TEXTURE_HARDWARE);
            ZuneSetTarget(rctx, ctx->board);

            if (btex) {
                ZuneDrawTextureStyled(rctx, btex, ZUNE_RECT_PTR(400, 335, 0, 0), NULL);
                ZuneDrawTextureStyled(rctx, btex, ZUNE_RECT_PTR(540, 335, 64, 64), NULL);

                ZuneDestroyTexture(rctx, btex);
            }
            ZuneDestroyDrawingBoard(rctx, tmp);
        }
    }

    ZuneDestroyTexture(rctx, tex);
    FreeMem(pixel_data, data_size);
}

int main(int argc, char **argv)
{
    struct DemoContext ctx;
    ULONG backend = ParseBackendArg(argc, argv);

    printf("Texture Demo - %s\n", DemoBackendName(backend));
    printf("================================\n\n");

    if (!DemoInit(&ctx, "Texture Demo", 700, 480, backend, 0))
        return 1;

    ZuneClearRenderContext(ctx.rctx, ZUNE_COLOR_ARGB32(255, 30, 30, 50));
    RunTextureDemo(&ctx);
    DemoPresent(&ctx);
    DemoWaitKey(&ctx);
    DemoCleanup(&ctx);
    return 0;
}
