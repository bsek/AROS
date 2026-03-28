#include "opengl_intern.h"

/* GLCompositor semaphore structure -- must match glcompositor_intern.h */
struct GLCompositorSemaphore
{
    struct SignalSemaphore   sem;
    APTR                    master_context;
};
#define GLCOMPOSITOR_SEMAPHORE_NAME "GLCompositorMasterContext"

/*
 * OpenGL_CreateMasterContext - Create the master GL context for resource sharing
 *
 * The master context is created once and all other window contexts share
 * resources (textures, buffers, shaders) with it via GLA_ShareContext.
 * This enables zero-copy compositing between DrawingBoards.
 *
 * Returns TRUE if master context was created successfully.
 */
BOOL OpenGL_CreateMasterContext(struct Window *window)
{
    struct TagItem tags[10];
    WORD tag_idx = 0;
    GLAContext master_ctx;
    APTR master_pipe_screen;

    if (!window || !GLBase) {
        return FALSE;
    }

    if (!g_opengl_priv) {
        return FALSE;
    }

    /* Already created? */
    if (g_opengl_priv->master_context_created && g_opengl_priv->master_context) {
        return TRUE;
    }

    /*
     * Check if GLCompositor has already published a master context.
     * If so, share with it instead of creating a standalone context.
     * This is critical on SoftPipe where only one pipe_screen exists.
     */
    {
        struct GLCompositorSemaphore *comp_sem;
        APTR compositor_ctx = NULL;

        Forbid();
        comp_sem = (struct GLCompositorSemaphore *)FindSemaphore(GLCOMPOSITOR_SEMAPHORE_NAME);
        Permit();

        if (comp_sem) {
            ObtainSemaphoreShared(&comp_sem->sem);
            compositor_ctx = comp_sem->master_context;
            ReleaseSemaphore(&comp_sem->sem);
        }

        if (compositor_ctx) {
            D(bug("[ZuneGfx:OpenGL] CreateMasterContext: Found compositor master context %p, sharing\n",
                  compositor_ctx));

            tags[tag_idx].ti_Tag = GLA_Window;
            tags[tag_idx].ti_Data = (IPTR)window;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Left;
            tags[tag_idx].ti_Data = window->BorderLeft;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Top;
            tags[tag_idx].ti_Data = window->BorderTop;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Right;
            tags[tag_idx].ti_Data = window->BorderRight;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Bottom;
            tags[tag_idx].ti_Data = window->BorderBottom;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_NoDepth;
            tags[tag_idx].ti_Data = GL_TRUE;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_NoStencil;
            tags[tag_idx].ti_Data = GL_TRUE;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_ShareContext;
            tags[tag_idx].ti_Data = (IPTR)compositor_ctx;
            tag_idx++;
            tags[tag_idx].ti_Tag = TAG_DONE;
            tags[tag_idx].ti_Data = 0;

            master_ctx = glACreateContext(tags);
            if (master_ctx) {
                g_opengl_priv->master_context = (APTR)master_ctx;
                g_opengl_priv->master_context_created = TRUE;
                g_opengl_priv->shared_contexts_supported = TRUE;

                glAMakeCurrent(master_ctx);

                D(bug("[ZuneGfx:OpenGL] CreateMasterContext: Shared with compositor, master=%p\n", master_ctx));

                OpenGL_LoadFBOFunctions();

                if (!g_shaders_available) {
                    if (OpenGL_InitShaders()) {
                        g_opengl_priv->has_shaders = TRUE;
                    }
                }
                return TRUE;
            }
            D(bug("[ZuneGfx:OpenGL] CreateMasterContext: Sharing with compositor failed, trying standalone\n"));
            tag_idx = 0;
        }
    }

    /* Set up tags for standalone master context creation */
    tags[tag_idx].ti_Tag = GLA_Window;
    tags[tag_idx].ti_Data = (IPTR)window;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Left;
    tags[tag_idx].ti_Data = window->BorderLeft;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Top;
    tags[tag_idx].ti_Data = window->BorderTop;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Right;
    tags[tag_idx].ti_Data = window->BorderRight;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Bottom;
    tags[tag_idx].ti_Data = window->BorderBottom;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoDepth;
    tags[tag_idx].ti_Data = GL_TRUE;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoStencil;
    tags[tag_idx].ti_Data = GL_TRUE;
    tag_idx++;

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create standalone master GL context */
    master_ctx = glACreateContext(tags);
    if (!master_ctx) {
        D(bug("[ZuneGfx:OpenGL] CreateMasterContext: glACreateContext FAILED\n"));
        return FALSE;
    }

    /* Store master context */
    g_opengl_priv->master_context = (APTR)master_ctx;
    g_opengl_priv->master_context_created = TRUE;

    /* Make it current to initialize GL state */
    glAMakeCurrent(master_ctx);

    /* Get pipe_screen to verify sharing will work */
    master_pipe_screen = glAGetPipeScreen(master_ctx);
    g_opengl_priv->shared_contexts_supported = (master_pipe_screen != NULL);

    D(bug("[ZuneGfx:OpenGL] CreateMasterContext: master=%p, pipe_screen=%p, shared_supported=%d\n",
          master_ctx, master_pipe_screen, g_opengl_priv->shared_contexts_supported));

    /* Load FBO functions now that we have a context */
    OpenGL_LoadFBOFunctions();

    /*
     * Initialize shaders now that we have a GL context.
     * This is deferred from library init to first window creation so we don't
     * need a hidden backdrop window. The first application window's stack is
     * typically large enough for Mesa/LLVM shader compilation.
     *
     * If the stack is too small, OpenGL_InitShaders will detect it and shaders
     * will be unavailable (fallback to non-shader rendering).
     */
    if (!g_shaders_available) {
        if (OpenGL_InitShaders()) {
            g_opengl_priv->has_shaders = TRUE;
            D(bug("[ZuneGfx:OpenGL] CreateMasterContext: Shaders compiled successfully\n"));
        } else {
            D(bug("[ZuneGfx:OpenGL] CreateMasterContext: Shader compilation failed (stack too small?)\n"));
        }
    }

    return TRUE;
}

/*
 * TryHeadlessContext - Create a headless GL context without a window
 *
 * Uses GLA_Headless to create a GL context without needing a window.
 * CreatePipeV handles the Gallium driver lookup via LockPubScreen fallback,
 * so no friendBM is needed.
 *
 * If the compositor has already published its master context, we share
 * with it via GLA_ShareContext. Otherwise we create a standalone context.
 *
 * Returns the new context, or NULL on failure.
 */
GLAContext TryHeadlessContext(void)
{
    struct GLCompositorSemaphore *comp_sem;
    GLAContext ctx;
    APTR compositor_ctx = NULL;

    /* Check if compositor's master context is available for sharing */
    Forbid();
    comp_sem = (struct GLCompositorSemaphore *)FindSemaphore(GLCOMPOSITOR_SEMAPHORE_NAME);
    Permit();

    if (comp_sem)
    {
        ObtainSemaphoreShared(&comp_sem->sem);
        compositor_ctx = comp_sem->master_context;
        ReleaseSemaphore(&comp_sem->sem);
    }

    if (compositor_ctx)
    {
        struct TagItem ctx_tags[] = {
            { GLA_Headless,      GL_TRUE },
            { GLA_BitsPerPixel,  32 },
            { GLA_Width,         1 },
            { GLA_Height,        1 },
            { GLA_NoDepth,       GL_TRUE },
            { GLA_NoStencil,     GL_TRUE },
            { GLA_NoAccum,       GL_TRUE },
            { GLA_ShareContext,  (IPTR)compositor_ctx },
            { TAG_DONE,          0 }
        };

        D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: Sharing with compositor master context @ %p\n", compositor_ctx));
        ctx = glACreateContext(ctx_tags);
    }
    else
    {
        struct TagItem ctx_tags[] = {
            { GLA_Headless,      GL_TRUE },
            { GLA_BitsPerPixel,  32 },
            { GLA_Width,         1 },
            { GLA_Height,        1 },
            { GLA_NoDepth,       GL_TRUE },
            { GLA_NoStencil,     GL_TRUE },
            { GLA_NoAccum,       GL_TRUE },
            { TAG_DONE,          0 }
        };

        D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: Creating standalone headless context (no compositor yet)\n"));
        ctx = glACreateContext(ctx_tags);
    }

    if (!ctx)
    {
        D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: glACreateContext failed\n"));
        return NULL;
    }

    D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: Created headless context @ %p (shared=%d)\n",
          ctx, compositor_ctx ? 1 : 0));
    return ctx;
}

/*
 * OpenGL_PreInitializeShaders - Initialize shaders during library init
 *
 * First tries to discover GLCompositor's master GL context via semaphore
 * and create a shared headless context (no window needed).
 *
 * Falls back to opening a small backdrop window if GLCompositor is not
 * available (e.g. running without compositor, or compositor not yet
 * initialized).
 *
 * The context is kept alive for the library lifetime since GL shader
 * objects are bound to the context that created them.
 */
BOOL OpenGL_PreInitializeShaders(void)
{
    GLAContext preinit_ctx;
    struct Task *this_task;
    IPTR stack_size;

    D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Initializing...\n"));

    /* GL context creation and Mesa internals require significant stack space.
     * If the stack is too small, skip pre-init entirely — shaders will be
     * initialized later when called from a context with sufficient stack. */
    this_task = FindTask(NULL);
    if (this_task) {
        stack_size = (IPTR)this_task->tc_SPUpper - (IPTR)this_task->tc_SPLower;
        if (stack_size < ZUNEGFX_SHADER_SAFESTACK) {
            D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Stack too small (%ld < %ld), deferring\n",
                  (LONG)stack_size, (LONG)ZUNEGFX_SHADER_SAFESTACK));
            return FALSE;
        }
    }

    /* Strategy 1: Use a headless GL context (no window needed) */
    preinit_ctx = TryHeadlessContext();
    if (preinit_ctx)
    {
        g_using_compositor_context = TRUE;
        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Using headless context (no window needed)\n"));
    }
    else
    {
        /* Strategy 2: Fall back to creating a pre-init window */
        struct TagItem ctx_tags[4];

        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Compositor not available, creating pre-init window\n"));

        g_preinit_screen = LockPubScreen(NULL);
        if (!g_preinit_screen) {
            D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Cannot lock public screen\n"));
            return FALSE;
        }

        g_preinit_window = OpenWindowTags(NULL,
            WA_Left, 0,
            WA_Top, 0,
            WA_Width, 64,
            WA_Height, 64,
            WA_Backdrop, TRUE,
            WA_Borderless, TRUE,
            WA_NoCareRefresh, TRUE,
            WA_PubScreen, (IPTR)g_preinit_screen,
            TAG_DONE);

        if (!g_preinit_window) {
            D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Cannot open pre-init window\n"));
            UnlockPubScreen(NULL, g_preinit_screen);
            g_preinit_screen = NULL;
            return FALSE;
        }

        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Pre-init window opened at %p\n", g_preinit_window));

        ctx_tags[0].ti_Tag = GLA_Window;
        ctx_tags[0].ti_Data = (IPTR)g_preinit_window;
        ctx_tags[1].ti_Tag = GLA_DoubleBuf;
        ctx_tags[1].ti_Data = FALSE;
        ctx_tags[2].ti_Tag = GLA_NoStencil;
        ctx_tags[2].ti_Data = TRUE;
        ctx_tags[3].ti_Tag = TAG_DONE;

        preinit_ctx = glACreateContext(ctx_tags);
        if (!preinit_ctx) {
            D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Cannot create GL context\n"));
            CloseWindow(g_preinit_window);
            g_preinit_window = NULL;
            UnlockPubScreen(NULL, g_preinit_screen);
            g_preinit_screen = NULL;
            return FALSE;
        }
    }

    /* Make context current */
    glAMakeCurrent(preinit_ctx);

    D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: GL context created, compiling shaders...\n"));

    /* Load FBO functions */
    OpenGL_LoadFBOFunctions();

    /* Compile shaders - this is the slow part that benefits from pre-init */
    if (OpenGL_InitShaders()) {
        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Shaders compiled successfully\n"));
        g_shaders_available = TRUE;
    } else {
        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Shader compilation failed\n"));
    }

    /* Store this as the master context */
    if (g_opengl_priv) {
        APTR pipe_screen;

        g_opengl_priv->master_context = preinit_ctx;
        g_opengl_priv->master_context_created = TRUE;
        g_opengl_priv->has_shaders = g_shaders_available;

        /* Check if context sharing is supported by getting pipe_screen */
        pipe_screen = glAGetPipeScreen(preinit_ctx);
        g_opengl_priv->shared_contexts_supported = (pipe_screen != NULL);

        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Stored as master context, pipe_screen=%p, sharing=%d\n",
              pipe_screen, g_opengl_priv->shared_contexts_supported));
    }

    /* Unbind our context so we don't interfere with other GL users
     * (e.g. GLCompositor which may already have a context current) */
    glAMakeCurrent(NULL);

    D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Done (compositor_shared=%d)\n",
          g_using_compositor_context));

    return TRUE;
}

/*
 * OpenGL_CleanupPreInit - Clean up pre-init resources
 *
 * Called during library cleanup to close the pre-init window and
 * release the public screen lock.
 */
void OpenGL_CleanupPreInit(void)
{
    D(bug("[ZuneGfx:OpenGL] CleanupPreInit: Cleaning up pre-init resources (compositor_shared=%d)\n",
          g_using_compositor_context));

    /* Note: We don't destroy the GL context here because it's stored as master_context
     * and may be in use. It will be cleaned up when the library is expunged.
     */

    if (g_preinit_window) {
        CloseWindow(g_preinit_window);
        g_preinit_window = NULL;
        D(bug("[ZuneGfx:OpenGL] CleanupPreInit: Pre-init window closed\n"));
    }

    if (g_preinit_screen) {
        UnlockPubScreen(NULL, g_preinit_screen);
        g_preinit_screen = NULL;
        D(bug("[ZuneGfx:OpenGL] CleanupPreInit: Public screen unlocked\n"));
    }

    g_using_compositor_context = FALSE;
}

/*
 * OpenGL_GetMasterContext - Get the master GL context for sharing
 *
 * Public function - can be called from outside the backend.
 */
APTR OpenGL_GetMasterContext(void)
{
    D(bug("[ZuneGfx:OpenGL] GetMasterContext: g_opengl_priv=%p\n", g_opengl_priv));
    
    if (!g_opengl_priv) {
        D(bug("[ZuneGfx:OpenGL] GetMasterContext: no g_opengl_priv, returning NULL\n"));
        return NULL;
    }
    
    D(bug("[ZuneGfx:OpenGL] GetMasterContext: master_context_created=%d, master_context=%p\n",
          g_opengl_priv->master_context_created, g_opengl_priv->master_context));
    
    if (!g_opengl_priv->master_context_created) {
        D(bug("[ZuneGfx:OpenGL] GetMasterContext: master not created, returning NULL\n"));
        return NULL;
    }
    
    return g_opengl_priv->master_context;
}

/*
 * OpenGL_EnsureMasterContext - Ensure master context exists
 *
 * Creates the master GL context if it doesn't exist, using the given window.
 * Public function - can be called from outside the backend.
 */
APTR OpenGL_EnsureMasterContext(struct Window *window)
{
    if (!g_opengl_priv) {
        D(bug("[ZuneGfx:OpenGL] EnsureMasterContext: g_opengl_priv is NULL\n"));
        return NULL;
    }

    /* If master context already exists, return it */
    if (g_opengl_priv->master_context_created && g_opengl_priv->master_context) {
        return g_opengl_priv->master_context;
    }

    /* Create master context */
    if (!window) {
        D(bug("[ZuneGfx:OpenGL] EnsureMasterContext: window is NULL\n"));
        return NULL;
    }

    if (!OpenGL_CreateMasterContext(window)) {
        D(bug("[ZuneGfx:OpenGL] EnsureMasterContext: CreateMasterContext failed\n"));
        return NULL;
    }

    return g_opengl_priv->master_context;
}

/*
 * OpenGL_CreateWindowContext - Create a GL context for a window
 *
 * If a master context exists, the new context will share resources with it
 * via GLA_ShareContext. This enables efficient switching between windows
 * using glAMakeCurrent() instead of the crash-prone glASetRast().
 */
OpenGLWindowContext *OpenGL_CreateWindowContext(struct Window *window)
{
    OpenGLWindowContext *ctx;
    struct TagItem tags[12];  /* Extra space for GLA_ShareContext */
    WORD tag_idx = 0;
    BOOL use_shared_context = FALSE;

    if (!window || !GLBase) {
        return NULL;
    }

    /* Allocate context structure */
    ctx = AllocVec(sizeof(OpenGLWindowContext), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ctx) {
        return NULL;
    }

    /*
     * Check if we should use shared context mode.
     * If master context exists and sharing is supported, create this context
     * with GLA_ShareContext to enable resource sharing.
     */
    if (g_opengl_priv && g_opengl_priv->master_context_created &&
        g_opengl_priv->master_context && g_opengl_priv->shared_contexts_supported) {
        use_shared_context = TRUE;
    }

    /* Set up tags for context creation */
    tags[tag_idx].ti_Tag = GLA_Window;
    tags[tag_idx].ti_Data = (IPTR)window;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Left;
    tags[tag_idx].ti_Data = window->BorderLeft;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Top;
    tags[tag_idx].ti_Data = window->BorderTop;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Right;
    tags[tag_idx].ti_Data = window->BorderRight;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Bottom;
    tags[tag_idx].ti_Data = window->BorderBottom;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoDepth;
    tags[tag_idx].ti_Data = GL_TRUE;  /* No depth buffer for 2D */
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoStencil;
    tags[tag_idx].ti_Data = GL_TRUE;  /* No stencil buffer for 2D */
    tag_idx++;

    /* Add GLA_ShareContext if master context is available */
    if (use_shared_context) {
        tags[tag_idx].ti_Tag = GLA_ShareContext;
        tags[tag_idx].ti_Data = (IPTR)g_opengl_priv->master_context;
        tag_idx++;
    }

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create GL context */
    ctx->gl_context = glACreateContext(tags);
    if (!ctx->gl_context) {
        /* If shared context failed, try again without sharing */
        if (use_shared_context) {
            use_shared_context = FALSE;

            /* Rebuild tags without GLA_ShareContext */
            tag_idx = 0;
            tags[tag_idx].ti_Tag = GLA_Window;
            tags[tag_idx].ti_Data = (IPTR)window;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Left;
            tags[tag_idx].ti_Data = window->BorderLeft;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Top;
            tags[tag_idx].ti_Data = window->BorderTop;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Right;
            tags[tag_idx].ti_Data = window->BorderRight;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Bottom;
            tags[tag_idx].ti_Data = window->BorderBottom;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_NoDepth;
            tags[tag_idx].ti_Data = GL_TRUE;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_NoStencil;
            tags[tag_idx].ti_Data = GL_TRUE;
            tag_idx++;
            tags[tag_idx].ti_Tag = TAG_DONE;
            tags[tag_idx].ti_Data = 0;

            ctx->gl_context = glACreateContext(tags);
            if (!ctx->gl_context) {
                FreeVec(ctx);
                return NULL;
            }
        } else {
            FreeVec(ctx);
            return NULL;
        }
    }

    /* Verify sharing worked by comparing pipe_screen */
    if (use_shared_context) {
        APTR ctx_pipe_screen = glAGetPipeScreen((GLAContext)ctx->gl_context);
        APTR master_pipe_screen = glAGetPipeScreen((GLAContext)g_opengl_priv->master_context);

        ctx->uses_shared_context = (ctx_pipe_screen && master_pipe_screen &&
                                    ctx_pipe_screen == master_pipe_screen);
    } else {
        ctx->uses_shared_context = FALSE;
    }

    /* Fill in context data */
    ctx->window = window;
    ctx->context_valid = TRUE;
    ctx->shaders_initialized = FALSE;
    ctx->width = window->Width - window->BorderLeft - window->BorderRight;
    ctx->height = window->Height - window->BorderTop - window->BorderBottom;
    ctx->next = NULL;

    /* Add to linked list */
    if (g_opengl_priv) {
        ctx->next = g_opengl_priv->window_contexts;
        g_opengl_priv->window_contexts = ctx;
        g_opengl_priv->contexts_created++;
    }

    return ctx;
}

/*
 * OpenGL_DestroyWindowContext - Destroy a window's GL context
 *
 * When using shared contexts, this handles the reference counting properly.
 * The master context is only destroyed when all window contexts are gone.
 */
void OpenGL_DestroyWindowContext(OpenGLWindowContext *ctx)
{
    OpenGLWindowContext **prev;
    BOOL was_shared;

    if (!ctx) return;

    was_shared = ctx->uses_shared_context;

    /* Remove from linked list */
    if (g_opengl_priv) {
        prev = &g_opengl_priv->window_contexts;
        while (*prev) {
            if (*prev == ctx) {
                *prev = ctx->next;
                break;
            }
            prev = &(*prev)->next;
        }

        if (g_opengl_priv->current_context == ctx) {
            g_opengl_priv->current_context = NULL;
        }
    }

    /* Destroy GL context */
    if (ctx->gl_context) {
        glADestroyContext((GLAContext)ctx->gl_context);
    }

    FreeVec(ctx);

    /*
     * Check if we should destroy the master context.
     * Only destroy it when there are no more window contexts using it.
     */
    if (was_shared && g_opengl_priv && g_opengl_priv->master_context_created) {
        /* Check if any remaining contexts are using shared resources */
        BOOL has_shared_contexts = FALSE;
        OpenGLWindowContext *remaining = g_opengl_priv->window_contexts;

        while (remaining) {
            if (remaining->uses_shared_context) {
                has_shared_contexts = TRUE;
                break;
            }
            remaining = remaining->next;
        }

        if (!has_shared_contexts) {
            if (g_opengl_priv->master_context) {
                glADestroyContext((GLAContext)g_opengl_priv->master_context);
                g_opengl_priv->master_context = NULL;
            }
            g_opengl_priv->master_context_created = FALSE;
            g_opengl_priv->shared_contexts_supported = FALSE;
        }
    }
}

/*
 * OpenGL_FindWindowContext - Find the GL context for a window
 */
OpenGLWindowContext *OpenGL_FindWindowContext(struct Window *window)
{
    OpenGLWindowContext *ctx;

    if (!window || !g_opengl_priv) {
        return NULL;
    }

    ctx = g_opengl_priv->window_contexts;
    while (ctx) {
        if (ctx->window == window) {
            return ctx;
        }
        ctx = ctx->next;
    }

    return NULL;
}

/*
 * OpenGL_MakeContextCurrent - Make a window context current
 */
BOOL OpenGL_MakeContextCurrent(OpenGLWindowContext *ctx)
{
    if (!ctx || !ctx->gl_context || !ctx->context_valid) {
        return FALSE;
    }

    /* Skip if already current */
    if (g_opengl_priv && g_opengl_priv->current_context == ctx) {
        return TRUE;
    }

    glAMakeCurrent((GLAContext)ctx->gl_context);

    if (g_opengl_priv) {
        g_opengl_priv->current_context = ctx;
        g_opengl_priv->context_switches++;
    }

    /* Initialize shaders if not done yet for this context */
    if (!ctx->shaders_initialized) {
        if (OpenGL_LoadShaderFunctions()) {
            if (OpenGL_CreateRoundedRectShader()) {
                ctx->shaders_initialized = TRUE;
            }
        }
        /* Also load FBO functions */
        OpenGL_LoadFBOFunctions();
    }

    return TRUE;
}

/*
 * OpenGL_EnsureGlobalContext - Ensure the global GL context exists
 *
 * Creates or validates the global GL context. If master context exists,
 * the global context will share resources with it via GLA_ShareContext.
 *
 * Returns TRUE if global context is available.
 */
BOOL OpenGL_EnsureGlobalContext(struct Window *window)
{
    struct TagItem tags[12];  /* Extra space for GLA_ShareContext */
    WORD tag_idx = 0;
    GLAContext gl_ctx;
    BOOL use_shared_context = FALSE;

    if (!g_opengl_priv || !GLBase) {
        return FALSE;
    }

    /* If context already exists, we're good */
    if (g_opengl_priv->context_created && g_opengl_priv->gl_context) {
        return TRUE;
    }

    /* Need a window to create the initial context */
    if (!window) {
        return FALSE;
    }

    /*
     * NEW: Create master context first if it doesn't exist.
     * This enables shared context mode for all subsequent contexts.
     * The master context is used for resource sharing (textures, FBOs, etc.)
     */
    if (!g_opengl_priv->master_context_created) {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Creating master context first\n"));
        if (OpenGL_CreateMasterContext(window)) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Master context created, shared_contexts_supported=%d\n",
                  g_opengl_priv->shared_contexts_supported));
            use_shared_context = g_opengl_priv->shared_contexts_supported;

            /*
             * If sharing is NOT supported (e.g. SoftPipe), reuse the master
             * context as the global context instead of creating a second one
             * for the same window — SoftPipe cannot create two contexts
             * targeting the same window.
             */
            if (!use_shared_context) {
                D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Promoting master context to global (no sharing support)\n"));
                g_opengl_priv->gl_context = g_opengl_priv->master_context;
                g_opengl_priv->context_created = TRUE;
                g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
                g_opengl_priv->current_window = window;
                g_opengl_priv->current_board = NULL;
                g_opengl_priv->current_width = window->Width - window->BorderLeft - window->BorderRight;
                g_opengl_priv->current_height = window->Height - window->BorderTop - window->BorderBottom;
                glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);
                OpenGL_SetupOrthoProjection(g_opengl_priv->current_width, g_opengl_priv->current_height);
                D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Global context = master %p\n",
                      g_opengl_priv->gl_context));
                return TRUE;
            }
        } else {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Master context creation failed, continuing without sharing\n"));
        }
    } else if (g_opengl_priv->shared_contexts_supported && g_opengl_priv->master_context) {
        /* Master context exists and supports sharing */
        use_shared_context = TRUE;
    }

    /* Set up tags for context creation */
    tags[tag_idx].ti_Tag = GLA_Window;
    tags[tag_idx].ti_Data = (IPTR)window;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Left;
    tags[tag_idx].ti_Data = window->BorderLeft;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Top;
    tags[tag_idx].ti_Data = window->BorderTop;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Right;
    tags[tag_idx].ti_Data = window->BorderRight;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_Bottom;
    tags[tag_idx].ti_Data = window->BorderBottom;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoDepth;
    tags[tag_idx].ti_Data = GL_TRUE;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoStencil;
    tags[tag_idx].ti_Data = GL_TRUE;
    tag_idx++;

    /* Add GLA_ShareContext if master context is available */
    if (use_shared_context) {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Using GLA_ShareContext with master %p\n",
              g_opengl_priv->master_context));
        tags[tag_idx].ti_Tag = GLA_ShareContext;
        tags[tag_idx].ti_Data = (IPTR)g_opengl_priv->master_context;
        tag_idx++;
    }

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create the global context (with sharing if master context exists) */
    gl_ctx = glACreateContext(tags);
    if (!gl_ctx) {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: glACreateContext FAILED\n"));
        
        /* If shared context failed, try again without sharing */
        if (use_shared_context) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Retrying without sharing\n"));
            use_shared_context = FALSE;
            
            /* Rebuild tags without GLA_ShareContext */
            tag_idx = 0;
            tags[tag_idx].ti_Tag = GLA_Window;
            tags[tag_idx].ti_Data = (IPTR)window;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Left;
            tags[tag_idx].ti_Data = window->BorderLeft;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Top;
            tags[tag_idx].ti_Data = window->BorderTop;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Right;
            tags[tag_idx].ti_Data = window->BorderRight;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_Bottom;
            tags[tag_idx].ti_Data = window->BorderBottom;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_NoDepth;
            tags[tag_idx].ti_Data = GL_TRUE;
            tag_idx++;
            tags[tag_idx].ti_Tag = GLA_NoStencil;
            tags[tag_idx].ti_Data = GL_TRUE;
            tag_idx++;
            tags[tag_idx].ti_Tag = TAG_DONE;
            tags[tag_idx].ti_Data = 0;
            
            gl_ctx = glACreateContext(tags);
            if (!gl_ctx) {
                D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: glACreateContext FAILED (without sharing)\n"));
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    /* Verify sharing worked by comparing pipe_screens */
    if (use_shared_context) {
        APTR global_pipe_screen = glAGetPipeScreen(gl_ctx);
        APTR master_pipe_screen = glAGetPipeScreen((GLAContext)g_opengl_priv->master_context);
        
        if (global_pipe_screen && master_pipe_screen && global_pipe_screen == master_pipe_screen) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Sharing verified - same pipe_screen %p\n",
                  global_pipe_screen));
        } else {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: WARNING - pipe_screens differ (global=%p, master=%p)\n",
                  global_pipe_screen, master_pipe_screen));
            /* Sharing didn't work as expected, but context was created successfully */
        }
    }

    /* Store in global state */
    g_opengl_priv->gl_context = (APTR)gl_ctx;
    g_opengl_priv->context_created = TRUE;
    g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
    g_opengl_priv->current_window = window;
    g_opengl_priv->current_board = NULL;
    g_opengl_priv->current_width = window->Width - window->BorderLeft - window->BorderRight;
    g_opengl_priv->current_height = window->Height - window->BorderTop - window->BorderBottom;

    /* Make it current */
    glAMakeCurrent(gl_ctx);
    
    D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Global context created %p, shared=%d\n",
          gl_ctx, use_shared_context));

    /* Set up initial orthographic projection */
    OpenGL_SetupOrthoProjection(g_opengl_priv->current_width, g_opengl_priv->current_height);

    /* Initialize shaders now that we have a context */
    if (OpenGL_InitShaders()) {
        if (g_opengl_priv) {
            g_opengl_priv->has_shaders = TRUE;
        }
    }

    /* Initialize FBO functions now that we have a context */
    if (OpenGL_LoadFBOFunctions()) {
        /*
         * Test FBO creation with a small texture to verify it actually works.
         * AROS Mesa/SoftPipe can crash in _mesa_error->fprintf when encountering
         * unsupported formats, so we test with a tiny texture first.
         * Note: SoftPipe is already disabled above, so this test only runs on other renderers.
         */
        GLuint test_fbo = 0, test_tex = 0;
        GLenum test_status;
        BOOL fbo_works = FALSE;

        /* Clear any pending errors */
        while (glGetError() != GL_NO_ERROR) {}

        glGenFramebuffers_ptr(1, &test_fbo);
        glGenTextures(1, &test_tex);

        if (test_fbo && test_tex) {
            glBindFramebuffer_ptr(GL_FRAMEBUFFER, test_fbo);
            glBindTexture(GL_TEXTURE_2D, test_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            /* Try creating a small 16x16 RGBA texture */
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

            if (glGetError() == GL_NO_ERROR) {
                glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, test_tex, 0);
                test_status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);

                if (test_status == GL_FRAMEBUFFER_COMPLETE) {
                    fbo_works = TRUE;
                }
            }

            /* Cleanup test resources */
            glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
            glDeleteTextures(1, &test_tex);
            glDeleteFramebuffers_ptr(1, &test_fbo);
        }

        if (fbo_works && g_opengl_priv) {
            g_opengl_priv->has_framebuffers = TRUE;
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: FBO test PASSED\n"));
        } else {
            g_fbo_available = FALSE;
            if (g_opengl_priv) {
                g_opengl_priv->has_framebuffers = FALSE;
            }
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: FBO test FAILED\n"));
        }
    } else {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: LoadFBOFunctions returned FALSE\n"));
    }

    /* Initialize VBO for efficient quad rendering */
    if (OpenGL_LoadVBOFunctions()) {
        OpenGL_CreateQuadVBO();
    }

    return TRUE;
}

/*
 * OpenGL_SwapBuffers - Swap the OpenGL framebuffer to screen
 *
 * This function is called from zunegfx_drawingboard.c when blitting
 * an OpenGL DrawingBoard to screen. For OpenGL, we don't do traditional
 * bitmap blitting - we just swap the GL framebuffer.
 *
 * This is an exported function that can be called from other modules.
 */
void OpenGL_SwapBuffers(void)
{
    if (!g_opengl_priv || !g_opengl_priv->gl_context) {
        return;
    }

    glFlush();
    glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
}
