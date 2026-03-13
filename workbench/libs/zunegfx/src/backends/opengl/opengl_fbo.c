#include "opengl_intern.h"

BOOL OpenGL_LoadFBOFunctions(void)
{
    /* Already loaded? */
    if (g_fbo_available) {
        return TRUE;
    }

    /* Try core FBO functions first (OpenGL 3.0+) */
    glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glAGetProcAddress("glGenFramebuffers");
    glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glAGetProcAddress("glDeleteFramebuffers");
    glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glAGetProcAddress("glBindFramebuffer");
    glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glAGetProcAddress("glCheckFramebufferStatus");
    glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glAGetProcAddress("glFramebufferTexture2D");
    glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glAGetProcAddress("glGenRenderbuffers");
    glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glAGetProcAddress("glDeleteRenderbuffers");
    glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glAGetProcAddress("glBindRenderbuffer");
    glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glAGetProcAddress("glRenderbufferStorage");
    glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glAGetProcAddress("glFramebufferRenderbuffer");

    /* If core functions not available, try EXT versions */
    if (!glGenFramebuffers_ptr) {
        glGenFramebuffers_ptr = (PFNGLGENFRAMEBUFFERSPROC)glAGetProcAddress("glGenFramebuffersEXT");
        glDeleteFramebuffers_ptr = (PFNGLDELETEFRAMEBUFFERSPROC)glAGetProcAddress("glDeleteFramebuffersEXT");
        glBindFramebuffer_ptr = (PFNGLBINDFRAMEBUFFERPROC)glAGetProcAddress("glBindFramebufferEXT");
        glCheckFramebufferStatus_ptr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glAGetProcAddress("glCheckFramebufferStatusEXT");
        glFramebufferTexture2D_ptr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glAGetProcAddress("glFramebufferTexture2DEXT");
        glGenRenderbuffers_ptr = (PFNGLGENRENDERBUFFERSPROC)glAGetProcAddress("glGenRenderbuffersEXT");
        glDeleteRenderbuffers_ptr = (PFNGLDELETERENDERBUFFERSPROC)glAGetProcAddress("glDeleteRenderbuffersEXT");
        glBindRenderbuffer_ptr = (PFNGLBINDRENDERBUFFERPROC)glAGetProcAddress("glBindRenderbufferEXT");
        glRenderbufferStorage_ptr = (PFNGLRENDERBUFFERSTORAGEPROC)glAGetProcAddress("glRenderbufferStorageEXT");
        glFramebufferRenderbuffer_ptr = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glAGetProcAddress("glFramebufferRenderbufferEXT");
    }

    /* Check if minimum required functions were loaded */
    if (!glGenFramebuffers_ptr || !glDeleteFramebuffers_ptr ||
        !glBindFramebuffer_ptr || !glCheckFramebufferStatus_ptr ||
        !glFramebufferTexture2D_ptr) {
        D(bug("[ZuneGfx:OpenGL] LoadFBOFunctions: Missing functions - Gen=%p Del=%p Bind=%p Status=%p Tex2D=%p\n",
              glGenFramebuffers_ptr, glDeleteFramebuffers_ptr, glBindFramebuffer_ptr,
              glCheckFramebufferStatus_ptr, glFramebufferTexture2D_ptr));
        g_fbo_available = FALSE;
        return FALSE;
    }

    D(bug("[ZuneGfx:OpenGL] LoadFBOFunctions: All FBO functions loaded OK\n"));
    g_fbo_available = TRUE;
    return TRUE;
}

/*
 * OpenGL_CreateFBO - Create a new Framebuffer Object
 *
 * Creates an FBO with a color texture attachment for off-screen rendering.
 * Returns the FBO data structure, or NULL on failure.
 */
OpenGLFBOData *OpenGL_CreateFBO(UWORD width, UWORD height)
{
    OpenGLFBOData *fbo;
    GLuint fbo_id, texture_id;
    GLenum status;
    GLint max_texture_size = 0;

    /* Validate dimensions */
    if (width == 0 || height == 0) {
        return NULL;
    }

    if (!g_fbo_available || !glGenFramebuffers_ptr) {
        return NULL;
    }

    /* Ensure GL context is current */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);

        /* Verify context is valid */
        if (!glGetString(GL_VERSION)) {
            return NULL;
        }

        /* Check maximum texture size */
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (max_texture_size > 0 && ((GLint)width > max_texture_size || (GLint)height > max_texture_size)) {
            return NULL;
        }
    } else {
        return NULL;
    }

    /* Clear any pending GL errors */
    while (glGetError() != GL_NO_ERROR) {}

    /* Allocate FBO data structure */
    fbo = AllocVec(sizeof(OpenGLFBOData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!fbo) {
        return NULL;
    }

    /* Generate and bind FBO */
    glGenFramebuffers_ptr(1, &fbo_id);
    if (fbo_id == 0) {
        FreeVec(fbo);
        return NULL;
    }
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo_id);

    /* Create color texture attachment */
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    /* Set texture parameters */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    /* Create texture storage */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture_id);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers_ptr(1, &fbo_id);
        FreeVec(fbo);
        return NULL;
    }

    /* Attach texture to FBO */
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    /* Check FBO completeness */
    glFlush();
    status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);

    /* Try GL_DRAW_FRAMEBUFFER if status is 0 */
    if (status == 0) {
        #ifndef GL_DRAW_FRAMEBUFFER
        #define GL_DRAW_FRAMEBUFFER 0x8CA9
        #endif
        status = glCheckFramebufferStatus_ptr(GL_DRAW_FRAMEBUFFER);
    }

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &texture_id);
        glDeleteFramebuffers_ptr(1, &fbo_id);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
        FreeVec(fbo);
        return NULL;
    }

    /* Clear FBO to transparent black */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Unbind FBO */
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

    /* Fill in FBO data */
    fbo->fbo_id = fbo_id;
    fbo->texture_id = texture_id;
    fbo->depth_rb_id = 0;
    fbo->width = width;
    fbo->height = height;
    fbo->valid = TRUE;
    fbo->dirty = FALSE;
    fbo->parent_context = NULL;

    if (g_opengl_priv) {
        g_opengl_priv->fbos_created++;
    }

    return fbo;
}

/*
 * OpenGL_DestroyFBO - Destroy a Framebuffer Object
 */
void OpenGL_DestroyFBO(OpenGLFBOData *fbo)
{
    if (!fbo) return;

    if (fbo->texture_id && glDeleteTextures) {
        glDeleteTextures(1, (GLuint*)&fbo->texture_id);
    }

    if (fbo->depth_rb_id && glDeleteRenderbuffers_ptr) {
        glDeleteRenderbuffers_ptr(1, (GLuint*)&fbo->depth_rb_id);
    }

    if (fbo->fbo_id && glDeleteFramebuffers_ptr) {
        glDeleteFramebuffers_ptr(1, (GLuint*)&fbo->fbo_id);
    }

    FreeVec(fbo);
}

/*
 * OpenGL_BindFBO - Bind an FBO for rendering
 *
 * Also sets up the viewport and projection for the FBO dimensions.
 * Returns TRUE if successful.
 */
BOOL OpenGL_BindFBO(OpenGLFBOData *fbo)
{
    if (!fbo || !fbo->valid || !glBindFramebuffer_ptr) {
        return FALSE;
    }

    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo->fbo_id);

    /* Set viewport for FBO dimensions */
    glViewport(0, 0, fbo->width, fbo->height);

    /* Set up orthographic projection for 2D rendering */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, fbo->width, fbo->height, 0, -1, 1);  /* Y-flipped for screen coords */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /*
     * Mark FBO as dirty - any subsequent GL draw calls will modify its contents.
     * This flag is checked before syncing FBO to bitmap to avoid unnecessary
     * expensive glReadPixels + WritePixelArray operations.
     */
    fbo->dirty = TRUE;

    if (g_opengl_priv) {
        g_opengl_priv->fbo_switches++;
    }

    return TRUE;
}

/*
 * OpenGL_UnbindFBO - Unbind FBO and return to default framebuffer
 */
void OpenGL_UnbindFBO(void)
{
    if (!glBindFramebuffer_ptr) return;

    glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
}

/*****************************************************************************/
/* Zero-Copy FBO Compositing Functions                                       */
/*****************************************************************************/

/*
 * OpenGL_GetFBOAsTexture - Get a DrawingBoard's FBO texture for compositing
 *
 * Returns the OpenGL texture ID of the FBO's color attachment, which can be
 * used directly as a texture input in another rendering operation. This
 * enables zero-copy compositing - the GPU never transfers data to the CPU.
 *
 * Prerequisites:
 * - DrawingBoard must have been rendered to via OpenGL
 * - Caller should call glFlush() on the source context before using texture
 * - For cross-context use, contexts must share resources (GLA_ShareContext)
 *
 * Returns texture ID, or 0 if not available.
 */
GLuint OpenGL_GetFBOAsTexture(struct DrawingBoard *board)
{
    OpenGLFBOData *fbo;

    if (!board || !board->backend_data) {
        return 0;
    }

    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo->valid || fbo->texture_id == 0) {
        return 0;
    }

    /* Ensure all rendering to this FBO is complete */
    glFlush();

    return fbo->texture_id;
}

/*
 * OpenGL_BlitFBOToFBO - Blit from one DrawingBoard's FBO to another
 *
 * This is the key zero-copy compositing function. It renders the source
 * FBO's texture directly onto the destination FBO without any CPU involvement.
 * All data stays on the GPU.
 *
 * Prerequisites:
 * - Both DrawingBoards must have OpenGL FBOs (backend_data != NULL)
 * - For best performance, both should share the same GL context/pipe_screen
 *
 * Parameters:
 *   src - Source DrawingBoard (texture will be read from its FBO)
 *   dst - Destination DrawingBoard (texture will be drawn to its FBO)
 *   src_x, src_y - Source rectangle origin
 *   dst_x, dst_y - Destination rectangle origin
 *   width, height - Size of region to blit
 */
void OpenGL_BlitFBOToFBO(struct DrawingBoard *src, struct DrawingBoard *dst,
                                WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                                UWORD width, UWORD height)
{
    OpenGLFBOData *src_fbo, *dst_fbo;
    GLuint src_texture;
    GLfloat tex_x1, tex_y1, tex_x2, tex_y2;

    if (!src || !dst || !src->backend_data || !dst->backend_data) {
        return;
    }

    if (!g_fbo_available || !glBindFramebuffer_ptr) {
        return;
    }

    src_fbo = (OpenGLFBOData *)src->backend_data;
    dst_fbo = (OpenGLFBOData *)dst->backend_data;

    if (!src_fbo->valid || !dst_fbo->valid) {
        return;
    }

    /* Get source texture */
    src_texture = src_fbo->texture_id;
    if (src_texture == 0) {
        return;
    }

    /* Ensure source rendering is complete */
    glFlush();

    /* Bind destination FBO */
    if (!OpenGL_BindFBO(dst_fbo)) {
        return;
    }

    /* Calculate texture coordinates (normalized 0-1) */
    tex_x1 = (GLfloat)src_x / (GLfloat)src_fbo->width;
    tex_y1 = (GLfloat)src_y / (GLfloat)src_fbo->height;
    tex_x2 = (GLfloat)(src_x + width) / (GLfloat)src_fbo->width;
    tex_y2 = (GLfloat)(src_y + height) / (GLfloat)src_fbo->height;

    /* Flip Y for OpenGL texture coordinates (FBO textures are not flipped) */
    tex_y1 = 1.0f - tex_y1;
    tex_y2 = 1.0f - tex_y2;

    /* Setup state for textured quad rendering */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, src_texture);

    /* Use nearest filtering for pixel-perfect blitting */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Disable shader if active - use fixed function for simple blit */
    if (glUseProgram_ptr) {
        glUseProgram_ptr(0);
    }

    /* Enable blending for alpha compositing */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* White color to pass through texture colors unchanged */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    /* Draw textured quad - all on GPU! */
    glBegin(GL_QUADS);
    glTexCoord2f(tex_x1, tex_y1); glVertex2i(dst_x, dst_y);
    glTexCoord2f(tex_x2, tex_y1); glVertex2i(dst_x + width, dst_y);
    glTexCoord2f(tex_x2, tex_y2); glVertex2i(dst_x + width, dst_y + height);
    glTexCoord2f(tex_x1, tex_y2); glVertex2i(dst_x, dst_y + height);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    /* Mark destination as dirty */
    dst_fbo->dirty = TRUE;

    /* Update global state */
    if (g_opengl_priv) {
        g_opengl_priv->current_target_type = OPENGL_TARGET_DRAWINGBOARD;
        g_opengl_priv->current_board = dst;
        g_opengl_priv->current_window = NULL;
    }
}
