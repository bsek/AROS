/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Visual Texture Test

    This example demonstrates texture functionality with visual display.
    It opens a screen, creates textures, and displays them visually.
*/

#include "clib/exec_protos.h"
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
#include <graphics/displayinfo.h>
#include <graphics/regions.h>
#include <proto/layers.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *ZuneRendererBase = NULL;

struct Screen *test_screen = NULL;
struct Window *test_window = NULL;
struct RenderPort *render_port = NULL;
struct RenderPort *screen_port = NULL;

/* Function prototypes */
BOOL OpenLibraries(void);
BOOL OpenPublicWindow();
void CloseLibraries(void);
void RunVisualTextureTest(void);
void DisplayTexture(struct ZuneTexture *texture, WORD x, WORD y, const char *label);
void WaitForUser(void);
struct ZuneTexture *CreateTestPattern(UWORD width, UWORD height, ULONG pattern_type);
void SetupClippingTest(void);
void ClearClippingTest(void);

int main(void)
{
    printf("Zune Renderer Visual Texture Test\n");
    printf("=================================\n\n");

    if (!OpenLibraries()) {
        printf("ERROR: Failed to open required libraries\n");
        CloseLibraries();
        return 20;
    }

    OpenPublicWindow();

    printf("Press CTRL+C to exit at any time\n\n");

    RunVisualTextureTest();

    printf("\nClosing display...\n");
    CloseLibraries();
    printf("Test completed!\n");
    return 0;
}

BOOL OpenLibraries(void)
{

    ZuneRendererBase = OpenLibrary("zunerenderer.library", 1);
    if (!ZuneRendererBase) {
        printf("ERROR: Cannot open zunerenderer.library\n");
        return FALSE;
    }

    printf("All libraries opened successfully\n");
    return TRUE;
}

void CloseLibraries(void)
{
    if (ZuneRendererBase) {
        CloseLibrary(ZuneRendererBase);
        ZuneRendererBase = NULL;
    }
}

BOOL OpenPublicWindow(void)
{
    struct Screen *test_screen = LockPubScreen(NULL);
    if (!test_screen) {
        printf("ERROR: Failed to open screen\n");
        return FALSE;
    }

    struct TagItem window_tags[] = {
        {WA_Width, 640},
        {WA_Height, 480},
        {WA_Title, (IPTR)"Texture Display"},
        {WA_DragBar, TRUE},
        {WA_CloseGadget, TRUE},
        {WA_IDCMP, IDCMP_CLOSEWINDOW},
        {WA_Flags, WFLG_ACTIVATE },
        {WA_CustomScreen, (IPTR)test_screen},
        {TAG_END, 0}
    };

    test_window = OpenWindowTagList(NULL, window_tags);
    if (!test_window) {
        printf("ERROR: Failed to open window\n");
        return FALSE;
    }

    render_port = CreateRenderPort(test_screen->ViewPort.ColorMap, test_window->RPort);
   // struct DrawingBoard *board = CreateDrawingBoard(640, 480, 32, ZUNE_DRAWINGBOARD_HARDWARE | ZUNE_DRAWINGBOARD_CACHED);

    /* Create RenderPort for the window */
    //render_port = CreateRenderPortWithDrawingBoard(test_screen->ViewPort.ColorMap, board);
    if (!render_port) {
        printf("ERROR: Failed to create RenderPort\n");
        return FALSE;
    }

    printf("Screen opened: 640x480x8\n");
    printf("Window and RenderPort created\n");
    return TRUE;
}

void ClosePublicWindow(void)
{
    if (render_port) {
        DestroyRenderPort(render_port);
        render_port = NULL;
    }
    if (test_window) {
        CloseWindow(test_window);
        test_window = NULL;
    }
    if (test_screen) {
        CloseScreen(test_screen);
        test_screen = NULL;
    }
}

struct ZuneTexture *CreateTestPattern(UWORD width, UWORD height, ULONG pattern_type)
{
    struct ZuneTexture *texture;
    ULONG *pixel_data;
    WORD x, y;

    /* Create texture */
    texture = CreateTexture(width, height, 32, ZUNE_TEXTURE_FORMAT_ARGB32, 0);
    if (!texture) {
        return NULL;
    }

    /* Lock pixels for direct access */
    ULONG pitch;
    pixel_data = (ULONG *)LockTexturePixels(texture, &pitch);
    if (!pixel_data) {
        DestroyTexture(texture);
        return NULL;
    }

    /* Generate different patterns based on type */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            ULONG pixel = 0xFF000000; /* Full alpha */

            switch (pattern_type) {
                case 0: /* Gradient */
                    pixel |= ((x * 255 / width) << 16);        /* Red gradient */
                    pixel |= ((y * 255 / height) << 8);        /* Green gradient */
                    pixel |= (128);                             /* Blue constant */
                    break;

                case 1: /* Checkerboard */
                    if (((x / 8) + (y / 8)) % 2) {
                        pixel |= 0x00FFFFFF; /* White */
                    } else {
                        pixel |= 0x00000000; /* Black */
                    }
                    break;

                case 2: /* Color rings */
                {
                    WORD dx = x - width/2;
                    WORD dy = y - height/2;
                    UWORD dist = (UWORD)sqrt(dx*dx + dy*dy);
                    UBYTE color = (dist * 8) % 256;
                    pixel |= (color << 16) | (color << 8) | color;
                    break;
                }

                case 3: /* RGB stripes */
                    if (x < width/3) {
                        pixel |= 0x00FF0000; /* Red */
                    } else if (x < 2*width/3) {
                        pixel |= 0x0000FF00; /* Green */
                    } else {
                        pixel |= 0x000000FF; /* Blue */
                    }
                    break;

                default: /* Solid color */
                    pixel |= 0x00808080; /* Gray */
                    break;
            }

            pixel_data[y * (pitch/4) + x] = pixel;
        }
    }

    UnlockTexturePixels(texture);
    return texture;
}

void DisplayTexture(struct ZuneTexture *texture, WORD x, WORD y, const char *label)
{
    if (!texture || !render_port) return;

    /* Draw texture at specified position */
    struct ZunePoint position = {x, y};
    ZuneDrawTexture(render_port, texture, &position);

    //BlitDrawingBoardToRenderPortRects(render_port->target_board, screen_port, x, y, x, y, texture->width, texture->height);

    /* Draw label below texture if provided */
    if (label) {
        SetAPen(test_window->RPort, 1); /* White pen */
        Move(test_window->RPort, x, y + texture->height + 15);
        Text(test_window->RPort, label, strlen(label));
    }
}

void WaitForUser(void)
{
    printf("Display updated. Press Enter to continue...\n");
    getchar();
}

void RunVisualTextureTest(void)
{
    struct ZuneTexture *textures[6];
    const char *labels[] = {
        "Gradient", "Checkerboard", "Rings",
        "RGB Stripes", "Small Test", "From Data"
    };
    WORD positions[6][2] = {
        {50, 50},   {200, 50},  {350, 50},
        {50, 200},  {200, 200}, {350, 200}
    };

    printf("\n=== Visual Texture Display Test ===\n");

    /* Clear screen to black */
    SetRast(test_window->RPort, 0);

    printf("\n1. Creating test pattern textures...\n");

    /* Create different pattern textures */
    for (int i = 0; i < 4; i++) {
        textures[i] = CreateTestPattern(64, 64, i);
        if (textures[i]) {
            printf("   Created %s pattern (64x64)\n", labels[i]);
        } else {
            printf("   FAILED to create %s pattern\n", labels[i]);
            textures[i] = NULL;
        }
    }

    /* Create a small texture for testing scaling */
    printf("\n2. Creating small texture...\n");
    textures[4] = CreateTexture(16, 16, 32, ZUNE_TEXTURE_FORMAT_ARGB32, 0);
    if (textures[4]) {
        /* Fill with orange */
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                struct ZunePoint point = {x, y};
                SetTexturePixel(textures[4], &point, 0xFFFF8000); /* Orange */
            }
        }
        printf("   Created small 16x16 orange texture\n");
    }

    /* Create texture from data */
    printf("\n3. Creating texture from data...\n");
    ULONG test_data[32*32];
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            /* Create a simple pattern */
            ULONG r = (x * 8) % 256;
            ULONG g = (y * 8) % 256;
            ULONG b = ((x + y) * 4) % 256;
            test_data[y * 32 + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    textures[5] = CreateTextureFromData(test_data, 32, 32, 32,
                                       ZUNE_TEXTURE_FORMAT_ARGB32,
                                       32 * sizeof(ULONG), 0);
    if (textures[5]) {
        printf("   Created 32x32 texture from data\n");
    }

    printf("\n4. Setting up clipping region test...\n");
    SetupClippingTest();
    
    printf("\n5. Displaying textures on screen (with clipping)...\n");

    //LockDrawingBoardPixels(render_port, NULL);

    /* Display all textures */
    for (int i = 0; i < 6; i++) {
        if (textures[i]) {
            DisplayTexture(textures[i], positions[i][0], positions[i][1], labels[i]);
            printf("   Displayed: %s\n", labels[i]);
        }
    }

    //UnlockDrawingBoardPixels(render_port);


    /* Add some text info */
    SetAPen(test_window->RPort, 1);
    Move(test_window->RPort, 50, 400);
    Text(test_window->RPort, "Zune Renderer Texture Test - Various patterns with CLIPPING", 59);

    Move(test_window->RPort, 50, 420);
    Text(test_window->RPort, "Notice textures are clipped to circular region!", 47);

    Move(test_window->RPort, 50, 440);
    Text(test_window->RPort, "Press Enter to continue with pixel tests...", 44);

    WaitForUser();
    
    /* Clear clipping for next test */
    ClearClippingTest();

    printf("\n6. Testing pixel manipulation (without clipping)...\n");

    /* Test pixel operations on displayed texture */
    if (textures[0]) { /* Use gradient texture */
        printf("   Modifying gradient texture pixels...\n");

        /* Draw a cross pattern */
        for (int i = 10; i < 54; i++) {
            struct ZunePoint h_point = {i, 32}; /* Horizontal line */
            struct ZunePoint v_point = {32, i}; /* Vertical line */
            SetTexturePixel(textures[0], &h_point, 0xFFFFFF00); /* Yellow */
            SetTexturePixel(textures[0], &v_point, 0xFFFFFF00); /* Yellow */
        }

        /* Redraw the modified texture */
        DisplayTexture(textures[0], positions[0][0], positions[0][1], "Modified Gradient");
        printf("   Added yellow cross to gradient texture\n");
    }

    Move(test_window->RPort, 50, 440);
    Text(test_window->RPort, "Gradient texture modified with yellow cross", 43);

    WaitForUser();

    printf("\n7. Testing texture validation...\n");

    /* Test all textures for validity */
    for (int i = 0; i < 6; i++) {
        if (textures[i]) {
            BOOL valid = ZuneIsTextureValid(textures[i]);
            printf("   %s texture: %s\n", labels[i], valid ? "VALID" : "INVALID");
        }
    }

    printf("\n8. Display complete. Final wait...\n");
    Move(test_window->RPort, 50, 460);
    Text(test_window->RPort, "All tests complete. Press Enter to exit.", 41);

    WaitForUser();

    /* Cleanup textures */
    printf("\n9. Cleaning up textures...\n");
    for (int i = 0; i < 6; i++) {
        if (textures[i]) {
            DestroyTexture(textures[i]);
            printf("   Destroyed %s texture\n", labels[i]);
        }
    }

    printf("\nVisual texture test completed successfully!\n");
}

void SetupClippingTest(void) {
    if (!test_window || !test_window->RPort) return;
    
    /* Create a circular clipping region in the center of the display area */
    struct Region *clip_region = NewRegion();
    if (clip_region) {
        struct Rectangle rect;
        
        /* Create a circular clipping region centered around texture area */
        WORD center_x = 200;
        WORD center_y = 150;
        WORD radius = 120;
        
        /* Approximate circle with rectangles (simple approach) */
        for (WORD y = -radius; y <= radius; y++) {
            WORD x_width = (WORD)sqrt(radius * radius - y * y);
            if (x_width > 0) {
                rect.MinX = center_x - x_width;
                rect.MaxX = center_x + x_width;
                rect.MinY = center_y + y;
                rect.MaxY = center_y + y;
                OrRectRegion(clip_region, &rect);
            }
        }
        
        /* Install the clipping region */
        InstallClipRegion(test_window->RPort->Layer, clip_region);
        
        printf("   Circular clipping region installed (center=%d,%d radius=%d)\n", 
               center_x, center_y, radius);
    }
}

void ClearClippingTest(void) {
    if (!test_window || !test_window->RPort) return;
    
    /* Remove clipping region */
    struct Region *old_region = InstallClipRegion(test_window->RPort->Layer, NULL);
    if (old_region) {
        DisposeRegion(old_region);
        printf("   Clipping region removed\n");
    }
}
