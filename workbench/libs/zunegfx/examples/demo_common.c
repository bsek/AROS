/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Shared Demo Infrastructure
*/

#include "demo_common.h"

/* These globals are required by the AROS library auto-open mechanism */
struct Library *ZuneGfxBase = NULL;
struct Library *CyberGfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

ULONG ParseBackendArg(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcasecmp(argv[1], "opengl") == 0)
            return BACKEND_OPENGL;
        if (strcasecmp(argv[1], "cybergfx") == 0)
            return BACKEND_CYBERGFX;
        printf("Unknown backend '%s', using cybergfx\n", argv[1]);
    }
    return BACKEND_CYBERGFX;
}

const char *DemoBackendName(ULONG backend)
{
    switch (backend) {
        case BACKEND_OPENGL:    return "OpenGL";
        case BACKEND_CYBERGFX:  return "CyberGfx";
        case BACKEND_SOFTWARE:  return "Software";
        default:                return "BestAvailable";
    }
}

BOOL DemoInit(struct DemoContext *ctx, const char *title, UWORD width, UWORD height, ULONG backend, ULONG flags)
{
    WORD inner_width, inner_height;

    memset(ctx, 0, sizeof(*ctx));
    ctx->backend = backend;
    ctx->width = width;
    ctx->height = height;

    /* Open libraries */
    ZuneGfxBase = OpenLibrary("zunegfx.library", 1);
    if (!ZuneGfxBase) {
        printf("ERROR: Cannot open zunegfx.library\n");
        return FALSE;
    }
    ctx->ZuneGfxBase = ZuneGfxBase;

    CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
    ctx->CyberGfxBase = CyberGfxBase;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    if (!IntuitionBase) {
        printf("ERROR: Cannot open intuition.library\n");
        return FALSE;
    }
    ctx->IntuitionBase = IntuitionBase;

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!GfxBase) {
        printf("ERROR: Cannot open graphics.library\n");
        return FALSE;
    }
    ctx->GfxBase = GfxBase;

    /* Lock public screen */
    ctx->screen = LockPubScreen(NULL);
    if (!ctx->screen) {
        printf("ERROR: Cannot lock Workbench screen\n");
        return FALSE;
    }

    /* Open window */
    ctx->window = OpenWindowTags(NULL,
        WA_CustomScreen, (IPTR)ctx->screen,
        WA_Left, 50, WA_Top, 30,
        WA_Width, width, WA_Height, height,
        WA_Title, (IPTR)title,
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        TAG_DONE);
    if (!ctx->window) {
        printf("ERROR: Cannot open window\n");
        return FALSE;
    }

    inner_width = ctx->window->Width - ctx->window->BorderLeft - ctx->window->BorderRight;
    inner_height = ctx->window->Height - ctx->window->BorderTop - ctx->window->BorderBottom;

    /* Create RenderContext with selected backend */
    ctx->rctx = ZuneCreateRenderContextForWindow(ctx->window, ctx->screen->ViewPort.ColorMap, backend);
    if (!ctx->rctx) {
        printf("ERROR: Cannot create RenderContext (%s)\n", DemoBackendName(backend));
        return FALSE;
    }

    /* Create DrawingBoard */
    ctx->board = ZuneCreateDrawingBoardForRenderContext(ctx->rctx, inner_width, inner_height, flags);
    if (!ctx->board) {
        printf("ERROR: Cannot create DrawingBoard\n");
        return FALSE;
    }

    /* Set DrawingBoard as render target */
    ZuneSetTarget(ctx->rctx, ctx->board);

    printf("Initialized: %s backend, %dx%d\n", DemoBackendName(backend), inner_width, inner_height);
    return TRUE;
}

void DemoPresent(struct DemoContext *ctx)
{
    ZunePresent(ctx->rctx, 0, 0,
                ctx->window->BorderLeft, ctx->window->BorderTop,
                ctx->board->width, ctx->board->height);
}

void DemoWaitKey(struct DemoContext *ctx)
{
    printf("Press ENTER to continue...\n");
    getchar();
}

void DemoCleanup(struct DemoContext *ctx)
{
    if (ctx->board) {
        ZuneDestroyDrawingBoard(ctx->rctx, ctx->board);
        ctx->board = NULL;
    }
    if (ctx->rctx) {
        ZuneDestroyRenderContext(ctx->rctx);
        ctx->rctx = NULL;
    }
    if (ctx->window) {
        CloseWindow(ctx->window);
        ctx->window = NULL;
    }
    if (ctx->screen) {
        UnlockPubScreen(NULL, ctx->screen);
        ctx->screen = NULL;
    }
    if (ZuneGfxBase) {
        CloseLibrary(ZuneGfxBase);
        ZuneGfxBase = NULL;
    }
    if (CyberGfxBase) {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
    if (GfxBase) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
}
