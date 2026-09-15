#include "opengl_intern.h"

/*
 * OpenGL_CreateMasterContext - Create the master GL context
 *
 * The master context is created once against the first window that needs
 * GL rendering. It is published through ZuneGetMasterGLContext().
 *
 * Returns TRUE if the master context was created successfully.
 */
BOOL OpenGL_CreateMasterContext(struct Window *window)
{
    struct TagItem tags[9];
    WORD tag_idx = 0;
    GLAContext master_ctx;

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

    master_ctx = glACreateContext(tags);
    if (!master_ctx) {
        D(bug("[ZuneGfx:OpenGL] CreateMasterContext: glACreateContext FAILED\n"));
        return FALSE;
    }

    g_opengl_priv->master_context = (APTR)master_ctx;
    g_opengl_priv->master_context_created = TRUE;

    /* Make it current to initialize GL state */
    glAMakeCurrent(master_ctx);

    D(bug("[ZuneGfx:OpenGL] CreateMasterContext: master=%p\n", master_ctx));

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
 * OpenGL_PreInitializeShaders - Initialize shaders during library init
 *
 * Opens a small backdrop window and compiles the shaders against its GL
 * context. The context is kept alive for the library lifetime since GL
 * shader objects are bound to the context that created them.
 */
BOOL OpenGL_PreInitializeShaders(void)
{
    GLAContext preinit_ctx;
    struct TagItem ctx_tags[4];
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
        g_opengl_priv->master_context = preinit_ctx;
        g_opengl_priv->master_context_created = TRUE;
        g_opengl_priv->has_shaders = g_shaders_available;

        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Stored as master context %p\n", preinit_ctx));
    }

    /* Unbind our context so we don't interfere with other GL users */
    glAMakeCurrent(NULL);

    D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Done\n"));

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
    D(bug("[ZuneGfx:OpenGL] CleanupPreInit: Cleaning up pre-init resources\n"));

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
}

/*
 * OpenGL_GetMasterContext - Get the master GL context
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
 * OpenGL_EnsureGlobalContext - Ensure the global GL context exists
 *
 * The backend runs a single GL context, retargeted between windows with
 * glASetRast(). The master context created for the first window is promoted
 * to be that global context.
 *
 * Returns TRUE if global context is available.
 */
BOOL OpenGL_EnsureGlobalContext(struct Window *window)
{
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

    if (!g_opengl_priv->master_context_created) {
        if (!OpenGL_CreateMasterContext(window)) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Master context creation failed\n"));
            return FALSE;
        }
    }

    g_opengl_priv->gl_context = g_opengl_priv->master_context;
    g_opengl_priv->context_created = TRUE;
    g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
    g_opengl_priv->current_window = window;
    g_opengl_priv->current_board = NULL;
    g_opengl_priv->current_width = window->Width - window->BorderLeft - window->BorderRight;
    g_opengl_priv->current_height = window->Height - window->BorderTop - window->BorderBottom;

    glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);

    D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Global context = master %p\n",
          g_opengl_priv->gl_context));

    /* Set up initial orthographic projection */
    OpenGL_SetupOrthoProjection(g_opengl_priv->current_width, g_opengl_priv->current_height);

    /* Initialize shaders now that we have a context */
    if (OpenGL_InitShaders()) {
        g_opengl_priv->has_shaders = TRUE;
    }

    /* Initialize FBO functions now that we have a context */
    if (OpenGL_LoadFBOFunctions()) {
        /*
         * Test FBO creation with a small texture to verify it actually works.
         * AROS Mesa/SoftPipe can crash in _mesa_error->fprintf when encountering
         * unsupported formats, so we test with a tiny texture first.
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

        if (fbo_works) {
            g_opengl_priv->has_framebuffers = TRUE;
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: FBO test PASSED\n"));
        } else {
            g_fbo_available = FALSE;
            g_opengl_priv->has_framebuffers = FALSE;
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
