/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Texture Test Example

    This example demonstrates the texture functionality of the Zune Renderer
    library, including texture creation, pixel manipulation, and rendering.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/zunerenderer.h>
#include <libraries/zunerenderer.h>

#include <stdio.h>
#include <stdlib.h>

/* Test configuration */
#define TEXTURE_WIDTH   64
#define TEXTURE_HEIGHT  64
#define WINDOW_WIDTH    400
#define WINDOW_HEIGHT   300

struct Library *ZuneRendererBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

/* Function prototypes */
BOOL OpenLibraries(void);
void CloseLibraries(void);
struct ZuneTexture *CreateTestTexture(void);
void DrawTestPattern(struct ZuneTexture *texture);
void RunTextureTest(void);

int main(void)
{
    printf("Zune Renderer Texture Test\n");
    printf("==========================\n\n");

    if (!OpenLibraries()) {
        printf("ERROR: Failed to open required libraries\n");
        return 20;
    }

    RunTextureTest();

    CloseLibraries();
    return 0;
}

BOOL OpenLibraries(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    if (!IntuitionBase) {
        printf("ERROR: Cannot open intuition.library\n");
        return FALSE;
    }

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    if (!GfxBase) {
        printf("ERROR: Cannot open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return FALSE;
    }

    ZuneRendererBase = OpenLibrary("zunerenderer.library", 1);
    if (!ZuneRendererBase) {
        printf("ERROR: Cannot open zunerenderer.library\n");
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return FALSE;
    }

    printf("All libraries opened successfully\n");
    return TRUE;
}

void CloseLibraries(void)
{
    if (ZuneRendererBase) CloseLibrary(ZuneRendererBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

struct ZuneTexture *CreateTestTexture(void)
{
    struct ZuneTexture *texture;

    printf("Creating test texture (%dx%d)...\n", TEXTURE_WIDTH, TEXTURE_HEIGHT);

    /* Create a 32-bit ARGB texture */
    texture = CreateTexture(TEXTURE_WIDTH, TEXTURE_HEIGHT, 32, 
                           0, /* format - use default */
                           0); /* flags - use default */

    if (!texture) {
        printf("ERROR: Failed to create texture\n");
        return NULL;
    }

    if (!IsTextureValid(texture)) {
        printf("ERROR: Created texture is not valid\n");
        DestroyTexture(texture);
        return NULL;
    }

    printf("Texture created successfully\n");

    /* Get and display texture info */
    UWORD width, height;
    UBYTE depth;
    ULONG format;
    
    GetTextureInfo(texture, &width, &height, &depth, &format);
    printf("Texture info: %dx%d, depth=%d, format=0x%08x\n", 
           width, height, depth, format);

    ULONG size = GetTextureSizeInBytes(width, height, format);
    ULONG bpp = GetTextureFormatBPP(format);
    printf("Texture size: %d bytes, BPP: %d\n", size, bpp);

    return texture;
}

void DrawTestPattern(struct ZuneTexture *texture)
{
    WORD x, y;
    ULONG color;

    printf("Drawing test pattern to texture...\n");

    /* Draw a simple gradient pattern */
    for (y = 0; y < TEXTURE_HEIGHT; y++) {
        for (x = 0; x < TEXTURE_WIDTH; x++) {
            /* Create a gradient from red to blue */
            UBYTE red = (255 * x) / TEXTURE_WIDTH;
            UBYTE green = (255 * y) / TEXTURE_HEIGHT;
            UBYTE blue = 255 - red;
            UBYTE alpha = 255;
            
            color = ((ULONG)alpha << 24) | ((ULONG)red << 16) | ((ULONG)green << 8) | blue;
            SetTexturePixel(texture, x, y, color);
        }
    }

    /* Draw a border around the texture */
    for (x = 0; x < TEXTURE_WIDTH; x++) {
        SetTexturePixel(texture, x, 0, 0xFFFFFFFF); /* White */
        SetTexturePixel(texture, x, TEXTURE_HEIGHT-1, 0xFFFFFFFF); /* White */
    }
    for (y = 0; y < TEXTURE_HEIGHT; y++) {
        SetTexturePixel(texture, 0, y, 0xFFFFFFFF); /* White */
        SetTexturePixel(texture, TEXTURE_WIDTH-1, y, 0xFFFFFFFF); /* White */
    }

    /* Draw a cross in the center */
    WORD center_x = TEXTURE_WIDTH / 2;
    WORD center_y = TEXTURE_HEIGHT / 2;
    
    for (x = center_x - 8; x <= center_x + 8; x++) {
        if (x >= 0 && x < TEXTURE_WIDTH) {
            SetTexturePixel(texture, x, center_y, 0xFF000000); /* Black */
        }
    }
    for (y = center_y - 8; y <= center_y + 8; y++) {
        if (y >= 0 && y < TEXTURE_HEIGHT) {
            SetTexturePixel(texture, center_x, y, 0xFF000000); /* Black */
        }
    }

    printf("Test pattern drawn successfully\n");
}

void TestTexturePixelAccess(struct ZuneTexture *texture)
{
    APTR pixels;
    ULONG pitch;

    printf("Testing texture pixel access...\n");

    /* Test direct pixel access */
    pixels = LockTexturePixels(texture, &pitch);
    if (pixels) {
        printf("Pixels locked successfully, pitch=%d\n", pitch);
        
        /* Verify we can't lock again */
        APTR pixels2 = LockTexturePixels(texture, NULL);
        if (!pixels2) {
            printf("Correctly prevented double-locking\n");
        } else {
            printf("WARNING: Double-locking was allowed\n");
        }
        
        UnlockTexturePixels(texture);
        printf("Pixels unlocked\n");
    } else {
        printf("ERROR: Failed to lock texture pixels\n");
    }

    /* Test pixel get/set */
    ULONG test_color = 0xFF8040C0; /* ARGB format */
    SetTexturePixel(texture, 10, 10, test_color);
    ULONG read_color = GetTexturePixel(texture, 10, 10);
    
    if (read_color == test_color) {
        printf("Pixel get/set test PASSED\n");
    } else {
        printf("Pixel get/set test FAILED (wrote 0x%08x, read 0x%08x)\n", 
               test_color, read_color);
    }
}

void TestTextureFromData(void)
{
    ULONG *test_data;
    struct ZuneTexture *texture;
    WORD x, y;

    printf("Testing texture creation from data...\n");

    /* Create test data */
    test_data = AllocVec(32 * 32 * sizeof(ULONG), MEMF_CLEAR);
    if (!test_data) {
        printf("ERROR: Failed to allocate test data\n");
        return;
    }

    /* Fill with a checkerboard pattern */
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            BOOL checker = ((x / 4) + (y / 4)) & 1;
            test_data[y * 32 + x] = checker ? 0xFFFF0000 : 0xFF0000FF; /* Red or Blue */
        }
    }

    /* Create texture from data */
    texture = CreateTextureFromData(test_data, 32, 32, 32, 
                                   0, /* format */
                                   32 * sizeof(ULONG), 0);

    if (texture && IsTextureValid(texture)) {
        printf("Texture from data created successfully\n");
        
        /* Verify the data was copied correctly */
        ULONG pixel = GetTexturePixel(texture, 0, 0);
        ULONG expected = test_data[0];
        if (pixel == expected) {
            printf("Data copy verification PASSED\n");
        } else {
            printf("Data copy verification FAILED\n");
        }
        
        DestroyTexture(texture);
    } else {
        printf("ERROR: Failed to create texture from data\n");
    }

    FreeVec(test_data);
}

void RunTextureTest(void)
{
    struct ZuneTexture *texture;
    struct Screen *screen;
    struct Window *window;
    struct RenderPort *rp;
    BOOL done = FALSE;
    struct IntuiMessage *msg;

    printf("\nStarting texture functionality test...\n\n");

    /* Test basic texture creation */
    texture = CreateTestTexture();
    if (!texture) {
        return;
    }

    /* Draw test pattern */
    DrawTestPattern(texture);

    /* Test pixel access */
    TestTexturePixelAccess(texture);

    /* Test texture from data */
    TestTextureFromData();

    /* Open a screen and window for display testing */
    screen = OpenScreenTags(NULL,
        SA_Width, WINDOW_WIDTH,
        SA_Height, WINDOW_HEIGHT,
        SA_Depth, 8,
        SA_Title, (IPTR)"Zune Renderer Texture Test",
        TAG_DONE);

    if (!screen) {
        printf("ERROR: Cannot open screen\n");
        DestroyTexture(texture);
        return;
    }

    window = OpenWindowTags(NULL,
        WA_CustomScreen, (IPTR)screen,
        WA_Left, 0,
        WA_Top, 0,
        WA_Width, WINDOW_WIDTH,
        WA_Height, WINDOW_HEIGHT,
        WA_Title, (IPTR)"Texture Test",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        TAG_DONE);

    if (!window) {
        printf("ERROR: Cannot open window\n");
        CloseScreen(screen);
        DestroyTexture(texture);
        return;
    }

    /* Create RenderPort for the window */
    rp = CreateRenderPort(screen->ViewPort.ColorMap, window->RPort);
    if (!rp) {
        printf("ERROR: Cannot create RenderPort\n");
        CloseWindow(window);
        CloseScreen(screen);
        DestroyTexture(texture);
        return;
    }

    printf("Display window opened, testing texture rendering...\n");

    /* Clear the background */
    ClearRenderPort(rp, 0xFF404040); /* Dark gray */

    /* Test various texture drawing functions */
    printf("Drawing texture at original size...\n");
    ZuneDrawTexture(rp, texture, 50, 50);

    printf("Drawing texture scaled...\n");
    ZuneDrawTextureScaled(rp, texture, 150, 50, 128, 128);

    printf("Drawing texture region...\n");
    ZuneDrawTextureRegion(rp, texture, 16, 16, 32, 32, 300, 50, 64, 64);

    printf("Drawing texture with tint...\n");
    ZuneDrawTextureTinted(rp, texture, 50, 150, 0x80FF0000); /* Semi-transparent red */

    printf("Drawing texture scaled with tint...\n");
    ZuneDrawTextureScaledTinted(rp, texture, 150, 150, 96, 96, 0x8000FF00); /* Semi-transparent green */

    printf("Drawing texture region with tint...\n");
    ZuneDrawTextureRegionTinted(rp, texture, 8, 8, 48, 48, 280, 150, 80, 80, 0x800000FF); /* Semi-transparent blue */

    printf("\nTexture rendering complete. Close window to exit.\n");

    /* Wait for user to close window */
    while (!done) {
        Wait(1L << window->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            if (msg->Class == IDCMP_CLOSEWINDOW) {
                done = TRUE;
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    /* Cleanup */
    DestroyRenderPort(rp);
    CloseWindow(window);
    CloseScreen(screen);
    DestroyTexture(texture);

    printf("\nTexture test completed successfully!\n");
}