/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - OpenGL Backend Core

    This file implements the core OpenGL rendering backend for ZuneGfx:
    backend lifecycle, availability checks, context switching, pixel access,
    batching, blitting, and DrawingBoard management.

    Drawing primitives are in opengl_drawing.c.
    Shader management is in opengl_shaders.c.
    FBO management is in opengl_fbo.c.
    GL context management is in opengl_context.c.
    Sync/blit helpers are in opengl_sync.c.
    Pixel buffer utilities are in opengl_pixel_utils.c.
    Global variable definitions are in opengl_globals.c.
*/

#include "opengl_intern.h"

/* Forward declarations for static functions */
static ULONG OpenGLGetCapabilities(void);

/*****************************************************************************/
/* Library/Capability Checking                                               */
/*****************************************************************************/

/*
 * OpenGL_CheckLibrary - Verify GL library is available
 *
 * The gl.library is opened centrally in DetectLibraries() (zunegfx_core.c).
 * This function just checks if it's available and stores a reference.
 *
 * Returns TRUE if library is available.
 */
BOOL OpenGL_CheckLibrary(OpenGLPrivateData *priv)
{
    if (!priv) {
        return FALSE;
    }

    /* GLBase is opened in DetectLibraries() */
    if (GLBase) {
        priv->GLBase = GLBase;
        priv->gl_available = TRUE;
        return TRUE;
    }

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
    priv->max_texture_size = 1024;
    priv->has_npot_textures = FALSE;
    priv->has_framebuffers = FALSE;
    priv->has_shaders = FALSE;

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

    D(bug("  Global Context: %s (ptr=%p)\n",
          priv->context_created ? "Created" : "Not created",
          priv->gl_context));
    D(bug("  Current Window: %p (%dx%d)\n",
          priv->current_window, priv->current_width, priv->current_height));
    D(bug("  GL Version: %ld.%ld\n",
          priv->gl_version_major, priv->gl_version_minor));
    D(bug("  Max Texture Size: %ld\n", priv->max_texture_size));
    D(bug("  NPOT Textures: %s\n", priv->has_npot_textures ? "Yes" : "No"));
    D(bug("  Framebuffers: %s\n", priv->has_framebuffers ? "Yes" : "No"));
    D(bug("  Shaders: %s\n", priv->has_shaders ? "Yes" : "No"));
    D(bug("  Draw Calls: %ld\n", priv->draw_calls));
    D(bug("  SetRast Calls: %ld\n", priv->setrast_calls));
    D(bug("  Capabilities: 0x%08lx\n", OpenGLGetCapabilities()));

    D(bug("=== End OpenGL Debug Info ===\n"));
}

/*****************************************************************************/
/* Backend Lifecycle                                                         */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx)
{
    OpenGLPrivateData *priv;

    if (!ctx) {
        return FALSE;
    }

    /* Allocate private data */
    priv = AllocVec(sizeof(OpenGLPrivateData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!priv) {
        return FALSE;
    }

    /* Check if GL library is available (opened in DetectLibraries) */
    if (!OpenGL_CheckLibrary(priv)) {
        FreeVec(priv);
        return FALSE;
    }

    /* Set default capabilities */
    OpenGL_CheckCapabilities(priv);

    /* Initialize global context state */
    priv->gl_context = NULL;
    priv->context_created = FALSE;
    priv->current_window = NULL;
    priv->current_width = 0;
    priv->current_height = 0;

    /* Mark as initialized */
    priv->initialized = TRUE;
    priv->draw_calls = 0;
    priv->setrast_calls = 0;

    /* Store private data in context AND global pointer */
    ctx->private_data = priv;
    g_opengl_priv = priv;  /* Global access for drawing functions */

    ctx->capabilities = BACKEND_CAP_BASIC |
                        BACKEND_CAP_OPENGL |
                        BACKEND_CAP_HARDWARE |
                        BACKEND_CAP_BLENDING |
                        BACKEND_CAP_ANTIALIASING |
                        BACKEND_CAP_TEXTURES;
    ctx->initialized = TRUE;

    return TRUE;
}

static void OpenGLCleanupBackend(ZuneBackendContext *ctx)
{

    if (!ctx || !ctx->private_data) {
        return;
    }

    OpenGLPrivateData *priv = (OpenGLPrivateData *)ctx->private_data;

    /* Destroy VBO before destroying the GL context */
    OpenGL_DestroyQuadVBO();

    /* Destroy shaders before destroying the GL context */
    OpenGL_DestroyShaders();

    /* Destroy all window contexts first */
    while (priv->window_contexts) {
        OpenGLWindowContext *ctx_to_destroy = priv->window_contexts;
        priv->window_contexts = ctx_to_destroy->next;

        if (ctx_to_destroy->gl_context) {
            glADestroyContext((GLAContext)ctx_to_destroy->gl_context);
        }
        FreeVec(ctx_to_destroy);
    }
    priv->current_context = NULL;

    /* Destroy the master context if it exists */
    if (priv->master_context) {
        glADestroyContext((GLAContext)priv->master_context);
        /* If global context is the same as master (promoted), clear it too */
        if (priv->gl_context == priv->master_context) {
            priv->gl_context = NULL;
            priv->context_created = FALSE;
        }
        priv->master_context = NULL;
        priv->master_context_created = FALSE;
        priv->shared_contexts_supported = FALSE;
    }

    /* Destroy the global GL context if it exists and is separate from master */
    if (priv->gl_context) {
        glADestroyContext((GLAContext)priv->gl_context);
        priv->gl_context = NULL;
        priv->context_created = FALSE;
    }

    /* GL library is closed centrally in CleanupZuneGfx() */
    priv->GLBase = NULL;
    priv->gl_available = FALSE;

    /* Clear global pointer */
    g_opengl_priv = NULL;

    /* Free private data */
    FreeVec(priv);
    ctx->private_data = NULL;
    ctx->initialized = FALSE;
}

/*****************************************************************************/
/* Availability and Compatibility                                            */
/*****************************************************************************/

static BOOL OpenGLIsAvailable(void)
{
    /*
     * Always re-check if GLBase became available.
     * This handles the case where GL library is opened after
     * initial backend registration.
     */
    if (GLBase != NULL) {
        g_opengl_available_cached = TRUE;
        g_opengl_available_checked = TRUE;
        return TRUE;
    }

    /* Return cached FALSE result if already checked and still not available */
    if (g_opengl_available_checked) {
        return g_opengl_available_cached;
    }

    /*
     * Check if GLBase was opened in DetectLibraries().
     */
    g_opengl_available_cached = (GLBase != NULL);
    g_opengl_available_checked = TRUE;
    return g_opengl_available_cached;
}

static BOOL OpenGLIsCompatible(struct RenderContext *rctx)
{
    if (!rctx) {
        /* NULL rctx means checking general compatibility */
        return OpenGLIsAvailable();
    }

    D(bug("[ZuneGfx:OpenGL] IsCompatible: rctx=%p window=%p target_board=%p\n",
          rctx, rctx->window, rctx->target_board));
    if (rctx->target_board) {
        D(bug("[ZuneGfx:OpenGL] IsCompatible: board->parent_window=%p\n",
              rctx->target_board->parent_window));
    }

    /*
     * NEW ARCHITECTURE: OpenGL compatibility is based on having a Window.
     *
     * OpenGL requires a Window to create a GL context. The RenderContext should
     * have rctx->window set (via ZuneCreateRenderContextForWindow) for OpenGL to work.
     *
     * With the new architecture:
     * - RenderContext is bound to a Window (required for GL context)
     * - DrawingBoards always have BitMap (for legacy compatibility)
     * - OpenGL adds FBO to DrawingBoard for accelerated rendering
     * - Switching targets uses glBindFramebuffer() (fast)
     */

    /* Check if RenderContext has a window - required for GL context */
    if (rctx->window) {
        return TRUE;
    }

    /* Legacy path: Check for DrawingBoard with parent_window */
    if (rctx->target_board && rctx->target_board->parent_window) {
        return TRUE;
    }

    /* No Window means we can't create a GL context - fall back to CyberGraphics */
    return FALSE;
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

static ULONG OpenGLGetPixelFormat(struct BitMap *bitmap)
{
    /* OpenGL backend uses CyberGfx bitmaps as backing store */
    if (CyberGfxBase && bitmap) {
        return GetCyberMapAttr(bitmap, CYBRMATTR_PIXFMT);
    }
    return 0;
}

/*****************************************************************************/
/* Context Switching Helpers                                                 */
/*****************************************************************************/

/*
 * OpenGL_SwitchToWindow - Switch the GL context to render to a different window
 *
 * NEW ARCHITECTURE (with shared contexts):
 * - Each window has its own GL context that shares resources with the master context
 * - Switching uses glAMakeCurrent() which is safe and efficient
 * - No glASetRast() needed - avoids crashes with different dimensions
 *
 * FALLBACK (without shared contexts):
 * - Uses the old glASetRast() method with the single global context
 *
 * Returns TRUE if switch was successful.
 */
BOOL OpenGL_SwitchToWindow(struct RenderContext *rctx)
{
    struct Window *window = NULL;
    struct RastPort *rastport;
    OpenGLWindowContext *win_ctx;
    UWORD width, height;

    if (!rctx || !g_opengl_priv) {
        return FALSE;
    }

    /* Get the target window */
    if (rctx->target_rastport) {
        rastport = rctx->target_rastport;
        if (rastport->Layer && rastport->Layer->Window) {
            window = (struct Window *)rastport->Layer->Window;
        }
    }

    if (!window) {
        return FALSE;
    }

    /* Calculate window dimensions */
    width = window->Width - window->BorderLeft - window->BorderRight;
    height = window->Height - window->BorderTop - window->BorderBottom;

    /*
     * NEW: Try shared context approach first.
     * Each window gets its own GL context that shares resources with master.
     * This allows safe switching via glAMakeCurrent().
     *
     * We try this path if:
     * 1. Shared contexts are already known to be supported, OR
     * 2. Master context hasn't been created yet (we'll create it and check)
     */
    if (g_opengl_priv->shared_contexts_supported || !g_opengl_priv->master_context_created) {
        /* Ensure master context exists */
        if (!g_opengl_priv->master_context_created) {
            D(bug("[ZuneGfx:OpenGL] SwitchToWindow: Creating master context for shared context support\n"));
            if (!OpenGL_CreateMasterContext(window)) {
                D(bug("[ZuneGfx:OpenGL] SwitchToWindow: Master context creation failed\n"));
                goto fallback_setrast;
            }
        }

        /* Check if sharing is actually supported after master context creation */
        if (!g_opengl_priv->shared_contexts_supported) {
            D(bug("[ZuneGfx:OpenGL] SwitchToWindow: Shared contexts not supported, using fallback\n"));
            goto fallback_setrast;
        }

        /* Find or create window context */
        win_ctx = OpenGL_FindWindowContext(window);
        if (!win_ctx) {
            win_ctx = OpenGL_CreateWindowContext(window);
            if (!win_ctx) {
                goto fallback_setrast;
            }
        }

        /* Check if this window context uses shared resources */
        if (win_ctx->uses_shared_context) {
            /* Check if we need to switch */
            if (g_opengl_priv->current_context != win_ctx ||
                g_opengl_priv->current_target_type != OPENGL_TARGET_WINDOW) {

                /* Simply make this window's context current - fast! */
                glAMakeCurrent((GLAContext)win_ctx->gl_context);

                /* Update state */
                g_opengl_priv->current_context = win_ctx;
                g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
                g_opengl_priv->current_window = window;
                g_opengl_priv->current_board = NULL;
                g_opengl_priv->current_width = width;
                g_opengl_priv->current_height = height;
                g_opengl_priv->context_switches++;

                /* Setup projection for new dimensions */
                OpenGL_SetupOrthoProjection(width, height);

                g_opengl_priv->needs_sync = TRUE;
            }
            return TRUE;
        }
        /* Fall through to glASetRast if this context doesn't share */
    }

fallback_setrast:
    /*
     * FALLBACK: Use the old glASetRast() method.
     * This is used when:
     * - Shared contexts are not supported
     * - Window context creation failed
     * - The window context doesn't share resources
     */

    /* Ensure global context exists (creates it if this is the first window) */
    if (!OpenGL_EnsureGlobalContext(window)) {
        return FALSE;
    }

    /* Check if we need to switch targets (including from DrawingBoard to Window) */
    if (g_opengl_priv->current_target_type != OPENGL_TARGET_WINDOW ||
        g_opengl_priv->current_window != window ||
        g_opengl_priv->current_width != width ||
        g_opengl_priv->current_height != height) {

        struct TagItem setrast_tags[6];
        WORD tag_idx = 0;

        /* Build tags for glASetRast */
        setrast_tags[tag_idx].ti_Tag = GLA_Window;
        setrast_tags[tag_idx].ti_Data = (IPTR)window;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Left;
        setrast_tags[tag_idx].ti_Data = window->BorderLeft;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Top;
        setrast_tags[tag_idx].ti_Data = window->BorderTop;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Right;
        setrast_tags[tag_idx].ti_Data = window->BorderRight;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Bottom;
        setrast_tags[tag_idx].ti_Data = window->BorderBottom;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = TAG_DONE;
        setrast_tags[tag_idx].ti_Data = 0;

        /* Switch render target */
        glASetRast((GLAContext)g_opengl_priv->gl_context, setrast_tags);

        /* Update state */
        g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
        g_opengl_priv->current_window = window;
        g_opengl_priv->current_board = NULL;
        g_opengl_priv->current_width = width;
        g_opengl_priv->current_height = height;
        g_opengl_priv->setrast_calls++;

        /* Make context current (may be needed after SetRast) */
        glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);

        /* Re-setup orthographic projection for new dimensions */
        OpenGL_SetupOrthoProjection(width, height);

        /* Mark that we need to sync from RastPort before drawing */
        g_opengl_priv->needs_sync = TRUE;
    }

    return TRUE;
}

/*
 * OpenGL_SwitchToDrawingBoard - Switch the global GL context to render to a DrawingBoard
 *
 * Uses FBO binding to switch the render target to an off-screen DrawingBoard.
 *
 * Returns TRUE if switch was successful.
 */
BOOL OpenGL_SwitchToDrawingBoard(struct RenderContext *rctx)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    if (!rctx || !rctx->target_board || !g_opengl_priv) {
        return FALSE;
    }

    board = rctx->target_board;

    /*
     * CRITICAL: Ensure the global context is current before any GL operations.
     * Another application/compositor may have made a different context current.
     * FBOs are per-context, so we MUST use the same context that created the FBO.
     */
    if (g_opengl_priv->context_created && g_opengl_priv->gl_context) {
        GLAContext current_ctx = glAGetCurrentContext();
        if (current_ctx != (GLAContext)g_opengl_priv->gl_context) {
            D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: Wrong context active (%p), switching to global (%p)\n",
                  current_ctx, g_opengl_priv->gl_context));
            glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);
        }
    }

    /*
     * FBO-based DrawingBoard switching
     *
     * Instead of using glASetRast (which causes crashes when switching between
     * targets with different dimensions), we use FBOs:
     * - Each DrawingBoard gets its own FBO
     * - Switching is done via glBindFramebuffer (very fast)
     * - No internal Mesa framebuffer reallocation needed
     */
    if (board->width == 0 || board->height == 0) {
        return FALSE;
    }

    /*
     * We need a GL context before we can create/use FBOs.
     * If no context exists yet, we need to create one first.
     * This requires a window - check multiple sources:
     * 1. DrawingBoard's parent_window (set by user)
     * 2. RenderContext's target_rastport Layer->Window
     */
    if (!g_opengl_priv->context_created) {
        struct Window *window = NULL;

        /* First, check if DrawingBoard has a parent_window set */
        if (board->parent_window) {
            window = board->parent_window;
        }
        /* Otherwise, try to get window from RenderContext's target_rastport */
        else if (rctx->target_rastport && rctx->target_rastport->Layer && rctx->target_rastport->Layer->Window) {
            window = (struct Window *)rctx->target_rastport->Layer->Window;
        }

        if (window) {
            /* Create context for the window */
            if (!OpenGL_EnsureGlobalContext(window)) {
                return FALSE;
            }
        } else {
            /* No window available - DrawingBoards require a window for GL context */
            return FALSE;
        }
    }

    /* Check if FBO support is available */
    if (!g_fbo_available) {
        /*
         * FBO not available (e.g., SoftPipe renderer).
         *
         * The legacy glASetRast method doesn't work reliably for off-screen
         * DrawingBoards because Mesa/SoftPipe can only render to window
         * RastPorts, not arbitrary bitmaps.
         *
         * Return FALSE to trigger fallback to CyberGfx/software rendering
         * for DrawingBoard operations. This ensures rendering actually works.
         */
        D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: FBO not available, fallback to CyberGfx\n"));
        return FALSE;
    }

    /* Get or create FBO for this DrawingBoard */
    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo) {
        /* Create FBO for this DrawingBoard */
        fbo = OpenGL_CreateFBO(board->width, board->height);
        if (!fbo) {
            return FALSE;
        }

        /* Store FBO in DrawingBoard */
        board->backend_data = fbo;

        /*
         * Track parent context for cleanup and for making the correct context
         * current when blitting. Note: current_context may be NULL if we're
         * using the global context path (which is common for single-context
         * AROS/Mesa). In that case, OpenGL_BlitFBOToRastPort will fall back
         * to g_opengl_priv->gl_context.
         */
        fbo->parent_context = g_opengl_priv->current_context;
    }

    /* Check if we need to switch to this FBO */
    if (g_opengl_priv->current_target_type != OPENGL_TARGET_DRAWINGBOARD ||
        g_opengl_priv->current_board != board) {

        D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: Binding FBO %u (was target_type=%d)\n",
              fbo->fbo_id, g_opengl_priv->current_target_type));

        /* Bind the FBO - this is much faster than glASetRast! */
        if (!OpenGL_BindFBO(fbo)) {
            D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: BindFBO FAILED\n"));
            return FALSE;
        }

        /* Update state */
        g_opengl_priv->current_target_type = OPENGL_TARGET_DRAWINGBOARD;
        g_opengl_priv->current_board = board;
        g_opengl_priv->current_window = NULL;
        g_opengl_priv->current_width = board->width;
        g_opengl_priv->current_height = board->height;

        /* DrawingBoards don't need sync - they start fresh */
        g_opengl_priv->needs_sync = FALSE;
    } else {
        D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: Already on same board, just reset projection\n"));
        /*
         * Already on the same DrawingBoard - FBO is already bound.
         * However, we must ensure viewport and projection are correct,
         * as they may have been modified by other GL operations
         * (e.g., ZuneReload's texture drawing or window resize).
         *
         * This is a lightweight operation compared to full FBO binding,
         * but ensures rendering uses correct coordinate system.
         */
        glViewport(0, 0, fbo->width, fbo->height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, fbo->width, fbo->height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    return TRUE;
}

/*
 * OpenGL_SwitchToTarget - Unified function to switch GL context to any target
 *
 * Determines if the RenderContext targets a Window or DrawingBoard and calls
 * the appropriate switching function.
 *
 * Returns TRUE if switch was successful.
 */
BOOL OpenGL_SwitchToTarget(struct RenderContext *rctx)
{
    if (!rctx || !g_opengl_priv) {
        return FALSE;
    }

    /* Check if this is a DrawingBoard target */
    if (rctx->target_board) {
        return OpenGL_SwitchToDrawingBoard(rctx);
    }

    /* Otherwise it's a Window-based RastPort */
    return OpenGL_SwitchToWindow(rctx);
}

/*
 * Set up 2D orthographic projection for pixel-perfect rendering
 */
void OpenGL_SetupOrthoProjection(UWORD width, UWORD height)
{

    /* Set viewport to full render area */
    glViewport(0, 0, width, height);

    /* Set up orthographic projection */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* Y-axis flipped: 0 at top, height at bottom (like 2D screen coords) */
    glOrtho(0, width, height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Enable blending for alpha */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Disable depth test for 2D */
    glDisable(GL_DEPTH_TEST);
}

/*
 * Set current GL color from InternalColor
 */
void OpenGL_SetColor(struct InternalColor *color)
{
    if (!color) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    glColor4ub(color->r, color->g, color->b, color->a);
}

/*****************************************************************************/
/* Direct Pixel Access                                                       */
/*****************************************************************************/

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out)
{
    /*
     * OpenGL doesn't support direct pixel access in the same way.
     * For now, return NULL. Future: could use glReadPixels to read
     * into a buffer.
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
    /* TODO: Would need to draw a point via the RenderContext */
}

/*****************************************************************************/
/* Batching Operations                                                       */
/*****************************************************************************/

static void OpenGLBeginBatch(struct RenderContext *rctx)
{
    /*
     * OpenGL naturally batches commands. We could use this to
     * defer glASwapBuffers until ZuneEndBatch.
     */
    if (rctx) {
        rctx->batching_enabled = TRUE;
    }
}

static void OpenGLEndBatch(struct RenderContext *rctx)
{
    if (!rctx) {
        return;
    }

    rctx->batching_enabled = FALSE;

    /* Flush and swap buffers using global context */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();
        glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
    }
}

static void OpenGLFlushBatch(struct RenderContext *rctx)
{
    if (!rctx) {
        return;
    }

    /* Flush and swap buffers using global context */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();
        glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
    }
}

static BOOL OpenGLIsBatching(struct RenderContext *rctx)
{
    if (!rctx) {
        return FALSE;
    }
    return rctx->batching_enabled;
}

/*****************************************************************************/
/* Blitting Operations                                                       */
/*****************************************************************************/

static void OpenGLBlitRenderContexts(struct RenderContext *source,
                                  struct RenderContext *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height)
{
    struct DrawingBoard *src_board, *dst_board;

    if (!source || !dest) {
        return;
    }

    src_board = source->target_board;
    dst_board = dest->target_board;

    /*
     * ZERO-COPY PATH: Both source and destination are DrawingBoards with FBOs
     *
     * This is the optimal case - we can blit directly from one FBO texture
     * to another FBO without involving the CPU at all. All data stays on GPU.
     */
    if (src_board && src_board->backend_data &&
        dst_board && dst_board->backend_data &&
        g_fbo_available) {

        OpenGLFBOData *src_fbo = (OpenGLFBOData *)src_board->backend_data;
        OpenGLFBOData *dst_fbo = (OpenGLFBOData *)dst_board->backend_data;

        if (src_fbo->valid && dst_fbo->valid) {
            OpenGL_BlitFBOToFBO(src_board, dst_board,
                                src_x, src_y, dest_x, dest_y, width, height);
            return;
        }
    }

    /*
     * FALLBACK PATH: Use software blitting via CyberGfx
     *
     * This handles cases where:
     * - Source or destination is a Window (not DrawingBoard)
     * - FBOs are not available
     * - One or both boards don't have valid FBO data
     */

    /* If source is a DrawingBoard with FBO, sync it to bitmap first */
    if (src_board && src_board->backend_data && g_fbo_available) {
        OpenGLFBOData *src_fbo = (OpenGLFBOData *)src_board->backend_data;
        if (src_fbo->valid && src_fbo->dirty) {
            OpenGL_SyncFBOToBitmap(source);
        }
    }

    /* Use BltBitMapRastPort for the actual blit if both have rastports */
    if (src_board && src_board->rastport && src_board->rastport->BitMap &&
        dst_board && dst_board->rastport && dst_board->rastport->BitMap) {

        BltBitMapRastPort(src_board->rastport->BitMap,
                          src_x, src_y,
                          dst_board->rastport,
                          dest_x, dest_y,
                          width, height,
                          0xC0);  /* Copy */
    }
}

static void OpenGLBlitToScreen(struct RenderContext *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;
    struct Window *target_window = NULL;
    OpenGLWindowContext *win_ctx = NULL;

    if (!source || !screen_rp) {
        return;
    }

    board = source->target_board;

    /* Try to get target window from RastPort */
    if (screen_rp->Layer && screen_rp->Layer->Window) {
        target_window = (struct Window *)screen_rp->Layer->Window;
    }

    /*
     * GPU-ACCELERATED PATH: Blit FBO directly to window via shared context
     *
     * If the target RastPort belongs to a window that has a shared GL context,
     * we can render the FBO texture directly to that window's framebuffer.
     * This avoids the expensive glReadPixels + WritePixelArray roundtrip.
     */
    if (board && board->backend_data && g_fbo_available && target_window &&
        g_opengl_priv && g_opengl_priv->shared_contexts_supported) {

        fbo = (OpenGLFBOData *)board->backend_data;

        /* Find or create window context */
        win_ctx = OpenGL_FindWindowContext(target_window);
        if (!win_ctx) {
            /* Try to create shared context for this window */
            if (g_opengl_priv->master_context_created) {
                win_ctx = OpenGL_CreateWindowContext(target_window);
            }
        }

        /* If we have a shared window context, use GPU path */
        if (win_ctx && win_ctx->uses_shared_context && fbo->valid) {
            GLuint src_texture = fbo->texture_id;
            GLfloat tex_x1, tex_y1, tex_x2, tex_y2;
            UWORD win_width, win_height;

            /* Make window context current */
            glAMakeCurrent((GLAContext)win_ctx->gl_context);

            /* Update state */
            g_opengl_priv->current_context = win_ctx;
            g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
            g_opengl_priv->current_window = target_window;
            g_opengl_priv->current_board = NULL;
            g_opengl_priv->context_switches++;

            /* Setup projection for window */
            win_width = target_window->Width - target_window->BorderLeft - target_window->BorderRight;
            win_height = target_window->Height - target_window->BorderTop - target_window->BorderBottom;
            OpenGL_SetupOrthoProjection(win_width, win_height);

            /* Unbind any FBO - render to window's default framebuffer */
            glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

            /* Ensure source FBO rendering is complete */
            glFlush();

            /* Calculate texture coordinates */
            tex_x1 = (GLfloat)src_x / (GLfloat)fbo->width;
            tex_y1 = (GLfloat)src_y / (GLfloat)fbo->height;
            tex_x2 = (GLfloat)(src_x + width) / (GLfloat)fbo->width;
            tex_y2 = (GLfloat)(src_y + height) / (GLfloat)fbo->height;

            /* Flip Y for FBO texture */
            tex_y1 = 1.0f - tex_y1;
            tex_y2 = 1.0f - tex_y2;

            /* Setup for textured rendering */
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, src_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            /* Disable shader */
            if (glUseProgram_ptr) {
                glUseProgram_ptr(0);
            }

            /* Enable blending for alpha */
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            /* Draw textured quad to window */
            glBegin(GL_QUADS);
            glTexCoord2f(tex_x1, tex_y1); glVertex2i(dest_x, dest_y);
            glTexCoord2f(tex_x2, tex_y1); glVertex2i(dest_x + width, dest_y);
            glTexCoord2f(tex_x2, tex_y2); glVertex2i(dest_x + width, dest_y + height);
            glTexCoord2f(tex_x1, tex_y2); glVertex2i(dest_x, dest_y + height);
            glEnd();

            glDisable(GL_TEXTURE_2D);

            /* Swap to make visible */
            glASwapBuffers((GLAContext)win_ctx->gl_context);
            return;
        }
    }

    /*
     * FALLBACK: FBO-based software blitting
     *
     * If this DrawingBoard has an FBO, we need to:
     * 1. Bind the FBO to read from it
     * 2. Read pixels using glReadPixels
     * 3. Write pixels to the destination RastPort
     *
     * If no FBO, just swap buffers (window rendering path).
     */
    if (board && board->backend_data && g_fbo_available) {
        UBYTE *pixelbuffer;
        UBYTE *flipped_buffer;
        ULONG row, src_row, dst_row;

        fbo = (OpenGLFBOData *)board->backend_data;

        if (!CyberGfxBase) {
            return;
        }

        /* Clamp dimensions to FBO size */
        if (src_x + width > fbo->width) width = fbo->width - src_x;
        if (src_y + height > fbo->height) height = fbo->height - src_y;
        if (width == 0 || height == 0) return;

        /* Allocate buffers */
        pixelbuffer = AllocVec(width * height * 4, MEMF_ANY);
        if (!pixelbuffer) {
            return;
        }

        flipped_buffer = AllocVec(width * height * 4, MEMF_ANY);
        if (!flipped_buffer) {
            FreeVec(pixelbuffer);
            return;
        }

        /* Bind the FBO for reading */
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo->fbo_id);

        /* Ensure GL operations are complete */
        glFlush();
        glFinish();

        /* Read pixels from FBO */
        glReadPixels(src_x, src_y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelbuffer);

        /* Unbind FBO */
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

        /*
         * Flip the image vertically because:
         * - OpenGL has Y=0 at bottom
         * - Screen coordinates have Y=0 at top
         */
        for (row = 0; row < height; row++) {
            src_row = row * width * 4;
            dst_row = (height - 1 - row) * width * 4;
            CopyMem(pixelbuffer + src_row, flipped_buffer + dst_row, width * 4);
        }

        /* Write to destination RastPort */
        WritePixelArray(flipped_buffer, 0, 0, width * 4,
                        screen_rp, dest_x, dest_y,
                        width, height, RECTFMT_RGBA);

        FreeVec(flipped_buffer);
        FreeVec(pixelbuffer);
    } else {
        /*
         * No FBO: rendering went directly to window's GL buffer.
         * Just swap buffers to make it visible.
         */
        if (g_opengl_priv && g_opengl_priv->gl_context) {
            glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
        }
    }
}

/*****************************************************************************/
/* DrawingBoard Management                                                   */
/*****************************************************************************/

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board)
{
    if (!board) {
        return FALSE;
    }

    /*
     * FBO-based DrawingBoard initialization
     *
     * We don't create the FBO here because we need an active GL context first.
     * The FBO will be created lazily in OpenGL_SwitchToDrawingBoard when
     * rendering first targets this DrawingBoard.
     *
     * For now, we just:
     * 1. Mark as hardware surface (OpenGL uses GPU)
     * 2. Initialize backend_data to NULL (FBO will be stored here later)
     */
    board->hardware_surface = TRUE;
    board->backend_data = NULL;

    return TRUE;
}

void OpenGLCleanupDrawingBoard(struct DrawingBoard *board)
{
    if (!board) {
        return;
    }

    /*
     * Destroy the FBO if one was created for this DrawingBoard
     */
    if (board->backend_data) {
        OpenGLFBOData *fbo = (OpenGLFBOData *)board->backend_data;

        /*
         * We need an active GL context to destroy the FBO.
         * If this board's parent context is still valid, make it current.
         */
        if (fbo->parent_context && fbo->parent_context->context_valid) {
            OpenGL_MakeContextCurrent(fbo->parent_context);
        } else if (g_opengl_priv && g_opengl_priv->gl_context) {
            /* Fall back to global context if available */
            glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);
        }

        OpenGL_DestroyFBO(fbo);
        board->backend_data = NULL;
    }

    /* Clear current board reference if this was it */
    if (g_opengl_priv && g_opengl_priv->current_board == board) {
        g_opengl_priv->current_board = NULL;
        g_opengl_priv->current_target_type = OPENGL_TARGET_NONE;
    }
}

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
    .GetPixelFormat = OpenGLGetPixelFormat,

    .InitRenderContext = OpenGLInitRenderContext,
    .CleanupRenderContext = OpenGLCleanupRenderContext,

    .PrepareColor = OpenGLPrepareColor,
    .ReleaseColor = OpenGLReleaseColor,

    .DrawPixel = OpenGLDrawPixel,
    .DrawLine = OpenGLDrawLine,
    .DrawRectangle = OpenGLDrawRectangle,
    .DrawCircle = OpenGLDrawCircle,

    .ClearRenderContext = OpenGLClearRenderContext,

    .LockPixels = OpenGLLockPixels,
    .UnlockPixels = OpenGLUnlockPixels,
    .GetPixel = OpenGLGetPixel,
    .SetPixel = OpenGLSetPixel,

    .BeginBatch = OpenGLBeginBatch,
    .EndBatch = OpenGLEndBatch,
    .FlushBatch = OpenGLFlushBatch,
    .IsBatching = OpenGLIsBatching,

    .BlitRenderContexts = OpenGLBlitRenderContexts,
    .BlitToScreen = OpenGLBlitToScreen,

    .InitDrawingBoard = OpenGLInitDrawingBoard,
    .CleanupDrawingBoard = OpenGLCleanupDrawingBoard,
    .CopyFromDrawingBoard = OpenGL_SyncFBOToBitmap,
    .CopyRegionFromDrawingBoard = OpenGL_SyncRegionFBOToBitmap,

    .CopyFromRastPort = OpenGLCopyFromRastPort,

    /* Text rendering */
    .DrawText = OpenGLDrawText,

    /* Polygon fill */
    .FillPolygon = OpenGLFillPolygon,

    /* Clipping */
    .SetupClipping = OpenGLSetupClipping,
    .ClearClipping = OpenGLClearClipping,
};
