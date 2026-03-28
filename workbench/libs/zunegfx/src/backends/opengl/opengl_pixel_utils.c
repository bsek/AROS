/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - OpenGL Backend Pixel Utility Functions

    Helper functions for pixel buffer manipulation, texture upload/download,
    and brush-to-texture conversion.
*/

#include "opengl_intern.h"

/*****************************************************************************/
/* Pixel Buffer Helper Functions                                             */
/*****************************************************************************/

/*
 * OpenGL_FlipPixelBufferY - Flip a pixel buffer vertically in-place
 *
 * OpenGL and screen coordinates have opposite Y directions:
 * - Screen: Y=0 at top
 * - OpenGL: Y=0 at bottom
 *
 * Parameters:
 *   buffer - RGBA pixel buffer to flip (modified in place)
 *   width  - Width in pixels
 *   height - Height in pixels
 *   temp   - Temporary row buffer (must be at least width*4 bytes)
 */
void OpenGL_FlipPixelBufferY(UBYTE *buffer, UWORD width, UWORD height, UBYTE *temp)
{
    ULONG row_size = (ULONG)width * 4;
    UWORD top, bottom;

    for (top = 0, bottom = height - 1; top < bottom; top++, bottom--) {
        UBYTE *top_row = buffer + top * row_size;
        UBYTE *bottom_row = buffer + bottom * row_size;

        CopyMem(top_row, temp, row_size);
        CopyMem(bottom_row, top_row, row_size);
        CopyMem(temp, bottom_row, row_size);
    }
}

/*
 * OpenGL_FlipPixelBufferYCopy - Copy pixel buffer with vertical flip
 *
 * Copies src to dst while flipping vertically.
 */
void OpenGL_FlipPixelBufferYCopy(const UBYTE *src, UBYTE *dst, UWORD width, UWORD height)
{
    ULONG row_size = (ULONG)width * 4;
    UWORD row;

    for (row = 0; row < height; row++) {
        const UBYTE *src_row = src + row * row_size;
        UBYTE *dst_row = dst + (height - 1 - row) * row_size;
        CopyMem((APTR)src_row, dst_row, row_size);
    }
}

/*
 * OpenGL_ReadPixelsToBuffer - Read pixels from current GL framebuffer
 *
 * Allocates a buffer and reads pixels from the current GL framebuffer.
 * The buffer is Y-flipped to match screen coordinates.
 *
 * Returns allocated buffer (caller must FreeVec), or NULL on failure.
 */
UBYTE *OpenGL_ReadPixelsToBuffer(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_y)
{
    UBYTE *buffer;
    ULONG buffer_size = (ULONG)width * height * 4;
    GLenum gl_error;

    D(bug("[ZuneGfx:OpenGL] ReadPixelsToBuffer: pos=%d,%d size=%dx%d flip=%d\n",
          x, y, width, height, flip_y));

    buffer = AllocVec(buffer_size, MEMF_ANY);
    if (!buffer) {
        D(bug("[ZuneGfx:OpenGL] ReadPixelsToBuffer: FAILED to allocate %lu bytes\n", buffer_size));
        return NULL;
    }

    /* Clear any pending GL errors */
    while (glGetError() != GL_NO_ERROR) {}

    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneGfx:OpenGL] ReadPixelsToBuffer: glReadPixels ERROR 0x%04x\n", gl_error));
    }

    /* Sample first few pixels to verify we got data */
    D(bug("[ZuneGfx:OpenGL] ReadPixelsToBuffer: first pixel RGBA = %02x %02x %02x %02x\n",
          buffer[0], buffer[1], buffer[2], buffer[3]));
    if (buffer_size > 16) {
        D(bug("[ZuneGfx:OpenGL] ReadPixelsToBuffer: pixel[4] RGBA = %02x %02x %02x %02x\n",
              buffer[16], buffer[17], buffer[18], buffer[19]));
    }

    if (flip_y) {
        UBYTE *temp = AllocVec(width * 4, MEMF_ANY);
        if (temp) {
            OpenGL_FlipPixelBufferY(buffer, width, height, temp);
            FreeVec(temp);
        }
    }

    D(bug("[ZuneGfx:OpenGL] ReadPixelsToBuffer: success, buffer=%p\n", buffer));
    return buffer;
}

/*
 * OpenGL_ReadRastPortToBuffer - Read pixels from RastPort into buffer
 *
 * Allocates a buffer and reads pixels from a RastPort using CyberGraphics.
 *
 * Returns allocated buffer (caller must FreeVec), or NULL on failure.
 */
UBYTE *OpenGL_ReadRastPortToBuffer(struct RastPort *rp, WORD x, WORD y,
                                          UWORD width, UWORD height, BOOL force_opaque)
{
    UBYTE *buffer;
    ULONG buffer_size = (ULONG)width * height * 4;

    if (!CyberGfxBase || !rp) {
        return NULL;
    }

    buffer = AllocVec(buffer_size, MEMF_ANY);
    if (!buffer) {
        return NULL;
    }

    ReadPixelArray(buffer, 0, 0, width * 4, rp, x, y, width, height, RECTFMT_RGBA);

    if (force_opaque) {
        ULONG i;
        for (i = 3; i < buffer_size; i += 4) {
            buffer[i] = 0xFF;
        }
    }

    return buffer;
}

/*
 * OpenGL_UploadTextureFromBuffer - Create and upload a texture from pixel buffer
 *
 * Returns texture ID, or 0 on failure.
 */
GLuint OpenGL_UploadTextureFromBuffer(const UBYTE *buffer, UWORD width, UWORD height)
{
    GLuint texture;

    glGenTextures(1, &texture);
    if (texture == 0) {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, buffer);

    return texture;
}

/*
 * OpenGL_DrawTexturedQuad - Draw a textured quad
 *
 * Draws a quad with the currently bound texture.
 */
void OpenGL_DrawTexturedQuad(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_texcoord)
{
    GLfloat v0 = flip_texcoord ? 1.0f : 0.0f;
    GLfloat v1 = flip_texcoord ? 0.0f : 1.0f;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, v0); glVertex2i(x, y);
    glTexCoord2f(1.0f, v0); glVertex2i(x + width, y);
    glTexCoord2f(1.0f, v1); glVertex2i(x + width, y + height);
    glTexCoord2f(0.0f, v1); glVertex2i(x, y + height);
    glEnd();
}

/*
 * OpenGL_BrushToTexture - Convert a ZuneBrush to an OpenGL texture
 *
 * Rasterizes any brush type to an RGBA buffer and uploads it as a GL texture.
 * Supports: TEXTURE, DATATYPE, LINEAR_GRADIENT, RADIAL_GRADIENT, PATTERN, PEN, SOLID.
 *
 * Returns texture ID, or 0 on failure. Caller must delete the texture.
 */
GLuint OpenGL_BrushToTexture(struct RenderContext *rctx, struct ZuneBrush *brush,
                                    WORD x, WORD y, UWORD width, UWORD height)
{
    UBYTE *buffer;
    GLuint texture;
    ULONG row_bytes;
    UWORD px, py;

    if (!brush || width == 0 || height == 0) {
        return 0;
    }

    row_bytes = (ULONG)width * 4;
    buffer = AllocVec(row_bytes * height, MEMF_PUBLIC);
    if (!buffer) {
        return 0;
    }

    switch (brush->type) {
    case ZUNE_BRUSH_TYPE_SOLID: {
        /* Solid color - fill entire buffer with same color */
        ULONG color = brush->data.solid.color;
        UBYTE r = (color >> 16) & 0xFF;
        UBYTE g = (color >> 8) & 0xFF;
        UBYTE b = color & 0xFF;
        UBYTE a = (color >> 24) & 0xFF;

        UBYTE *dst = buffer;
        for (py = 0; py < height; py++) {
            for (px = 0; px < width; px++) {
                *dst++ = r;
                *dst++ = g;
                *dst++ = b;
                *dst++ = a;
            }
        }
        break;
    }

    case ZUNE_BRUSH_TYPE_PEN: {
        /* PEN - convert pen to color using colormap */
        ULONG rgb[3];
        UBYTE r, g, b, a = 0xFF;

        if (rctx && rctx->colormap) {
            GetRGB32(rctx->colormap, brush->data.pen.pen, 1, rgb);
            r = rgb[0] >> 24;
            g = rgb[1] >> 24;
            b = rgb[2] >> 24;
        } else {
            /* Fallback - use pen as gray value */
            r = g = b = (UBYTE)(brush->data.pen.pen & 0xFF);
        }

        UBYTE *dst = buffer;
        for (py = 0; py < height; py++) {
            for (px = 0; px < width; px++) {
                *dst++ = r;
                *dst++ = g;
                *dst++ = b;
                *dst++ = a;
            }
        }
        break;
    }

    case ZUNE_BRUSH_TYPE_TEXTURE:
    case ZUNE_BRUSH_TYPE_DATATYPE: {
        /* TEXTURE/DATATYPE - copy from ZuneTexture with wrapping */
        struct ZuneTexture *tex = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                                      ? brush->data.texture.texture
                                      : brush->data.datatype.texture;
        struct ZuneRect src = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                                  ? brush->data.texture.source
                                  : brush->data.datatype.source;
        enum ZuneBrushWrapMode wrap_u = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                                            ? brush->data.texture.wrap_u
                                            : brush->data.datatype.wrap_u;
        enum ZuneBrushWrapMode wrap_v = (brush->type == ZUNE_BRUSH_TYPE_TEXTURE)
                                            ? brush->data.texture.wrap_v
                                            : brush->data.datatype.wrap_v;

        if (!tex || !tex->pixel_data || !tex->valid) {
            FreeVec(buffer);
            return 0;
        }

        UWORD src_w = src.width ? src.width : tex->width;
        UWORD src_h = src.height ? src.height : tex->height;
        ULONG *src_pixels = (ULONG *)tex->pixel_data;
        ULONG src_pitch = tex->pitch / 4;

        UBYTE *dst = buffer;
        for (py = 0; py < height; py++) {
            for (px = 0; px < width; px++) {
                WORD tex_x = px;
                WORD tex_y = py;

                /* Apply wrapping */
                switch (wrap_u) {
                case ZUNE_BRUSH_WRAP_REPEAT:
                    tex_x = tex_x % src_w;
                    if (tex_x < 0) tex_x += src_w;
                    break;
                case ZUNE_BRUSH_WRAP_MIRROR: {
                    WORD doubled = src_w * 2;
                    tex_x = tex_x % doubled;
                    if (tex_x < 0) tex_x += doubled;
                    if (tex_x >= src_w) tex_x = (doubled - 1) - tex_x;
                    break;
                }
                default: /* CLAMP */
                    if (tex_x < 0) tex_x = 0;
                    if (tex_x >= src_w) tex_x = src_w - 1;
                    break;
                }

                switch (wrap_v) {
                case ZUNE_BRUSH_WRAP_REPEAT:
                    tex_y = tex_y % src_h;
                    if (tex_y < 0) tex_y += src_h;
                    break;
                case ZUNE_BRUSH_WRAP_MIRROR: {
                    WORD doubled = src_h * 2;
                    tex_y = tex_y % doubled;
                    if (tex_y < 0) tex_y += doubled;
                    if (tex_y >= src_h) tex_y = (doubled - 1) - tex_y;
                    break;
                }
                default: /* CLAMP */
                    if (tex_y < 0) tex_y = 0;
                    if (tex_y >= src_h) tex_y = src_h - 1;
                    break;
                }

                /* Add source offset */
                tex_x += src.x;
                tex_y += src.y;

                /* Sample pixel */
                ULONG pixel = src_pixels[tex_y * src_pitch + tex_x];
                *dst++ = (pixel >> 16) & 0xFF; /* R */
                *dst++ = (pixel >> 8) & 0xFF;  /* G */
                *dst++ = pixel & 0xFF;         /* B */
                *dst++ = (pixel >> 24) & 0xFF; /* A */
            }
        }
        break;
    }

    case ZUNE_BRUSH_TYPE_LINEAR_GRADIENT: {
        /* LINEAR_GRADIENT - rasterize gradient */
        float start_x = x + brush->data.linear.start.x;
        float start_y = y + brush->data.linear.start.y;
        float end_x = x + brush->data.linear.end.x;
        float end_y = y + brush->data.linear.end.y;

        float dx = end_x - start_x;
        float dy = end_y - start_y;
        float length_sq = dx * dx + dy * dy;

        if (length_sq < 0.001f || !brush->data.linear.stops ||
            brush->data.linear.stop_count == 0) {
            /* Degenerate gradient - use first stop color or black */
            ULONG color = (brush->data.linear.stops && brush->data.linear.stop_count > 0)
                              ? brush->data.linear.stops[0].color : 0xFF000000;
            UBYTE r = (color >> 16) & 0xFF;
            UBYTE g = (color >> 8) & 0xFF;
            UBYTE b = color & 0xFF;
            UBYTE a = (color >> 24) & 0xFF;

            UBYTE *dst = buffer;
            for (py = 0; py < height; py++) {
                for (px = 0; px < width; px++) {
                    *dst++ = r;
                    *dst++ = g;
                    *dst++ = b;
                    *dst++ = a;
                }
            }
        } else {
            const struct ZuneGradientStop *stops = brush->data.linear.stops;
            UWORD stop_count = brush->data.linear.stop_count;

            UBYTE *dst = buffer;
            for (py = 0; py < height; py++) {
                for (px = 0; px < width; px++) {
                    float pixel_x = x + px;
                    float pixel_y = y + py;
                    float t = ((pixel_x - start_x) * dx + (pixel_y - start_y) * dy) / length_sq;

                    /* Clamp t to [0,1] */
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;

                    /* Interpolate gradient stops */
                    UBYTE r, g, b, a;
                    if (t <= stops[0].position) {
                        ULONG c = stops[0].color;
                        a = (c >> 24) & 0xFF;
                        r = (c >> 16) & 0xFF;
                        g = (c >> 8) & 0xFF;
                        b = c & 0xFF;
                    } else if (t >= stops[stop_count - 1].position) {
                        ULONG c = stops[stop_count - 1].color;
                        a = (c >> 24) & 0xFF;
                        r = (c >> 16) & 0xFF;
                        g = (c >> 8) & 0xFF;
                        b = c & 0xFF;
                    } else {
                        /* Find surrounding stops */
                        UWORD i;
                        for (i = 1; i < stop_count; i++) {
                            if (t <= stops[i].position) {
                                float pos0 = stops[i - 1].position;
                                float pos1 = stops[i].position;
                                float local_t = (t - pos0) / (pos1 - pos0);
                                ULONG c0 = stops[i - 1].color;
                                ULONG c1 = stops[i].color;

                                a = (UBYTE)((1.0f - local_t) * ((c0 >> 24) & 0xFF) + local_t * ((c1 >> 24) & 0xFF));
                                r = (UBYTE)((1.0f - local_t) * ((c0 >> 16) & 0xFF) + local_t * ((c1 >> 16) & 0xFF));
                                g = (UBYTE)((1.0f - local_t) * ((c0 >> 8) & 0xFF) + local_t * ((c1 >> 8) & 0xFF));
                                b = (UBYTE)((1.0f - local_t) * (c0 & 0xFF) + local_t * (c1 & 0xFF));
                                break;
                            }
                        }
                    }

                    *dst++ = r;
                    *dst++ = g;
                    *dst++ = b;
                    *dst++ = a;
                }
            }
        }
        break;
    }

    case ZUNE_BRUSH_TYPE_RADIAL_GRADIENT: {
        /* RADIAL_GRADIENT - rasterize radial gradient */
        float center_x = x + brush->data.radial.center.x;
        float center_y = y + brush->data.radial.center.y;
        float radius = (float)brush->data.radial.radius;

        if (radius < 0.001f || !brush->data.radial.stops ||
            brush->data.radial.stop_count == 0) {
            /* Degenerate gradient */
            ULONG color = (brush->data.radial.stops && brush->data.radial.stop_count > 0)
                              ? brush->data.radial.stops[0].color : 0xFF000000;
            UBYTE r = (color >> 16) & 0xFF;
            UBYTE g = (color >> 8) & 0xFF;
            UBYTE b = color & 0xFF;
            UBYTE a = (color >> 24) & 0xFF;

            UBYTE *dst = buffer;
            for (py = 0; py < height; py++) {
                for (px = 0; px < width; px++) {
                    *dst++ = r;
                    *dst++ = g;
                    *dst++ = b;
                    *dst++ = a;
                }
            }
        } else {
            const struct ZuneGradientStop *stops = brush->data.radial.stops;
            UWORD stop_count = brush->data.radial.stop_count;

            UBYTE *dst = buffer;
            for (py = 0; py < height; py++) {
                for (px = 0; px < width; px++) {
                    float pixel_x = x + px;
                    float pixel_y = y + py;
                    float dist_x = pixel_x - center_x;
                    float dist_y = pixel_y - center_y;
                    float dist = sqrtf(dist_x * dist_x + dist_y * dist_y);
                    float t = dist / radius;

                    /* Clamp t to [0,1] */
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;

                    /* Interpolate gradient stops */
                    UBYTE r, g, b, a;
                    if (t <= stops[0].position) {
                        ULONG c = stops[0].color;
                        a = (c >> 24) & 0xFF;
                        r = (c >> 16) & 0xFF;
                        g = (c >> 8) & 0xFF;
                        b = c & 0xFF;
                    } else if (t >= stops[stop_count - 1].position) {
                        ULONG c = stops[stop_count - 1].color;
                        a = (c >> 24) & 0xFF;
                        r = (c >> 16) & 0xFF;
                        g = (c >> 8) & 0xFF;
                        b = c & 0xFF;
                    } else {
                        /* Find surrounding stops */
                        UWORD i;
                        for (i = 1; i < stop_count; i++) {
                            if (t <= stops[i].position) {
                                float pos0 = stops[i - 1].position;
                                float pos1 = stops[i].position;
                                float local_t = (t - pos0) / (pos1 - pos0);
                                ULONG c0 = stops[i - 1].color;
                                ULONG c1 = stops[i].color;

                                a = (UBYTE)((1.0f - local_t) * ((c0 >> 24) & 0xFF) + local_t * ((c1 >> 24) & 0xFF));
                                r = (UBYTE)((1.0f - local_t) * ((c0 >> 16) & 0xFF) + local_t * ((c1 >> 16) & 0xFF));
                                g = (UBYTE)((1.0f - local_t) * ((c0 >> 8) & 0xFF) + local_t * ((c1 >> 8) & 0xFF));
                                b = (UBYTE)((1.0f - local_t) * (c0 & 0xFF) + local_t * (c1 & 0xFF));
                                break;
                            }
                        }
                    }

                    *dst++ = r;
                    *dst++ = g;
                    *dst++ = b;
                    *dst++ = a;
                }
            }
        }
        break;
    }

    case ZUNE_BRUSH_TYPE_PATTERN: {
        /* PATTERN - 16x2 bit pattern with fg/bg colors */
        UBYTE fg_r, fg_g, fg_b, fg_a = 0xFF;
        UBYTE bg_r, bg_g, bg_b, bg_a = 0xFF;

        if (!brush->data.pattern.pattern || !brush->data.pattern.colormap) {
            FreeVec(buffer);
            return 0;
        }

        /* Get colors from pens */
        ULONG rgb[3];
        GetRGB32(brush->data.pattern.colormap, brush->data.pattern.fg_pen, 1, rgb);
        fg_r = rgb[0] >> 24;
        fg_g = rgb[1] >> 24;
        fg_b = rgb[2] >> 24;

        GetRGB32(brush->data.pattern.colormap, brush->data.pattern.bg_pen, 1, rgb);
        bg_r = rgb[0] >> 24;
        bg_g = rgb[1] >> 24;
        bg_b = rgb[2] >> 24;

        UBYTE *dst = buffer;
        for (py = 0; py < height; py++) {
            WORD pat_y = py % 2;
            UWORD pat_row = brush->data.pattern.pattern[pat_y];

            for (px = 0; px < width; px++) {
                WORD pat_x = px % 16;
                BOOL is_fg = (pat_row >> (15 - pat_x)) & 1;

                if (is_fg) {
                    *dst++ = fg_r;
                    *dst++ = fg_g;
                    *dst++ = fg_b;
                    *dst++ = fg_a;
                } else {
                    *dst++ = bg_r;
                    *dst++ = bg_g;
                    *dst++ = bg_b;
                    *dst++ = bg_a;
                }
            }
        }
        break;
    }

    default:
        FreeVec(buffer);
        return 0;
    }

    /* Upload to GL texture */
    texture = OpenGL_UploadTextureFromBuffer(buffer, width, height);
    FreeVec(buffer);

    return texture;
}
