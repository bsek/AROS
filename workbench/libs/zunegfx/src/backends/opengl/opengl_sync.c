/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - OpenGL Backend Sync & Blit Functions

    Functions for synchronizing between OpenGL framebuffers and RastPorts,
    and for blitting FBO contents to screen.
*/

#include "opengl_intern.h"

/*****************************************************************************/
/* RastPort Copy Operations                                                  */
/*****************************************************************************/

/*
 * OpenGLCopyFromRastPort - Copy pixels from a RastPort into OpenGL framebuffer
 *
 * This function reads pixels from a source RastPort (e.g., window background)
 * and uploads them into the OpenGL framebuffer as a texture. This is used for
 * proper alpha blending when drawing antialiased content over existing
 * background.
 */
void OpenGLCopyFromRastPort(struct RenderContext *rctx, struct RastPort *src_rp,
                                   WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                                   UWORD width, UWORD height)
{
    UBYTE *pixelbuffer;
    GLuint texture;

    if (!rctx || !src_rp || !g_opengl_priv || !CyberGfxBase) {
        return;
    }

    D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: ENTER, switching to target\n"));

    /* OpenGL_SwitchToTarget -> OpenGL_SwitchToDrawingBoard now handles context switching */
    if (!OpenGL_SwitchToTarget(rctx)) {
        D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: SwitchToTarget FAILED\n"));
        return;
    }
    D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: SwitchToTarget OK, target_type=%d\n",
          g_opengl_priv->current_target_type));

    if (width == 0 || height == 0) {
        return;
    }

    /* Check maximum texture size */
    {
        GLint max_texture_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (max_texture_size > 0 && ((GLint)width > max_texture_size || (GLint)height > max_texture_size)) {
            return;
        }
    }

    /* Read pixels from RastPort with alpha forced to opaque */
    pixelbuffer = OpenGL_ReadRastPortToBuffer(src_rp, src_x, src_y, width, height, TRUE);
    if (!pixelbuffer) {
        D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: ReadRastPortToBuffer FAILED\n"));
        return;
    }

    /* Upload pixel buffer to texture (no Y-flip needed, ortho projection handles it) */
    texture = OpenGL_UploadTextureFromBuffer(pixelbuffer, width, height);
    FreeVec(pixelbuffer);

    if (texture == 0) {
        D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: UploadTextureFromBuffer FAILED\n"));
        return;
    }
    D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: Texture created, id=%u\n", texture));

    /* Draw texture to framebuffer (replace, not blend) */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);  /* Explicitly bind the texture! */
    glDisable(GL_BLEND);

    if (glUseProgram_ptr) {
        glUseProgram_ptr(0);
    }

    /* Set up viewport and projection to match FBO dimensions */
    if (rctx->target_board) {
        struct DrawingBoard *board = rctx->target_board;
        OpenGLFBOData *fbo = (OpenGLFBOData *)board->backend_data;

        /* Re-bind FBO (glAMakeCurrent may have reset the binding) */
        if (fbo && fbo->valid && glBindFramebuffer_ptr) {
            glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo->fbo_id);
        }

        glViewport(0, 0, board->width, board->height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, board->width, board->height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    OpenGL_DrawTexturedQuad(dst_x, dst_y, width, height, FALSE);

    glFlush();
    glFinish();

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glDeleteTextures(1, &texture);
}

/*****************************************************************************/
/* Direct RastPort Blitting                                                  */
/*****************************************************************************/

/*
 * OpenGL_BlitToRastPortDirect - Blit GL framebuffer to RastPort using glASetRast
 */
void OpenGL_BlitToRastPortDirect(struct RastPort *dst_rp, WORD dst_x, WORD dst_y,
                                 UWORD width, UWORD height)
{
    struct TagItem setrast_tags[8];
    WORD tag_idx = 0;

    if (!g_opengl_priv || !g_opengl_priv->gl_context || !dst_rp) {
        return;
    }

    setrast_tags[tag_idx].ti_Tag = GLA_RastPort;
    setrast_tags[tag_idx].ti_Data = (IPTR)dst_rp;
    tag_idx++;

    setrast_tags[tag_idx].ti_Tag = GLA_Width;
    setrast_tags[tag_idx].ti_Data = width;
    tag_idx++;

    setrast_tags[tag_idx].ti_Tag = GLA_Height;
    setrast_tags[tag_idx].ti_Data = height;
    tag_idx++;

    setrast_tags[tag_idx].ti_Tag = GLA_Left;
    setrast_tags[tag_idx].ti_Data = dst_x;
    tag_idx++;

    setrast_tags[tag_idx].ti_Tag = GLA_Top;
    setrast_tags[tag_idx].ti_Data = dst_y;
    tag_idx++;

    setrast_tags[tag_idx].ti_Tag = TAG_DONE;
    setrast_tags[tag_idx].ti_Data = 0;

    glASetRast((GLAContext)g_opengl_priv->gl_context, setrast_tags);

    glFlush();
    glASwapBuffers((GLAContext)g_opengl_priv->gl_context);

    g_opengl_priv->setrast_calls++;
}

/*****************************************************************************/
/* FBO-to-RastPort Blitting                                                  */
/*****************************************************************************/

/*
 * OpenGL_BlitFBOToRastPort - Blit FBO contents to a RastPort
 *
 * Reads pixels from an FBO using glReadPixels and writes them to the
 * destination RastPort using WritePixelArray.
 */
void OpenGL_BlitFBOToRastPort(struct DrawingBoard *board, struct RastPort *dst_rp,
                              WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                              UWORD width, UWORD height)
{
    OpenGLFBOData *fbo;
    UBYTE *pixelbuffer;

    D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: board=%p, dst_rp=%p, src=%d,%d dst=%d,%d %dx%d\n",
          board, dst_rp, src_x, src_y, dst_x, dst_y, width, height));

    if (!board || !board->backend_data || !dst_rp) {
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: invalid params\n"));
        return;
    }

    if (!g_fbo_available || !glBindFramebuffer_ptr || !CyberGfxBase) {
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: FBO not available\n"));
        return;
    }

    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo->valid) {
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: FBO not valid\n"));
        return;
    }

    D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: fbo_id=%u, fbo size=%dx%d\n",
          fbo->fbo_id, fbo->width, fbo->height));

    /*
     * Make the correct GL context current for FBO access.
     * FBOs are NOT shared between GL contexts in Mesa.
     */
    glFlush();
    glFinish();

    if (g_opengl_priv && g_opengl_priv->gl_context) {
        GLAContext current_ctx = glAGetCurrentContext();
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: current ctx=%p, global ctx=%p\n",
              current_ctx, g_opengl_priv->gl_context));
        if (current_ctx != (GLAContext)g_opengl_priv->gl_context) {
            D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: switching to global context for FBO access\n"));
            glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);
        }
    } else {
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: no GL context!\n"));
        return;
    }

    /* Clamp dimensions to FBO size */
    if (src_x < 0) { dst_x -= src_x; width += src_x; src_x = 0; }
    if (src_y < 0) { dst_y -= src_y; height += src_y; src_y = 0; }
    if (src_x + width > fbo->width) width = fbo->width - src_x;
    if (src_y + height > fbo->height) height = fbo->height - src_y;

    if (width <= 0 || height <= 0) {
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: invalid dimensions after clamping\n"));
        return;
    }

    /* Bind the FBO for reading */
    D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: binding FBO %u for reading\n", fbo->fbo_id));
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo->fbo_id);
    glFlush();
    glFinish();

    /* Read pixels from FBO (Y-flipped for screen coordinates) */
    D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: reading pixels at %d,%d size %dx%d\n",
          src_x, fbo->height - src_y - height, width, height));
    pixelbuffer = OpenGL_ReadPixelsToBuffer(src_x, fbo->height - src_y - height, width, height, TRUE);

    /* Unbind FBO and invalidate state */
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

    if (g_opengl_priv) {
        g_opengl_priv->current_target_type = OPENGL_TARGET_NONE;
        g_opengl_priv->current_board = NULL;
        g_opengl_priv->current_window = NULL;
    }

    if (!pixelbuffer) {
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: ReadPixelsToBuffer FAILED!\n"));
        return;
    }

    D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: writing to RastPort at %d,%d\n", dst_x, dst_y));

    /* Write to destination RastPort */
    WritePixelArray(pixelbuffer, 0, 0, width * 4,
                    dst_rp, dst_x, dst_y,
                    width, height, RECTFMT_RGBA);

    FreeVec(pixelbuffer);
}

/*****************************************************************************/
/* FBO-to-Bitmap Sync                                                        */
/*****************************************************************************/

/*
 * OpenGL_SyncFBOToBitmap - Sync FBO contents to DrawingBoard's bitmap
 */
BOOL OpenGL_SyncFBOToBitmap(struct RenderContext *rctx)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    if (!rctx) {
        return FALSE;
    }

    board = rctx->target_board;

    if (!board || !board->valid) {
        return FALSE;
    }

    if (!board->backend_data) {
        return FALSE;
    }

    if (!board->rastport || !board->rastport->BitMap) {
        return FALSE;
    }

    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo->valid) {
        return FALSE;
    }

    if (!fbo->dirty) {
        return TRUE;
    }

    OpenGL_BlitFBOToRastPort(board, board->rastport,
                              0, 0, 0, 0,
                              board->width, board->height);

    fbo->dirty = FALSE;

    return TRUE;
}

/*
 * OpenGL_SyncRegionFBOToBitmap - Sync a region of FBO contents to DrawingBoard's bitmap
 */
BOOL OpenGL_SyncRegionFBOToBitmap(struct RenderContext *rctx,
                                         WORD x, WORD y, UWORD width, UWORD height)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: rctx=%p, region=%d,%d %dx%d\n",
          rctx, x, y, width, height));

    if (!rctx) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: rctx is NULL\n"));
        return FALSE;
    }

    board = rctx->target_board;

    if (!board || !board->valid) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: board invalid (board=%p)\n", board));
        return FALSE;
    }

    if (!board->backend_data) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: no backend_data (FBO not created)\n"));
        return FALSE;
    }

    if (!board->rastport || !board->rastport->BitMap) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: no rastport/bitmap\n"));
        return FALSE;
    }

    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo->valid) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: FBO not valid\n"));
        return FALSE;
    }

    D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: fbo=%p, fbo_id=%u, dirty=%d\n",
          fbo, fbo->fbo_id, fbo->dirty));

    if (!fbo->dirty) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: FBO not dirty, skipping sync\n"));
        return TRUE;
    }

    /* Clamp region to FBO bounds */
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > board->width) width = board->width - x;
    if (y + height > board->height) height = board->height - y;

    if (width <= 0 || height <= 0) {
        return TRUE;
    }

    OpenGL_BlitFBOToRastPort(board, board->rastport,
                              x, y, x, y,
                              width, height);

    return TRUE;
}

/*****************************************************************************/
/* RastPort Sync Helpers                                                     */
/*****************************************************************************/

/*
 * OpenGL_SyncFromRastPort - Copy RastPort contents into OpenGL buffer
 */
void OpenGL_SyncFromRastPort(struct RenderContext *rctx)
{
    struct Window *window;
    struct RastPort *rastport;
    UWORD width, height;
    UBYTE *pixelbuffer;
    UBYTE *flipped_buffer;
    WORD x_offset, y_offset;
    GLuint texture;

    if (!rctx || !rctx->target_rastport || !CyberGfxBase || !g_opengl_priv) {
        return;
    }

    rastport = rctx->target_rastport;
    if (!rastport->Layer || !rastport->Layer->Window) {
        return;
    }

    window = (struct Window *)rastport->Layer->Window;
    x_offset = window->BorderLeft;
    y_offset = window->BorderTop;
    width = window->Width - window->BorderLeft - window->BorderRight;
    height = window->Height - window->BorderTop - window->BorderBottom;

    if (width == 0 || height == 0) {
        return;
    }

    /* Check maximum texture size */
    {
        GLint max_texture_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (max_texture_size > 0 && ((GLint)width > max_texture_size || (GLint)height > max_texture_size)) {
            return;
        }
    }

    /* Read pixels from RastPort */
    pixelbuffer = OpenGL_ReadRastPortToBuffer(rastport, x_offset, y_offset, width, height, FALSE);
    if (!pixelbuffer) {
        return;
    }

    /* Allocate buffer for Y-flipped data */
    flipped_buffer = AllocVec((ULONG)width * height * 4, MEMF_ANY);
    if (!flipped_buffer) {
        FreeVec(pixelbuffer);
        return;
    }

    /* Flip vertically for OpenGL */
    OpenGL_FlipPixelBufferYCopy(pixelbuffer, flipped_buffer, width, height);

    /* Upload to texture */
    texture = OpenGL_UploadTextureFromBuffer(flipped_buffer, width, height);
    FreeVec(flipped_buffer);
    FreeVec(pixelbuffer);

    if (texture == 0) {
        return;
    }

    /* Draw the texture as a fullscreen quad */
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    OpenGL_DrawTexturedQuad(0, 0, width, height, TRUE);
    glDisable(GL_TEXTURE_2D);

    glDeleteTextures(1, &texture);
}

void OpenGL_SyncIfNeeded(struct RenderContext *rctx)
{
    if (!g_opengl_priv || !g_opengl_priv->needs_sync) {
        return;
    }

    g_opengl_priv->needs_sync = FALSE;
}

/*
 * OpenGL_FlushIfNotBatching - Flush OpenGL rendering to make it visible
 */
void OpenGL_FlushIfNotBatching(struct RenderContext *rctx)
{
    if (!rctx || rctx->batching_enabled) {
        return;  /* Don't flush during batching */
    }

    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();

        if (g_opengl_priv->current_target_type == OPENGL_TARGET_WINDOW) {
            glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
            /* After swapping, we need to sync again next time */
            g_opengl_priv->needs_sync = TRUE;
        }
    }
}
