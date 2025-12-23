/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Implementation

    This file implements the OpenGL rendering backend for ZuneRenderer.
    It uses AROS's gl.library (mesa3dgl.library or hostgl.library) to
    provide hardware-accelerated 2D rendering.

    The backend can work with:
    - mesa3dgl.library: Software Mesa3D implementation (native AROS)
    - hostgl.library: Hardware-accelerated passthrough (hosted AROS on X11)

    Context Management:
    - Each window/RenderPort gets its own GL context
    - Contexts are created on-demand when OpenGL rendering is first used
    - The backend manages context switching transparently
*/

#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/* GL includes - use proto for library-based access */
#include <GL/gla.h>

#define DEBUG 0
#include <aros/debug.h>

#include "../backend_interface.h"
#include "opengl_backend.h"

/*****************************************************************************/
/* Global GL Library Base                                                    */
/*****************************************************************************/

/*
 * GLBase is defined in zunerenderer_init.c and declared in zunerenderer_intern.h.
 * It is opened in DetectLibraries() in zunerenderer_core.c.
 * The proto/GL.h stubs use this global.
 */
extern struct Library *GLBase;

/*****************************************************************************/
/* Forward Declarations                                                      */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx);
static void OpenGLCleanupBackend(ZuneBackendContext *ctx);
static BOOL OpenGLIsAvailable(void);
static BOOL OpenGLIsCompatible(struct RenderPort *rp);
static ULONG OpenGLGetCapabilities(void);

static BOOL OpenGLInitRenderPort(struct RenderPort *rp);
static void OpenGLCleanupRenderPort(struct RenderPort *rp);

static BOOL OpenGLPrepareColor(struct RenderPort *rp,
                               struct InternalColor *color);
static void OpenGLReleaseColor(struct RenderPort *rp,
                               struct InternalColor *color);

static void OpenGLDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias);
static void OpenGLDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias);

static void OpenGLClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color);

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out);
static void OpenGLUnlockPixels(struct DrawingBoard *board);
static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y);
static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color);

static void OpenGLBeginBatch(struct RenderPort *rp);
static void OpenGLEndBatch(struct RenderPort *rp);
static void OpenGLFlushBatch(struct RenderPort *rp);
static BOOL OpenGLIsBatching(struct RenderPort *rp);

static void OpenGLBlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height);
static void OpenGLBlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height);

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board);
static void OpenGLCleanupDrawingBoard(struct DrawingBoard *board);

/*****************************************************************************/
/* Backend Operations Table                                                  */
/*****************************************************************************/

ZuneBackendOps opengl_backend_ops = {
    .name = OPENGL_BACKEND_NAME,
    .type = BACKEND_OPENGL,
    .GetCapabilities = OpenGLGetCapabilities,

    .InitBackend = OpenGLInitBackend,
    .CleanupBackend = OpenGLCleanupBackend,
    .IsAvailable = OpenGLIsAvailable,
    .IsCompatible = OpenGLIsCompatible,

    .InitRenderPort = OpenGLInitRenderPort,
    .CleanupRenderPort = OpenGLCleanupRenderPort,

    .PrepareColor = OpenGLPrepareColor,
    .ReleaseColor = OpenGLReleaseColor,

    .DrawPixel = OpenGLDrawPixel,
    .DrawLine = OpenGLDrawLine,
    /* DrawRectangle and DrawCircle will be added later */

    .ClearRenderPort = OpenGLClearRenderPort,

    .LockPixels = OpenGLLockPixels,
    .UnlockPixels = OpenGLUnlockPixels,
    .GetPixel = OpenGLGetPixel,
    .SetPixel = OpenGLSetPixel,

    .BeginBatch = OpenGLBeginBatch,
    .EndBatch = OpenGLEndBatch,
    .FlushBatch = OpenGLFlushBatch,
    .IsBatching = OpenGLIsBatching,

    .BlitRenderPorts = OpenGLBlitRenderPorts,
    .BlitToScreen = OpenGLBlitToScreen,

    .InitDrawingBoard = OpenGLInitDrawingBoard,
    .CleanupDrawingBoard = OpenGLCleanupDrawingBoard,

    .reserved = {NULL, NULL}
};

/*****************************************************************************/
/* Library Management                                                        */
/*****************************************************************************/

/*
 * OpenGL_CheckLibrary - Check if GL library is available
 *
 * The gl.library is opened centrally in DetectLibraries() (zunerenderer_core.c).
 * This function just checks if it's available and stores a reference.
 *
 * Returns TRUE if library is available.
 */
BOOL OpenGL_CheckLibrary(OpenGLPrivateData *priv)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: Checking gl.library\n"));
    
    if (!priv) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: NULL priv pointer\n"));
        return FALSE;
    }
    
    /* GLBase is opened in DetectLibraries() */
    if (GLBase) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: gl.library v%ld.%ld available\n",
              GLBase->lib_Version, GLBase->lib_Revision));
        
        priv->GLBase = GLBase;
        priv->gl_available = TRUE;
        
        return TRUE;
    }
    
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: gl.library not available\n"));
    
    priv->gl_available = FALSE;
    return FALSE;
}

/*
 * OpenGL_CheckCapabilities - Query GL capabilities
 *
 * Must be called after a GL context is current!
 * Queries the GL implementation for its capabilities.
 */
BOOL OpenGL_CheckCapabilities(OpenGLPrivateData *priv)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckCapabilities\n"));
    
    if (!priv || !priv->gl_available) {
        return FALSE;
    }
    
    /* 
     * Note: We can't query GL capabilities until we have a valid context.
     * This function should be called after the first context is created.
     * For now, we set reasonable defaults.
     */
    
    priv->gl_version_major = 1;
    priv->gl_version_minor = 1;
    priv->max_texture_size = 1024;      /* Conservative default */
    priv->has_npot_textures = FALSE;    /* Assume no NPOT support */
    priv->has_framebuffers = FALSE;     /* Will check when context available */
    priv->has_shaders = FALSE;          /* Will check when context available */
    
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckCapabilities: Defaults set\n"));
    D(bug("[ZuneRenderer:OpenGL]   Version: %ld.%ld\n", 
          priv->gl_version_major, priv->gl_version_minor));
    D(bug("[ZuneRenderer:OpenGL]   Max texture size: %ld\n", 
          priv->max_texture_size));
    
    return TRUE;
}

/*****************************************************************************/
/* Debug/Info Functions                                                      */
/*****************************************************************************/

void OpenGL_DumpDebugInfo(OpenGLPrivateData *priv)
{
    D(bug("=== OpenGL Backend Debug Info ===\n"));
    
    if (!priv) {
        D(bug("  Status: Not initialized (NULL priv)\n"));
        D(bug("=== End OpenGL Debug Info ===\n"));
        return;
    }
    
    D(bug("  Status: %s\n", priv->initialized ? "Initialized" : "Not initialized"));
    D(bug("  GL Available: %s\n", priv->gl_available ? "Yes" : "No"));
    
    if (priv->GLBase) {
        D(bug("  gl.library: v%ld.%ld\n", 
              priv->GLBase->lib_Version, priv->GLBase->lib_Revision));
    } else {
        D(bug("  gl.library: Not open\n"));
    }
    
    D(bug("  GL Version: %ld.%ld\n", 
          priv->gl_version_major, priv->gl_version_minor));
    D(bug("  Max Texture Size: %ld\n", priv->max_texture_size));
    D(bug("  NPOT Textures: %s\n", priv->has_npot_textures ? "Yes" : "No"));
    D(bug("  Framebuffers: %s\n", priv->has_framebuffers ? "Yes" : "No"));
    D(bug("  Shaders: %s\n", priv->has_shaders ? "Yes" : "No"));
    D(bug("  Contexts Created: %ld\n", priv->contexts_created));
    D(bug("  Draw Calls: %ld\n", priv->draw_calls));
    D(bug("  Capabilities: 0x%08lx\n", OpenGLGetCapabilities()));
    
    D(bug("=== End OpenGL Debug Info ===\n"));
}

/*****************************************************************************/
/* Backend Implementation - Lifecycle                                        */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx)
{
    OpenGLPrivateData *priv;
    
    D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend\n"));
    
    if (!ctx) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: NULL context\n"));
        return FALSE;
    }

    /* Allocate private data */
    priv = AllocVec(sizeof(OpenGLPrivateData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!priv) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: Failed to allocate private data\n"));
        return FALSE;
    }

    /* Check if GL library is available (opened in DetectLibraries) */
    if (!OpenGL_CheckLibrary(priv)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: GL library not available\n"));
        FreeVec(priv);
        return FALSE;
    }

    /* Set default capabilities */
    OpenGL_CheckCapabilities(priv);

    /* Mark as initialized */
    priv->initialized = TRUE;
    priv->contexts_created = 0;
    priv->draw_calls = 0;

    /* Store private data in context */
    ctx->private_data = priv;
    ctx->capabilities = BACKEND_CAP_BASIC | 
                        BACKEND_CAP_OPENGL | 
                        BACKEND_CAP_HARDWARE |
                        BACKEND_CAP_BLENDING |
                        BACKEND_CAP_ANTIALIASING |
                        BACKEND_CAP_TEXTURES;
    ctx->initialized = TRUE;

    D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: Backend initialized successfully\n"));
    OpenGL_DumpDebugInfo(priv);
    
    return TRUE;
}

static void OpenGLCleanupBackend(ZuneBackendContext *ctx)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupBackend\n"));
    
    if (!ctx || !ctx->private_data) {
        return;
    }

    OpenGLPrivateData *priv = (OpenGLPrivateData *)ctx->private_data;

    /* GL library is closed centrally in CleanupZuneRenderer() */
    priv->GLBase = NULL;
    priv->gl_available = FALSE;

    /* Free private data */
    FreeVec(priv);
    ctx->private_data = NULL;
    ctx->initialized = FALSE;
    
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupBackend: Cleanup complete\n"));
}

static BOOL OpenGLIsAvailable(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLIsAvailable: Checking for gl.library\n"));
    
    /*
     * Check if GLBase was opened in DetectLibraries().
     * 
     * NOTE: Currently returning FALSE because the OpenGL backend is not
     * yet fully implemented. The drawing functions are stubs.
     * Change this to return (GLBase != NULL) once the backend is functional.
     */
    if (GLBase) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLIsAvailable: gl.library v%ld.%ld found, but backend not yet implemented\n",
              GLBase->lib_Version, GLBase->lib_Revision));
    } else {
        D(bug("[ZuneRenderer:OpenGL] OpenGLIsAvailable: gl.library not found\n"));
    }
    
    /* Return FALSE until backend is fully implemented */
    return FALSE;
}

static BOOL OpenGLIsCompatible(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: RenderPort %p\n", rp));
    
    /* 
     * OpenGL backend requires:
     * 1. A valid window for context creation
     * 2. A CyberGraphics-compatible screen (for hardware acceleration)
     * 
     * For now, we're conservative and only report compatible if we have
     * a window-based RenderPort.
     */
    
    if (!rp) {
        /* NULL rp means checking general compatibility */
        return OpenGLIsAvailable();
    }
    
    /* We need a target RastPort with an associated window */
    if (!rp->target_rp) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: No target RastPort\n"));
        return FALSE;
    }
    
    /* 
     * TODO: Add more sophisticated checks:
     * - Check if the screen is CyberGraphics compatible
     * - Check if we can create a GL context for this window
     */
    
    D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: Compatible\n"));
    return TRUE;
}

static ULONG OpenGLGetCapabilities(void)
{
    if (!OpenGLIsAvailable()) {
        return 0;
    }
    
    return BACKEND_CAP_BASIC | 
           BACKEND_CAP_OPENGL | 
           BACKEND_CAP_HARDWARE |
           BACKEND_CAP_BLENDING |
           BACKEND_CAP_ANTIALIASING |
           BACKEND_CAP_TEXTURES;
}

/*****************************************************************************/
/* RenderPort Management                                                     */
/*****************************************************************************/

static BOOL OpenGLInitRenderPort(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLInitRenderPort: RenderPort %p\n", rp));
    
    if (!rp) {
        return FALSE;
    }

    /* 
     * GL context creation is deferred until first use.
     * This is because we may not have a valid window yet.
     */
    
    return TRUE;
}

static void OpenGLCleanupRenderPort(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupRenderPort: RenderPort %p\n", rp));
    
    if (!rp) {
        return;
    }

    /* 
     * TODO: Destroy any GL context associated with this RenderPort
     */
}

/*****************************************************************************/
/* Color Management                                                          */
/*****************************************************************************/

static BOOL OpenGLPrepareColor(struct RenderPort *rp,
                               struct InternalColor *color)
{
    if (!color) {
        return FALSE;
    }

    /* 
     * OpenGL uses floating-point colors (0.0 - 1.0).
     * The InternalColor already has r, g, b, a as UBYTE (0-255).
     * We'll convert when actually using the color in GL calls.
     */
    
    return TRUE;
}

static void OpenGLReleaseColor(struct RenderPort *rp,
                               struct InternalColor *color)
{
    /* Nothing to release for OpenGL colors */
}

/*****************************************************************************/
/* Drawing Operations (Stubs - to be implemented)                            */
/*****************************************************************************/

static void OpenGLDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawPixel: (%d,%d)\n", x, y));
    
    /* TODO: Implement using glBegin(GL_POINTS) / glVertex2i / glEnd */
}

static void OpenGLDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawLine: (%d,%d) to (%d,%d)\n",
          startX, startY, endX, endY));
    
    /* TODO: Implement using glBegin(GL_LINES) / glVertex2i / glEnd */
}

static void OpenGLClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLClearRenderPort\n"));
    
    /* TODO: Implement using glClearColor / glClear */
}

/*****************************************************************************/
/* Direct Pixel Access                                                       */
/*****************************************************************************/

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLLockPixels: Not supported\n"));
    
    /* 
     * OpenGL doesn't support direct pixel access in the same way.
     * Use glReadPixels for reading, textures for writing.
     */
    if (pitch_out) {
        *pitch_out = 0;
    }
    return NULL;
}

static void OpenGLUnlockPixels(struct DrawingBoard *board)
{
    /* Nothing to unlock */
}

static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y)
{
    /* TODO: Implement using glReadPixels */
    return 0x00000000;
}

static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color)
{
    /* TODO: Implement - may need to draw a point or update texture */
}

/*****************************************************************************/
/* Batching Operations                                                       */
/*****************************************************************************/

static void OpenGLBeginBatch(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLBeginBatch\n"));
    
    /* 
     * TODO: Begin collecting draw commands for batched submission.
     * Could use vertex arrays or display lists for batching.
     */
}

static void OpenGLEndBatch(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLEndBatch\n"));
    
    /* TODO: Flush batched commands and swap buffers if needed */
}

static void OpenGLFlushBatch(struct RenderPort *rp)
{
    /* TODO: glFlush() */
}

static BOOL OpenGLIsBatching(struct RenderPort *rp)
{
    return FALSE; /* Not yet implemented */
}

/*****************************************************************************/
/* Blitting Operations                                                       */
/*****************************************************************************/

static void OpenGLBlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLBlitRenderPorts: %dx%d\n", width, height));
    
    /* TODO: Implement using FBOs or texture copying */
}

static void OpenGLBlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: %dx%d\n", width, height));
    
    /* TODO: glASwapBuffers or read back to RastPort */
}

/*****************************************************************************/
/* DrawingBoard Management                                                   */
/*****************************************************************************/

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLInitDrawingBoard: %p\n", board));
    
    if (!board) {
        return FALSE;
    }

    /* 
     * TODO: Create an offscreen rendering target (FBO or pbuffer)
     * for DrawingBoard support.
     */

    return TRUE;
}

static void OpenGLCleanupDrawingBoard(struct DrawingBoard *board)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupDrawingBoard: %p\n", board));
    
    /* TODO: Destroy FBO/pbuffer */
}
