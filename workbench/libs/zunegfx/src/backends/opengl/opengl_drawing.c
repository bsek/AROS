/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Drawing Primitives

    Drawing operations: pixel, line, rectangle, circle, clear.
    Also includes RenderContext and color management.
*/

#include "opengl_intern.h"

/*****************************************************************************/
/* RenderContext Management                                                   */
/*****************************************************************************/

BOOL OpenGLInitRenderContext(struct RenderContext *rctx)
{
    if (!rctx) {
        return FALSE;
    }

    rctx->backend_context = NULL;

    return TRUE;
}

void OpenGLCleanupRenderContext(struct RenderContext *rctx)
{
    if (!rctx) {
        return;
    }
}

/*****************************************************************************/
/* Color Management                                                          */
/*****************************************************************************/

BOOL OpenGLPrepareColor(struct RenderContext *rctx,
                               struct InternalColor *color)
{
    if (!color) {
        return FALSE;
    }

    color->pen = -1;
    color->pen_allocated = FALSE;

    return TRUE;
}

void OpenGLReleaseColor(struct RenderContext *rctx,
                               struct InternalColor *color)
{
    /* Nothing to release for OpenGL colors */
}

/*****************************************************************************/
/* Drawing Operations                                                        */
/*****************************************************************************/

void OpenGLDrawPixel(struct RenderContext *rctx, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias)
{
    if (!rctx || !color) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_DrawPixel(rctx, x, y, color, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rctx);
    OpenGL_SetColor(color);

    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();

    OpenGL_FlushIfNotBatching(rctx);
}

void OpenGLDrawLine(struct RenderContext *rctx, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias)
{
    if (!rctx || !color) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_DrawLine(rctx, startX, startY, endX, endY, width, color, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rctx);
    OpenGL_SetColor(color);

    /* Set line width */
    if (width > 1) {
        glLineWidth((GLfloat)width);
    }

    /* Enable antialiasing if requested */
    if (antialias) {
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    }

    glBegin(GL_LINES);
    glVertex2i(startX, startY);
    glVertex2i(endX, endY);
    glEnd();

    /* Reset state */
    if (antialias) {
        glDisable(GL_LINE_SMOOTH);
    }

    if (width > 1) {
        glLineWidth(1.0f);
    }

    OpenGL_FlushIfNotBatching(rctx);
}

void OpenGLDrawRectangle(struct RenderContext *rctx, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius, struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias)
{
    if (!rctx) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_DrawRectangle(rctx, x, y, width, height, border_width, corner_radius,
                                   fill_brush, border_color, filled, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rctx);

    /* Clamp corner radius to half of smallest dimension */
    if (corner_radius > width / 2) corner_radius = width / 2;
    if (corner_radius > height / 2) corner_radius = height / 2;

    /* Simple rectangle (no rounded corners) */
    if (corner_radius == 0) {
        /* Handle fill */
        if (filled && fill_brush) {
            if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
                /* Fast path for solid colors - no texture needed */
                ULONG color = fill_brush->data.solid.color;
                glColor4ub(
                    (color >> 16) & 0xFF,
                    (color >> 8) & 0xFF,
                    color & 0xFF,
                    (color >> 24) & 0xFF
                );

                if (g_vbo_available && g_quad_vbo != 0 && glBindBuffer_ptr) {
                    /* VBO-based solid rect rendering */
                    glPushMatrix();
                    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);
                    glScalef((GLfloat)width, (GLfloat)height, 1.0f);

                    glBindBuffer_ptr(GL_ARRAY_BUFFER, g_quad_vbo);
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glVertexPointer(2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)0);
                    glDrawArrays(GL_QUADS, 0, 4);
                    glDisableClientState(GL_VERTEX_ARRAY);
                    glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);

                    glPopMatrix();
                } else {
                    glBegin(GL_QUADS);
                    glVertex2i(x, y);
                    glVertex2i(x + width, y);
                    glVertex2i(x + width, y + height);
                    glVertex2i(x, y + height);
                    glEnd();
                }
            } else {
                /* Non-solid brush - convert to texture and draw textured quad */
                GLuint brush_texture = OpenGL_BrushToTexture(rctx, fill_brush, x, y, width, height);
                if (brush_texture != 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, brush_texture);
                    glColor4ub(255, 255, 255, 255); /* Full brightness, let texture provide color */

                    OpenGL_DrawTexturedQuad(x, y, width, height, FALSE);

                    glDisable(GL_TEXTURE_2D);
                    glDeleteTextures(1, &brush_texture);
                }
            }
        }

        /* Handle border */
        if (border_width > 0 && border_color) {
            OpenGL_SetColor(border_color);

            if (border_width > 1) {
                glLineWidth((GLfloat)border_width);
            }

            glBegin(GL_LINE_LOOP);
            glVertex2i(x, y);
            glVertex2i(x + width - 1, y);
            glVertex2i(x + width - 1, y + height - 1);
            glVertex2i(x, y + height - 1);
            glEnd();

            if (border_width > 1) {
                glLineWidth(1.0f);
            }
        }
    } else {
        /* Rounded rectangle - use shader if available for smooth AA */

        if (g_shaders_available && g_rounded_rect_program && glUseProgram_ptr) {
            float border_r = 0, border_g = 0, border_b = 0, border_a = 0;
            BOOL has_fill = (filled && fill_brush);
            BOOL has_border = (border_width > 0 && border_color);
            BOOL use_textured_shader = (has_fill && fill_brush->type != ZUNE_BRUSH_TYPE_SOLID &&
                                        g_rounded_rect_textured_program != 0);
            GLuint brush_texture = 0;

            /* Extract border color */
            if (has_border) {
                border_r = border_color->r / 255.0f;
                border_g = border_color->g / 255.0f;
                border_b = border_color->b / 255.0f;
                border_a = border_color->a / 255.0f;
            }

            if (use_textured_shader) {
                /* Non-solid brush: convert to texture and use textured shader */
                brush_texture = OpenGL_BrushToTexture(rctx, fill_brush, x, y, width, height);
                if (brush_texture == 0) {
                    has_fill = FALSE;
                    use_textured_shader = FALSE;
                }
            }

            if (use_textured_shader) {
                /* Use textured shader for non-solid brushes */
                glUseProgram_ptr(g_rounded_rect_textured_program);

                glEnable(GL_TEXTURE_2D);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, brush_texture);

                if (g_uniform_tex_rect_size >= 0 && glUniform2f_ptr)
                    glUniform2f_ptr(g_uniform_tex_rect_size, (GLfloat)width, (GLfloat)height);
                if (g_uniform_tex_rect_radius >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_tex_rect_radius, (GLfloat)corner_radius);
                if (g_uniform_tex_fill_texture >= 0 && glUniform1i_ptr)
                    glUniform1i_ptr(g_uniform_tex_fill_texture, 0);
                if (g_uniform_tex_border_color >= 0 && glUniform4f_ptr)
                    glUniform4f_ptr(g_uniform_tex_border_color, border_r, border_g, border_b, border_a);
                if (g_uniform_tex_border_width >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_tex_border_width, (GLfloat)border_width);
                if (g_uniform_tex_has_fill >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_tex_has_fill, has_fill ? 1.0f : 0.0f);
                if (g_uniform_tex_has_border >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_tex_has_border, has_border ? 1.0f : 0.0f);
            } else {
                /* Use solid color shader */
                ULONG fill_color_val = 0;
                float fill_r = 0, fill_g = 0, fill_b = 0, fill_a = 0;

                if (has_fill && fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
                    fill_color_val = fill_brush->data.solid.color;
                    fill_r = ((fill_color_val >> 16) & 0xFF) / 255.0f;
                    fill_g = ((fill_color_val >> 8) & 0xFF) / 255.0f;
                    fill_b = (fill_color_val & 0xFF) / 255.0f;
                    fill_a = ((fill_color_val >> 24) & 0xFF) / 255.0f;
                } else {
                    has_fill = FALSE;
                }

                glUseProgram_ptr(g_rounded_rect_program);

                if (g_uniform_rect_size >= 0 && glUniform2f_ptr)
                    glUniform2f_ptr(g_uniform_rect_size, (GLfloat)width, (GLfloat)height);
                if (g_uniform_rect_radius >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_rect_radius, (GLfloat)corner_radius);
                if (g_uniform_fill_color >= 0 && glUniform4f_ptr)
                    glUniform4f_ptr(g_uniform_fill_color, fill_r, fill_g, fill_b, fill_a);
                if (g_uniform_border_color >= 0 && glUniform4f_ptr)
                    glUniform4f_ptr(g_uniform_border_color, border_r, border_g, border_b, border_a);
                if (g_uniform_border_width >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_border_width, (GLfloat)border_width);
                if (g_uniform_has_fill >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_has_fill, has_fill ? 1.0f : 0.0f);
                if (g_uniform_has_border >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_has_border, has_border ? 1.0f : 0.0f);
            }

            /* Draw the quad using VBO if available, otherwise immediate mode */
            if (g_vbo_available && g_quad_vbo != 0 && glBindBuffer_ptr) {
                glPushMatrix();
                glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);
                glScalef((GLfloat)width, (GLfloat)height, 1.0f);

                glBindBuffer_ptr(GL_ARRAY_BUFFER, g_quad_vbo);

                glEnableClientState(GL_VERTEX_ARRAY);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);

                glVertexPointer(2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)0);
                glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

                glDrawArrays(GL_QUADS, 0, 4);

                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glDisableClientState(GL_VERTEX_ARRAY);

                glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);
                glPopMatrix();
            } else {
                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
                glTexCoord2f(1.0f, 0.0f); glVertex2i(x + width, y);
                glTexCoord2f(1.0f, 1.0f); glVertex2i(x + width, y + height);
                glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + height);
                glEnd();
            }

            /* Cleanup */
            if (use_textured_shader) {
                glDisable(GL_TEXTURE_2D);
                if (brush_texture != 0) {
                    glDeleteTextures(1, &brush_texture);
                }
            }

            glUseProgram_ptr(0);
        } else {
            /*
             * Fallback: Draw rounded rectangle using geometry (no shaders)
             */
            WORD r = corner_radius;

            /* Handle fill */
            if (filled && fill_brush) {
                if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
                    ULONG color = fill_brush->data.solid.color;
                    glColor4ub(
                        (color >> 16) & 0xFF,
                        (color >> 8) & 0xFF,
                        color & 0xFF,
                        (color >> 24) & 0xFF
                    );

                    #define CORNER_SEGMENTS_FILL 16
                    {
                        WORD i;
                        float angle, angle_step;
                        WORD cx = x + width / 2;
                        WORD cy = y + height / 2;

                        angle_step = (3.14159265f / 2.0f) / CORNER_SEGMENTS_FILL;

                        glBegin(GL_TRIANGLE_FAN);
                        glVertex2i(cx, cy);

                        glVertex2i(x + r, y);
                        glVertex2i(x + width - r, y);

                        for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                            angle = (3.14159265f / 2.0f) - i * angle_step;
                            glVertex2f(x + width - r + r * cosf(angle),
                                      y + r - r * sinf(angle));
                        }

                        glVertex2i(x + width, y + r);
                        glVertex2i(x + width, y + height - r);

                        for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                            angle = -i * angle_step;
                            glVertex2f(x + width - r + r * cosf(angle),
                                      y + height - r - r * sinf(angle));
                        }

                        glVertex2i(x + width - r, y + height);
                        glVertex2i(x + r, y + height);

                        for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                            angle = -(3.14159265f / 2.0f) - i * angle_step;
                            glVertex2f(x + r + r * cosf(angle),
                                      y + height - r - r * sinf(angle));
                        }

                        glVertex2i(x, y + height - r);
                        glVertex2i(x, y + r);

                        for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                            angle = 3.14159265f - i * angle_step;
                            glVertex2f(x + r + r * cosf(angle),
                                      y + r - r * sinf(angle));
                        }

                        glVertex2i(x + r, y);
                        glEnd();

                        if (antialias) {
                            glEnable(GL_LINE_SMOOTH);
                            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

                            glBegin(GL_LINE_LOOP);
                            glVertex2i(x + r, y);
                            glVertex2i(x + width - r, y);

                            for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                                angle = (3.14159265f / 2.0f) - i * angle_step;
                                glVertex2f(x + width - r + r * cosf(angle),
                                          y + r - r * sinf(angle));
                            }

                            glVertex2i(x + width, y + r);
                            glVertex2i(x + width, y + height - r);

                            for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                                angle = -i * angle_step;
                                glVertex2f(x + width - r + r * cosf(angle),
                                          y + height - r - r * sinf(angle));
                            }

                            glVertex2i(x + width - r, y + height);
                            glVertex2i(x + r, y + height);

                            for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                                angle = -(3.14159265f / 2.0f) - i * angle_step;
                                glVertex2f(x + r + r * cosf(angle),
                                          y + height - r - r * sinf(angle));
                            }

                            glVertex2i(x, y + height - r);
                            glVertex2i(x, y + r);

                            for (i = 0; i <= CORNER_SEGMENTS_FILL; i++) {
                                angle = 3.14159265f - i * angle_step;
                                glVertex2f(x + r + r * cosf(angle),
                                          y + r - r * sinf(angle));
                            }

                            glEnd();
                            glDisable(GL_LINE_SMOOTH);
                        }
                    }
                    #undef CORNER_SEGMENTS_FILL
                }
            }

            /* Handle border */
            if (border_width > 0 && border_color) {
                #define CORNER_SEGMENTS 16
                WORD i;
                float angle, angle_step;

                OpenGL_SetColor(border_color);

                if (border_width > 1) {
                    glLineWidth((GLfloat)border_width);
                }

                if (antialias) {
                    glEnable(GL_LINE_SMOOTH);
                    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
                }

                angle_step = (3.14159265f / 2.0f) / CORNER_SEGMENTS;

                glBegin(GL_LINE_LOOP);

                /* Top-right corner arc (90 to 0 degrees) */
                for (i = 0; i <= CORNER_SEGMENTS; i++) {
                    angle = (3.14159265f / 2.0f) - i * angle_step;
                    glVertex2f(x + width - r + r * cosf(angle),
                              y + r - r * sinf(angle));
                }

                /* Bottom-right corner arc (0 to -90 degrees) */
                for (i = 0; i <= CORNER_SEGMENTS; i++) {
                    angle = -i * angle_step;
                    glVertex2f(x + width - r + r * cosf(angle),
                              y + height - r - r * sinf(angle));
                }

                /* Bottom-left corner arc (-90 to -180 degrees) */
                for (i = 0; i <= CORNER_SEGMENTS; i++) {
                    angle = -(3.14159265f / 2.0f) - i * angle_step;
                    glVertex2f(x + r + r * cosf(angle),
                              y + height - r - r * sinf(angle));
                }

                /* Top-left corner arc (180 to 90 degrees) */
                for (i = 0; i <= CORNER_SEGMENTS; i++) {
                    angle = 3.14159265f - i * angle_step;
                    glVertex2f(x + r + r * cosf(angle),
                              y + r - r * sinf(angle));
                }

                glEnd();

                if (antialias) {
                    glDisable(GL_LINE_SMOOTH);
                }
                if (border_width > 1) {
                    glLineWidth(1.0f);
                }
                #undef CORNER_SEGMENTS
            }
        }
    }

    OpenGL_FlushIfNotBatching(rctx);
}

void OpenGLDrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y,
                             UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias)
{
    #define CIRCLE_SEGMENTS 64
    WORD i;
    float angle, angle_step;

    if (!rctx) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_DrawCircle(rctx, center_x, center_y, radius, border_width,
                                fill_brush, border_color, filled, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rctx);

    angle_step = 2.0f * 3.14159265f / CIRCLE_SEGMENTS;

    /* Handle fill */
    if (filled && fill_brush) {
        if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
            ULONG color = fill_brush->data.solid.color;
            glColor4ub(
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF,
                (color >> 24) & 0xFF
            );

            glBegin(GL_TRIANGLE_FAN);
            glVertex2i(center_x, center_y);
            for (i = 0; i <= CIRCLE_SEGMENTS; i++) {
                angle = i * angle_step;
                glVertex2f(center_x + radius * cosf(angle),
                          center_y + radius * sinf(angle));
            }
            glEnd();

            if (antialias) {
                glEnable(GL_LINE_SMOOTH);
                glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

                glBegin(GL_LINE_LOOP);
                for (i = 0; i < CIRCLE_SEGMENTS; i++) {
                    angle = i * angle_step;
                    glVertex2f(center_x + radius * cosf(angle),
                              center_y + radius * sinf(angle));
                }
                glEnd();

                glDisable(GL_LINE_SMOOTH);
            }
        }
    }

    /* Handle border/outline */
    if (border_width > 0 && border_color) {
        OpenGL_SetColor(border_color);

        if (border_width > 1) {
            glLineWidth((GLfloat)border_width);
        }

        if (antialias) {
            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        }

        glBegin(GL_LINE_LOOP);
        for (i = 0; i < CIRCLE_SEGMENTS; i++) {
            angle = i * angle_step;
            glVertex2f(center_x + radius * cosf(angle),
                      center_y + radius * sinf(angle));
        }
        glEnd();

        if (antialias) {
            glDisable(GL_LINE_SMOOTH);
        }
        if (border_width > 1) {
            glLineWidth(1.0f);
        }
    }

    OpenGL_FlushIfNotBatching(rctx);
    #undef CIRCLE_SEGMENTS
}

void OpenGLClearRenderContext(struct RenderContext *rctx,
                                  struct InternalColor *color)
{
    if (!rctx || !color) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_ClearRenderContext(rctx, color);
        return;
    }

    /* Clear overwrites everything, no need to sync from RastPort */
    if (g_opengl_priv) {
        g_opengl_priv->needs_sync = FALSE;
    }

    glClearColor(
        color->r / 255.0f,
        color->g / 255.0f,
        color->b / 255.0f,
        color->a / 255.0f
    );

    glClear(GL_COLOR_BUFFER_BIT);

    OpenGL_FlushIfNotBatching(rctx);
}
