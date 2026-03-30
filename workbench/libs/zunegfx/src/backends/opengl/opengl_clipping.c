/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - OpenGL Backend Clipping

    Implements clipping for the OpenGL backend using glScissor.
    The scissor test clips all subsequent GL drawing to a rectangle
    derived from the clip region's bounding box.
*/

#include "opengl_intern.h"

/*****************************************************************************
 * OpenGLSetupClipping
 *
 * Enables the GL scissor test using the bounding box of the given region.
 * This clips all subsequent OpenGL drawing to that rectangle.
 *
 * Note: glScissor only supports a single rectangle, so we use the region's
 * overall bounding box. For most Zune usage (object bounds clipping) this
 * is sufficient since the clip regions are typically rectangular.
 *****************************************************************************/
BOOL OpenGLSetupClipping(struct RenderContext *rctx, struct Region *region) {
    WORD x, y, width, height;

    if (!rctx || !region) {
        return FALSE;
    }

    if (!g_opengl_priv) {
        return FALSE;
    }

    /* Use the region's bounding box for scissor */
    x = region->bounds.MinX;
    y = region->bounds.MinY;
    width = region->bounds.MaxX - region->bounds.MinX + 1;
    height = region->bounds.MaxY - region->bounds.MinY + 1;

    if (width <= 0 || height <= 0) {
        return FALSE;
    }

    /*
     * GL scissor uses bottom-left origin, but our coordinate system has
     * Y=0 at top (set up by glOrtho in OpenGL_SetupOrthoProjection).
     * Convert: gl_y = viewport_height - (y + height)
     */
    {
        UWORD viewport_height = g_opengl_priv->current_height;
        GLint gl_y = viewport_height - (y + height);

        glEnable(GL_SCISSOR_TEST);
        glScissor(x, gl_y, width, height);
    }

    D(bug("[ZuneGfx:OpenGL] SetupClipping: scissor (%d,%d) %dx%d\n",
          x, y, width, height));

    return TRUE;
}

/*****************************************************************************
 * OpenGLClearClipping
 *
 * Disables the GL scissor test, restoring full-viewport rendering.
 *****************************************************************************/
void OpenGLClearClipping(struct RenderContext *rctx) {
    if (!rctx) {
        return;
    }

    glDisable(GL_SCISSOR_TEST);

    D(bug("[ZuneGfx:OpenGL] ClearClipping: scissor disabled\n"));
}
