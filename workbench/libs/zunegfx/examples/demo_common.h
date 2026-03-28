/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Shared Demo Infrastructure
*/

#ifndef DEMO_COMMON_H
#define DEMO_COMMON_H

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/zunegfx.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/zunegfx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DemoContext {
    struct Library *ZuneGfxBase;
    struct Library *CyberGfxBase;
    struct IntuitionBase *IntuitionBase;
    struct GfxBase *GfxBase;

    struct Screen *screen;
    struct Window *window;
    struct DrawingBoard *board;
    struct RenderContext *rctx;

    ULONG backend;
    UWORD width;
    UWORD height;
};

/*
 * ParseBackendArg - Parse command line argument for backend selection
 *
 * Accepts "opengl" or "cybergfx" (case insensitive). Defaults to BACKEND_CYBERGFX.
 */
ULONG ParseBackendArg(int argc, char **argv);

/*
 * DemoInit - Initialize demo window and rendering context
 *
 * Opens libraries, creates window, RenderContext and DrawingBoard.
 * Returns TRUE on success.
 */
BOOL DemoInit(struct DemoContext *ctx, const char *title, UWORD width, UWORD height, ULONG backend);

/*
 * DemoPresent - Present DrawingBoard to window
 */
void DemoPresent(struct DemoContext *ctx);

/*
 * DemoWaitKey - Wait for ENTER key press on stdin
 */
void DemoWaitKey(struct DemoContext *ctx);

/*
 * DemoCleanup - Free all resources
 */
void DemoCleanup(struct DemoContext *ctx);

/*
 * DemoBackendName - Return human-readable backend name
 */
const char *DemoBackendName(ULONG backend);

/* Include the implementation — each demo is a standalone program */
#include "demo_common.c"

#endif /* DEMO_COMMON_H */
