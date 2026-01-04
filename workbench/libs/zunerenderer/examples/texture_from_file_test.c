/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Texture From File Test

    This example demonstrates loading an image file via CreateTextureFromFile()
    and rendering it using ZuneDrawTexture(). It serves as a test case for
    verifying that images without alpha channels render correctly (with proper
    alpha fixup to 0xFF).

    Usage: texture_from_file_test [imagefile]
    Default image: PROGDIR:testimage.png
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/zunerenderer.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <libraries/zunerenderer.h>
#include <intuition/screens.h>
#include <intuition/intuition.h>
#include <string.h>
#include <stdio.h>

#define DEBUG 1
#include <aros/debug.h>

struct Library *ZuneRendererBase = NULL;

struct Screen *pub_screen = NULL;
struct Window *test_window = NULL;
struct RenderPort *render_port = NULL;

#define DEFAULT_IMAGE "PROGDIR:testimage.png"
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* Function prototypes */
static BOOL OpenLibraries(void);
static void CloseLibraries(void);
static BOOL OpenTestWindow(void);
static void CloseTestWindow(void);
static void RunTextureFromFileTest(const char *filename);
static void WaitForClose(void);

int main(int argc, char **argv)
{
    const char *image_file = DEFAULT_IMAGE;
    
    printf("Zune Renderer - Texture From File Test\n");
    printf("======================================\n\n");
    
    /* Use command line argument if provided */
    if (argc > 1) {
        image_file = argv[1];
    }
    
    printf("Image file: %s\n\n", image_file);

    if (!OpenLibraries()) {
        printf("ERROR: Failed to open required libraries\n");
        CloseLibraries();
        return 20;
    }

    if (!OpenTestWindow()) {
        printf("ERROR: Failed to open test window\n");
        CloseLibraries();
        return 20;
    }

    RunTextureFromFileTest(image_file);

    printf("\nTest completed. Closing...\n");
    CloseTestWindow();
    CloseLibraries();
    return 0;
}

static BOOL OpenLibraries(void)
{
    ZuneRendererBase = OpenLibrary("zunerenderer.library", 1);
    if (!ZuneRendererBase) {
        printf("ERROR: Cannot open zunerenderer.library v1\n");
        return FALSE;
    }

    printf("zunerenderer.library opened successfully\n");
    return TRUE;
}

static void CloseLibraries(void)
{
    if (ZuneRendererBase) {
        CloseLibrary(ZuneRendererBase);
        ZuneRendererBase = NULL;
    }
}

static BOOL OpenTestWindow(void)
{
    /* Lock the default public screen */
    pub_screen = LockPubScreen(NULL);
    if (!pub_screen) {
        printf("ERROR: Failed to lock public screen\n");
        return FALSE;
    }

    /* Open a window on the public screen */
    test_window = OpenWindowTags(NULL,
        WA_Width, WINDOW_WIDTH,
        WA_Height, WINDOW_HEIGHT,
        WA_Title, (IPTR)"Texture From File Test",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY,
        WA_PubScreen, (IPTR)pub_screen,
        TAG_END);

    if (!test_window) {
        printf("ERROR: Failed to open window\n");
        UnlockPubScreen(NULL, pub_screen);
        pub_screen = NULL;
        return FALSE;
    }

    /* Create RenderPort for the window (new API) */
    render_port = CreateRenderPortForWindow(test_window, pub_screen->ViewPort.ColorMap);
    if (!render_port) {
        printf("ERROR: Failed to create RenderPort\n");
        CloseWindow(test_window);
        test_window = NULL;
        UnlockPubScreen(NULL, pub_screen);
        pub_screen = NULL;
        return FALSE;
    }

    printf("Window opened: %dx%d\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("RenderPort created successfully\n");
    return TRUE;
}

static void CloseTestWindow(void)
{
    if (render_port) {
        DestroyRenderPort(render_port);
        render_port = NULL;
    }
    if (test_window) {
        CloseWindow(test_window);
        test_window = NULL;
    }
    if (pub_screen) {
        UnlockPubScreen(NULL, pub_screen);
        pub_screen = NULL;
    }
}

static void WaitForClose(void)
{
    struct IntuiMessage *msg;
    BOOL done = FALSE;

    printf("Press any key or close window to exit...\n");

    while (!done) {
        WaitPort(test_window->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(test_window->UserPort))) {
            ULONG class = msg->Class;
            ReplyMsg((struct Message *)msg);

            switch (class) {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;
                case IDCMP_VANILLAKEY:
                    done = TRUE;
                    break;
            }
        }
    }
}

static void RunTextureFromFileTest(const char *filename)
{
    struct ZuneTexture *texture = NULL;
    WORD x_pos, y_pos;
    char info_text[256];

    printf("\n=== Loading Texture From File ===\n");

    /* Clear window background */
    SetRast(test_window->RPort, 0);

    /* Load texture from file */
    printf("Loading: %s\n", filename);
    texture = CreateTextureFromFile(filename, pub_screen, 
                                    ZUNE_TEXTURE_WRAPPING | ZUNE_TEXTURE_ALPHA);

    if (!texture) {
        printf("ERROR: Failed to load texture from '%s'\n", filename);
        
        /* Display error message in window */
        SetAPen(test_window->RPort, 1);
        Move(test_window->RPort, 20, 50);
        snprintf(info_text, sizeof(info_text), "ERROR: Failed to load '%s'", filename);
        Text(test_window->RPort, info_text, strlen(info_text));
        
        Move(test_window->RPort, 20, 70);
        Text(test_window->RPort, "Make sure the file exists and is a valid image.", 47);
        
        WaitForClose();
        return;
    }

    D(bug("TextureTest: Texture loaded successfully!\n"));
    D(bug("TextureTest:   Dimensions: %d x %d\n", texture->width, texture->height));
    D(bug("TextureTest:   Format: 0x%08lx\n", (unsigned long)texture->format));
    D(bug("TextureTest:   Flags: 0x%08lx\n", (unsigned long)texture->flags));
    D(bug("TextureTest:   Has Alpha: %s\n", (texture->flags & ZUNE_TEXTURE_ALPHA) ? "Yes" : "No"));
    D(bug("TextureTest:   Pitch: %lu\n", (unsigned long)texture->pitch));
    D(bug("TextureTest:   pixel_data: %p\n", texture->pixel_data));

    /* Dump first pixels for debugging */
    if (texture->pixel_data) {
        ULONG *pixels = (ULONG *)texture->pixel_data;
        UBYTE *bytes = (UBYTE *)texture->pixel_data;
        int num_pixels = (texture->width * texture->height);
        if (num_pixels > 16) num_pixels = 16;
        
        D(bug("TextureTest: First %d pixels as ULONG (hex):\n", num_pixels));
        for (int i = 0; i < num_pixels; i++) {
            D(bug("  [%d] = 0x%08lx\n", i, (unsigned long)pixels[i]));
        }
        
        D(bug("TextureTest: First 64 bytes (hex):\n"));
        for (int i = 0; i < 64 && i < (int)texture->data_size; i += 16) {
            D(bug("  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                bytes[i], bytes[i+1], bytes[i+2], bytes[i+3],
                bytes[i+4], bytes[i+5], bytes[i+6], bytes[i+7],
                bytes[i+8], bytes[i+9], bytes[i+10], bytes[i+11],
                bytes[i+12], bytes[i+13], bytes[i+14], bytes[i+15]));
        }
    }

    /* Validate texture */
    if (!ZuneIsTextureValid(texture)) {
        printf("WARNING: Texture validation failed!\n");
    }

    /* Calculate centered position */
    x_pos = (WINDOW_WIDTH - texture->width) / 2;
    y_pos = (WINDOW_HEIGHT - texture->height) / 2;
    
    /* Clamp position to window bounds */
    if (x_pos < 10) x_pos = 10;
    if (y_pos < 30) y_pos = 30;

    printf("\nDrawing texture at position (%d, %d)...\n", x_pos, y_pos);

    /* Draw the texture */
    struct ZunePoint position = { x_pos, y_pos };
    ZuneDrawTexture(render_port, texture, &position);

    /* Draw info text */
    SetAPen(test_window->RPort, 1);
    
    Move(test_window->RPort, 10, test_window->Height - 60);
    snprintf(info_text, sizeof(info_text), "File: %s", filename);
    Text(test_window->RPort, info_text, strlen(info_text));

    Move(test_window->RPort, 10, test_window->Height - 45);
    snprintf(info_text, sizeof(info_text), "Size: %d x %d  Alpha: %s", 
             texture->width, texture->height,
             (texture->flags & ZUNE_TEXTURE_ALPHA) ? "Yes" : "No");
    Text(test_window->RPort, info_text, strlen(info_text));

    Move(test_window->RPort, 10, test_window->Height - 30);
    Text(test_window->RPort, "Press any key or close window to exit", 37);

    printf("Texture displayed. Waiting for user...\n");

    WaitForClose();

    /* Cleanup */
    printf("Destroying texture...\n");
    DestroyTexture(texture);
    printf("Texture destroyed.\n");
}
