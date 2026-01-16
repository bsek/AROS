/*
 * alpha_window_test.c - Simple test for alpha window compositor
 *
 * This program opens a semi-transparent window to test the layer
 * compositor hook integration in hyperlayers.
 *
 * Build: make workbench-libs-zunegfx-examples
 * Run: Alpha_Window_Test
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>

#include <stdio.h>

/* Debug output */
#define D(x) x

/* Alpha window tags from intuition.h */
#ifndef WA_Alpha
#define WA_Alpha        (WA_Dummy + 110)
#endif
#ifndef WA_AlphaValue
#define WA_AlphaValue   (WA_Dummy + 111)
#endif
#ifndef WA_NoShadow
#define WA_NoShadow     (WA_Dummy + 112)
#endif

/* Layer flags to check */
#ifndef LAYERF_ALPHA
#define LAYERF_ALPHA    (1 << 16)
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *LayersBase;

/*
 * Draw a simple pattern in the window to visualize transparency
 */
void DrawTestPattern(struct Window *win)
{
    struct RastPort *rp = win->RPort;
    int x, y;
    int w = win->Width - win->BorderLeft - win->BorderRight;
    int h = win->Height - win->BorderTop - win->BorderBottom;
    int bx = win->BorderLeft;
    int by = win->BorderTop;
    
    /* Fill with semi-transparent looking pattern */
    SetAPen(rp, 1);  /* Blue-ish */
    RectFill(rp, bx, by, bx + w - 1, by + h - 1);
    
    /* Draw a grid pattern */
    SetAPen(rp, 2);  /* White */
    for (y = 0; y < h; y += 20)
    {
        Move(rp, bx, by + y);
        Draw(rp, bx + w - 1, by + y);
    }
    for (x = 0; x < w; x += 20)
    {
        Move(rp, bx + x, by);
        Draw(rp, bx + x, by + h - 1);
    }
    
    /* Draw text */
    SetAPen(rp, 1);
    SetBPen(rp, 0);
    Move(rp, bx + 10, by + 30);
    Text(rp, "Alpha Window Test", 17);
    
    Move(rp, bx + 10, by + 50);
    Text(rp, "You should see through this!", 28);
}

/*
 * Check and print layer info
 */
void PrintLayerInfo(struct Window *win)
{
    struct Layer *layer = win->WLayer;
    
    printf("Window: %p\n", (void *)win);
    printf("Layer:  %p\n", (void *)layer);
    
    if (layer)
    {
        printf("  Layer->Flags: 0x%08lx\n", (unsigned long)layer->Flags);
        printf("  LAYERF_ALPHA set: %s\n", 
               (layer->Flags & LAYERF_ALPHA) ? "YES" : "NO");
        
        if (win->WScreen)
        {
            struct Layer_Info *li = &win->WScreen->LayerInfo;
            printf("  LayerInfo: %p\n", (void *)li);
            printf("  LayerInfo_extra: %p\n", (void *)li->LayerInfo_extra);
        }
    }
}

int main(int argc, char **argv)
{
    struct Window *normalWin = NULL;
    struct Window *alphaWin = NULL;
    struct Screen *screen = NULL;
    UBYTE alphaValue = 180;  /* Semi-transparent (0=invisible, 255=opaque) */
    BOOL running = TRUE;
    
    printf("=== Alpha Window Test ===\n\n");
    
    /* Open libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    if (!IntuitionBase)
    {
        printf("ERROR: Cannot open intuition.library\n");
        return 1;
    }
    
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!GfxBase)
    {
        printf("ERROR: Cannot open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    
    LayersBase = OpenLibrary("layers.library", 39);
    if (!LayersBase)
    {
        printf("ERROR: Cannot open layers.library\n");
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    
    /* Lock the default public screen */
    screen = LockPubScreen(NULL);
    if (!screen)
    {
        printf("ERROR: Cannot lock public screen\n");
        goto cleanup;
    }
    
    printf("Screen: %p\n", (void *)screen);
    printf("Screen LayerInfo: %p\n", (void *)&screen->LayerInfo);
    printf("Screen LayerInfo_extra: %p\n", (void *)screen->LayerInfo.LayerInfo_extra);
    printf("\n");
    
    /* First, open a normal background window */
    printf("Opening normal (opaque) window...\n");
    normalWin = OpenWindowTags(NULL,
        WA_Left, 100,
        WA_Top, 100,
        WA_Width, 300,
        WA_Height, 200,
        WA_Title, (IPTR)"Normal Window (Background)",
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | 
                  WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_GIMMEZEROZERO,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
        WA_PubScreen, (IPTR)screen,
        TAG_DONE);
    
    if (!normalWin)
    {
        printf("ERROR: Cannot open normal window\n");
        goto cleanup;
    }
    
    /* Draw something in the background window */
    SetAPen(normalWin->RPort, 3);  /* Orange/red */
    RectFill(normalWin->RPort, 
             normalWin->BorderLeft, normalWin->BorderTop,
             normalWin->Width - normalWin->BorderRight - 1,
             normalWin->Height - normalWin->BorderBottom - 1);
    SetAPen(normalWin->RPort, 1);
    Move(normalWin->RPort, normalWin->BorderLeft + 10, normalWin->BorderTop + 30);
    Text(normalWin->RPort, "Background content here", 23);
    
    printf("Normal window info:\n");
    PrintLayerInfo(normalWin);
    printf("\n");
    
    /* Now open the alpha window on top */
    printf("Opening alpha window (alpha=%d)...\n", alphaValue);
    alphaWin = OpenWindowTags(NULL,
        WA_Left, 150,
        WA_Top, 150,
        WA_Width, 280,
        WA_Height, 180,
        WA_Title, (IPTR)"Alpha Window (Should be transparent)",
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | 
                  WFLG_SIZEGADGET | WFLG_GIMMEZEROZERO,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY,
        WA_PubScreen, (IPTR)screen,
        WA_Alpha, TRUE,
        WA_AlphaValue, alphaValue,
        TAG_DONE);
    
    if (!alphaWin)
    {
        printf("WARNING: Cannot open alpha window with WA_Alpha\n");
        printf("Trying without alpha tags...\n");
        
        alphaWin = OpenWindowTags(NULL,
            WA_Left, 150,
            WA_Top, 150,
            WA_Width, 280,
            WA_Height, 180,
            WA_Title, (IPTR)"Window (Alpha FAILED)",
            WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | 
                      WFLG_SIZEGADGET | WFLG_GIMMEZEROZERO,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY,
            WA_PubScreen, (IPTR)screen,
            TAG_DONE);
    }
    
    if (!alphaWin)
    {
        printf("ERROR: Cannot open window at all\n");
        goto cleanup;
    }
    
    printf("Alpha window info:\n");
    PrintLayerInfo(alphaWin);
    printf("\n");
    
    /* Draw test pattern */
    DrawTestPattern(alphaWin);
    
    /* Instructions */
    printf("=== Test Instructions ===\n");
    printf("1. The alpha window should overlap the normal window\n");
    printf("2. If compositor works: you'll see the background through the alpha window\n");
    printf("3. If compositor doesn't work: alpha window will be opaque\n");
    printf("\n");
    printf("Press 'Q' in alpha window or close it to exit\n");
    printf("Press '+'/'-' to change alpha value\n");
    printf("\n");
    
    /* Event loop */
    while (running)
    {
        struct IntuiMessage *imsg;
        ULONG signals;
        ULONG winSig = (1L << alphaWin->UserPort->mp_SigBit);
        ULONG normSig = normalWin ? (1L << normalWin->UserPort->mp_SigBit) : 0;
        
        signals = Wait(winSig | normSig | SIGBREAKF_CTRL_C);
        
        if (signals & SIGBREAKF_CTRL_C)
        {
            printf("Break signal received\n");
            running = FALSE;
            break;
        }
        
        /* Handle alpha window messages */
        while ((imsg = (struct IntuiMessage *)GetMsg(alphaWin->UserPort)))
        {
            ULONG class = imsg->Class;
            UWORD code = imsg->Code;
            
            ReplyMsg((struct Message *)imsg);
            
            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    running = FALSE;
                    break;
                    
                case IDCMP_VANILLAKEY:
                    if (code == 'q' || code == 'Q')
                    {
                        running = FALSE;
                    }
                    else if (code == '+' || code == '=')
                    {
                        if (alphaValue < 245) alphaValue += 10;
                        else alphaValue = 255;
                        printf("Alpha: %d (Note: changing alpha at runtime not implemented)\n", alphaValue);
                    }
                    else if (code == '-')
                    {
                        if (alphaValue > 10) alphaValue -= 10;
                        else alphaValue = 0;
                        printf("Alpha: %d (Note: changing alpha at runtime not implemented)\n", alphaValue);
                    }
                    break;
                    
                case IDCMP_REFRESHWINDOW:
                    BeginRefresh(alphaWin);
                    DrawTestPattern(alphaWin);
                    EndRefresh(alphaWin, TRUE);
                    break;
            }
        }
        
        /* Handle normal window messages */
        if (normalWin)
        {
            while ((imsg = (struct IntuiMessage *)GetMsg(normalWin->UserPort)))
            {
                ULONG class = imsg->Class;
                ReplyMsg((struct Message *)imsg);
                
                if (class == IDCMP_CLOSEWINDOW)
                {
                    CloseWindow(normalWin);
                    normalWin = NULL;
                }
            }
        }
    }
    
    printf("\nExiting...\n");

cleanup:
    if (alphaWin)
        CloseWindow(alphaWin);
    if (normalWin)
        CloseWindow(normalWin);
    if (screen)
        UnlockPubScreen(NULL, screen);
    
    if (LayersBase)
        CloseLibrary(LayersBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    
    return 0;
}
