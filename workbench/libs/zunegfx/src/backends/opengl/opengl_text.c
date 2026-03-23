/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Text Rendering

    Implements text drawing via a glyph texture atlas approach:
    - On ZuneSetFont: Build a GL texture atlas containing all glyphs
    - On ZuneDrawText: Draw textured quads from the atlas
    - Supports both bitmap (1-bit) and antialiased (8-bit) fonts
    - Atlas is cached per TextFont* for efficient font switching
*/

#include "opengl_intern.h"
#include <exec/lists.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <string.h>

/*****************************************************************************/
/* Glyph Atlas Data Structures                                               */
/*****************************************************************************/

/* Per-glyph metrics and atlas coordinates */
struct GlyphInfo {
    UWORD atlas_x, atlas_y;    /* Position in atlas texture */
    UWORD width, height;       /* Glyph bitmap dimensions */
    WORD  kern;                 /* Kerning offset (tf_CharKern) */
    WORD  space;                /* Advance width (tf_CharSpace) */
};

/* Cached atlas for one TextFont */
struct GlyphAtlas {
    struct Node node;           /* For linked list */
    struct TextFont *font;      /* Font this atlas was built from */
    GLuint texture_id;          /* GL texture containing all glyphs */
    UWORD tex_width, tex_height;/* Atlas texture dimensions (power of 2) */
    struct GlyphInfo glyphs[256]; /* Per-character info */
    UBYTE lo_char, hi_char;     /* Character range */
    BOOL antialias;             /* Font uses antialiased glyphs */
};

/* Global atlas cache */
static struct List g_atlas_cache;
static BOOL g_atlas_cache_initialized = FALSE;

/* Text shader state */
GLuint g_text_program = 0;
static GLuint g_text_vs = 0;
static GLuint g_text_fs = 0;
static GLint g_uniform_text_atlas = -1;
static GLint g_uniform_text_color = -1;
static GLint g_uniform_text_has_bg = -1;
static GLint g_uniform_text_bg_color = -1;

/*****************************************************************************/
/* Text Shader Source                                                         */
/*****************************************************************************/

static const GLchar *g_text_vs_source =
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = gl_MultiTexCoord0.xy;\n"
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "}\n";

static const GLchar *g_text_fs_source =
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_atlas;\n"
    "uniform vec4 u_color;\n"
    "uniform float u_has_bg;\n"
    "uniform vec4 u_bg_color;\n"
    "\n"
    "void main() {\n"
    "    float alpha = texture2D(u_atlas, v_texcoord).a;\n"
    "    if (u_has_bg > 0.5) {\n"
    "        gl_FragColor = mix(u_bg_color, vec4(u_color.rgb, 1.0), alpha);\n"
    "    } else {\n"
    "        if (alpha < 0.01) discard;\n"
    "        gl_FragColor = vec4(u_color.rgb, u_color.a * alpha);\n"
    "    }\n"
    "}\n";

/*****************************************************************************/
/* Atlas Cache Management                                                    */
/*****************************************************************************/

static void InitAtlasCache(void)
{
    if (!g_atlas_cache_initialized) {
        NEWLIST(&g_atlas_cache);
        g_atlas_cache_initialized = TRUE;
    }
}

static struct GlyphAtlas *FindCachedAtlas(struct TextFont *font)
{
    struct GlyphAtlas *atlas;

    InitAtlasCache();

    ForeachNode(&g_atlas_cache, atlas) {
        if (atlas->font == font) {
            return atlas;
        }
    }
    return NULL;
}

static void AddAtlasToCache(struct GlyphAtlas *atlas)
{
    InitAtlasCache();
    AddTail(&g_atlas_cache, &atlas->node);
}

/*****************************************************************************/
/* Power-of-two helper                                                       */
/*****************************************************************************/

static UWORD NextPowerOfTwo(UWORD v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v++;
    return v < 64 ? 64 : v;
}

/*****************************************************************************/
/* Glyph Atlas Building                                                      */
/*****************************************************************************/

/*
 * IsFontAntialiased - Check if a TextFont uses antialiased glyphs
 */
static BOOL IsFontAntialiased(struct TextFont *font)
{
    if (font->tf_Style & FSF_COLORFONT) {
        struct ColorTextFont *ctf = (struct ColorTextFont *)font;
        if ((ctf->ctf_Flags & CT_COLORMASK) == CT_ANTIALIAS) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * BuildGlyphAtlas - Create a GL texture atlas from a TextFont
 *
 * Extracts all glyphs from the font's character data and packs them
 * into a single GL_ALPHA texture.
 *
 * For bitmap fonts: tf_CharData contains packed 1-bit data
 *   - tf_CharLoc[i] encodes: (bit_offset << 16) | bit_width
 *   - tf_Modulo = bytes per scanline of the font bitmap strip
 *
 * For AA fonts: ctf_CharData[0] contains 8-bit alpha data
 *   - Same tf_CharLoc encoding but data is 8-bit per pixel
 *   - Modulo for AA data is tf_Modulo * 8 (8x wider for byte-per-pixel)
 */
static struct GlyphAtlas *BuildGlyphAtlas(struct TextFont *font)
{
    struct GlyphAtlas *atlas;
    UBYTE lo, hi;
    UWORD num_chars;
    UWORD row_x, row_y, row_height;
    UWORD tex_w, tex_h;
    UBYTE *pixels;
    BOOL aa;
    UWORD i;

    atlas = AllocVec(sizeof(struct GlyphAtlas), MEMF_CLEAR);
    if (!atlas) {
        return NULL;
    }

    atlas->font = font;
    lo = font->tf_LoChar;
    hi = font->tf_HiChar;
    atlas->lo_char = lo;
    atlas->hi_char = hi;
    num_chars = hi - lo + 1;
    aa = IsFontAntialiased(font);
    atlas->antialias = aa;

    memset(atlas->glyphs, 0, sizeof(atlas->glyphs));

    /* First pass: calculate atlas dimensions by computing glyph widths */
    ULONG *charLoc = (ULONG *)font->tf_CharLoc;
    WORD *charSpace = (WORD *)font->tf_CharSpace;
    WORD *charKern = (WORD *)font->tf_CharKern;

    row_x = 1;  /* Start with 1px padding */
    row_y = 1;
    row_height = font->tf_YSize;
    tex_w = 256; /* Start estimate */

    for (i = 0; i < num_chars; i++) {
        ULONG loc = charLoc[i];
        UWORD glyph_width = loc & 0xFFFF;

        if (glyph_width == 0) continue;

        if (row_x + glyph_width + 1 > tex_w) {
            /* Need wider or start new row */
            if (tex_w < 2048) {
                tex_w *= 2;
                /* Restart layout */
                row_x = 1;
                row_y = 1;
                i = (UWORD)-1; /* Will be incremented to 0 */
                continue;
            }
            row_x = 1;
            row_y += row_height + 1;
        }
        row_x += glyph_width + 1;
    }

    tex_h = row_y + row_height + 1;
    tex_w = NextPowerOfTwo(tex_w);
    tex_h = NextPowerOfTwo(tex_h);

    atlas->tex_width = tex_w;
    atlas->tex_height = tex_h;

    D(bug("[ZuneGfx:OpenGL] BuildGlyphAtlas: font='%s' size=%d chars=%d atlas=%dx%d aa=%d\n",
          font->tf_Message.mn_Node.ln_Name, font->tf_YSize, num_chars,
          tex_w, tex_h, aa));

    /* Allocate pixel buffer for atlas (GL_ALPHA - 1 byte per pixel) */
    pixels = AllocVec(tex_w * tex_h, MEMF_CLEAR);
    if (!pixels) {
        FreeVec(atlas);
        return NULL;
    }

    /* Second pass: extract glyphs and pack into atlas */
    row_x = 1;
    row_y = 1;

    for (i = 0; i < num_chars; i++) {
        ULONG loc = charLoc[i];
        UWORD bit_offset = loc >> 16;
        UWORD glyph_width = loc & 0xFFFF;
        UWORD ch = lo + i;
        UWORD row;

        if (glyph_width == 0) {
            atlas->glyphs[ch].width = 0;
            atlas->glyphs[ch].height = 0;
            atlas->glyphs[ch].kern = charKern ? charKern[i] : 0;
            atlas->glyphs[ch].space = charSpace ? charSpace[i] : font->tf_XSize;
            continue;
        }

        /* Wrap to next row if needed */
        if (row_x + glyph_width + 1 > tex_w) {
            row_x = 1;
            row_y += row_height + 1;
        }

        /* Record glyph position */
        atlas->glyphs[ch].atlas_x = row_x;
        atlas->glyphs[ch].atlas_y = row_y;
        atlas->glyphs[ch].width = glyph_width;
        atlas->glyphs[ch].height = font->tf_YSize;
        atlas->glyphs[ch].kern = charKern ? charKern[i] : 0;
        atlas->glyphs[ch].space = charSpace ? charSpace[i] : font->tf_XSize;

        /* Extract glyph pixels */
        if (aa) {
            /* Antialiased font: 8-bit per pixel in ctf_CharData[0] */
            struct ColorTextFont *ctf = (struct ColorTextFont *)font;
            UBYTE *aa_data = (UBYTE *)ctf->ctf_CharData[0];
            UWORD aa_modulo = font->tf_Modulo * 8;

            if (aa_data) {
                for (row = 0; row < font->tf_YSize; row++) {
                    UBYTE *src = aa_data + row * aa_modulo + bit_offset;
                    UBYTE *dst = pixels + (row_y + row) * tex_w + row_x;
                    UWORD col;

                    for (col = 0; col < glyph_width; col++) {
                        dst[col] = src[col];
                    }
                }
            }
        } else {
            /* Bitmap font: 1-bit per pixel in tf_CharData */
            UBYTE *bitmap = (UBYTE *)font->tf_CharData;
            UWORD modulo = font->tf_Modulo;

            for (row = 0; row < font->tf_YSize; row++) {
                UBYTE *dst = pixels + (row_y + row) * tex_w + row_x;
                UWORD col;

                for (col = 0; col < glyph_width; col++) {
                    UWORD bit_pos = bit_offset + col;
                    UWORD byte_idx = bit_pos >> 3;
                    UBYTE bit_mask = 0x80 >> (bit_pos & 7);

                    if (bitmap[row * modulo + byte_idx] & bit_mask) {
                        dst[col] = 0xFF;
                    }
                }
            }
        }

        row_x += glyph_width + 1;
    }

    /* Upload atlas to GL texture */
    glGenTextures(1, &atlas->texture_id);
    glBindTexture(GL_TEXTURE_2D, atlas->texture_id);

    if (aa) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tex_w, tex_h, 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    FreeVec(pixels);

    D(bug("[ZuneGfx:OpenGL] BuildGlyphAtlas: Created atlas texture %u (%dx%d)\n",
          atlas->texture_id, tex_w, tex_h));

    return atlas;
}

/*****************************************************************************/
/* Text Shader Initialization                                                */
/*****************************************************************************/

BOOL OpenGL_CreateTextShader(void)
{
    GLint linked;

    if (g_text_program != 0) {
        return TRUE;
    }

    if (!glCreateShader_ptr || !glCreateProgram_ptr) {
        return FALSE;
    }

    D(bug("[ZuneGfx:OpenGL] CreateTextShader: compiling\n"));

    g_text_vs = OpenGL_CompileShader(GL_VERTEX_SHADER, g_text_vs_source);
    if (g_text_vs == 0) {
        return FALSE;
    }

    g_text_fs = OpenGL_CompileShader(GL_FRAGMENT_SHADER, g_text_fs_source);
    if (g_text_fs == 0) {
        if (glDeleteShader_ptr) glDeleteShader_ptr(g_text_vs);
        g_text_vs = 0;
        return FALSE;
    }

    g_text_program = glCreateProgram_ptr();
    if (g_text_program == 0) {
        if (glDeleteShader_ptr) {
            glDeleteShader_ptr(g_text_vs);
            glDeleteShader_ptr(g_text_fs);
        }
        g_text_vs = 0;
        g_text_fs = 0;
        return FALSE;
    }

    glAttachShader_ptr(g_text_program, g_text_vs);
    glAttachShader_ptr(g_text_program, g_text_fs);
    glLinkProgram_ptr(g_text_program);

    linked = GL_FALSE;
    if (glGetProgramiv_ptr) {
        glGetProgramiv_ptr(g_text_program, GL_LINK_STATUS, &linked);
        if (!linked) {
            TEXT log[512];
            if (glGetProgramInfoLog_ptr) {
                log[0] = 0;
                glGetProgramInfoLog_ptr(g_text_program, sizeof(log), NULL, (char *)log);
                D(bug("[ZuneGfx:OpenGL] CreateTextShader: link error: %s\n", log));
            }
            if (glDeleteProgram_ptr) glDeleteProgram_ptr(g_text_program);
            if (glDeleteShader_ptr) {
                glDeleteShader_ptr(g_text_vs);
                glDeleteShader_ptr(g_text_fs);
            }
            g_text_program = 0;
            g_text_vs = 0;
            g_text_fs = 0;
            return FALSE;
        }
    }

    g_uniform_text_atlas = glGetUniformLocation_ptr(g_text_program, "u_atlas");
    g_uniform_text_color = glGetUniformLocation_ptr(g_text_program, "u_color");
    g_uniform_text_has_bg = glGetUniformLocation_ptr(g_text_program, "u_has_bg");
    g_uniform_text_bg_color = glGetUniformLocation_ptr(g_text_program, "u_bg_color");

    D(bug("[ZuneGfx:OpenGL] CreateTextShader: OK program=%u\n", g_text_program));
    return TRUE;
}

void OpenGL_DestroyTextShader(void)
{
    if (g_text_program && glUseProgram_ptr) {
        glUseProgram_ptr(0);
    }

    if (g_text_program && glDetachShader_ptr) {
        if (g_text_vs) glDetachShader_ptr(g_text_program, g_text_vs);
        if (g_text_fs) glDetachShader_ptr(g_text_program, g_text_fs);
    }

    if (g_text_vs && glDeleteShader_ptr) {
        glDeleteShader_ptr(g_text_vs);
        g_text_vs = 0;
    }
    if (g_text_fs && glDeleteShader_ptr) {
        glDeleteShader_ptr(g_text_fs);
        g_text_fs = 0;
    }
    if (g_text_program && glDeleteProgram_ptr) {
        glDeleteProgram_ptr(g_text_program);
        g_text_program = 0;
    }

    g_uniform_text_atlas = -1;
    g_uniform_text_color = -1;
    g_uniform_text_has_bg = -1;
    g_uniform_text_bg_color = -1;
}

/*****************************************************************************/
/* Atlas Cache Cleanup                                                       */
/*****************************************************************************/

void OpenGL_CleanupAtlasCache(void)
{
    struct GlyphAtlas *atlas, *next;

    if (!g_atlas_cache_initialized) return;

    ForeachNodeSafe(&g_atlas_cache, atlas, next) {
        Remove(&atlas->node);
        if (atlas->texture_id) {
            glDeleteTextures(1, &atlas->texture_id);
        }
        FreeVec(atlas);
    }

    NEWLIST(&g_atlas_cache);
}

/*****************************************************************************/
/* OpenGL Font Setup (called when ZuneSetFont dispatches)                    */
/*****************************************************************************/

struct GlyphAtlas *OpenGL_EnsureGlyphAtlas(struct TextFont *font)
{
    struct GlyphAtlas *atlas;

    if (!font) return NULL;

    atlas = FindCachedAtlas(font);
    if (atlas) return atlas;

    atlas = BuildGlyphAtlas(font);
    if (atlas) {
        AddAtlasToCache(atlas);
    }

    return atlas;
}

/*****************************************************************************/
/* OpenGL Text Drawing                                                       */
/*****************************************************************************/

void OpenGLDrawText(struct RenderContext *rctx, WORD x, WORD y,
                    CONST_STRPTR string, UWORD count,
                    struct InternalColor *fg_color,
                    struct InternalColor *bg_color)
{
    struct GlyphAtlas *atlas;
    UWORD i;
    float cur_x;
    float inv_w, inv_h;

    if (!rctx || !string || count == 0 || !fg_color || !rctx->font) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_DrawText(rctx, x, y, string, count, fg_color, bg_color);
        return;
    }

    /* Ensure text shader is ready */
    if (g_text_program == 0) {
        if (!OpenGL_CreateTextShader()) {
            ZuneFallback_DrawText(rctx, x, y, string, count, fg_color, bg_color);
            return;
        }
    }

    /* Ensure glyph atlas is built */
    atlas = OpenGL_EnsureGlyphAtlas(rctx->font);
    if (!atlas || atlas->texture_id == 0) {
        ZuneFallback_DrawText(rctx, x, y, string, count, fg_color, bg_color);
        return;
    }

    OpenGL_SyncIfNeeded(rctx);

    inv_w = 1.0f / (float)atlas->tex_width;
    inv_h = 1.0f / (float)atlas->tex_height;

    /* Enable blending for text alpha */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Bind text shader */
    glUseProgram_ptr(g_text_program);

    /* Set uniforms */
    if (g_uniform_text_color >= 0 && glUniform4f_ptr) {
        glUniform4f_ptr(g_uniform_text_color,
                        fg_color->r / 255.0f,
                        fg_color->g / 255.0f,
                        fg_color->b / 255.0f,
                        fg_color->a / 255.0f);
    }

    if (g_uniform_text_has_bg >= 0 && glUniform1f_ptr) {
        glUniform1f_ptr(g_uniform_text_has_bg, bg_color ? 1.0f : 0.0f);
    }

    if (bg_color && g_uniform_text_bg_color >= 0 && glUniform4f_ptr) {
        glUniform4f_ptr(g_uniform_text_bg_color,
                        bg_color->r / 255.0f,
                        bg_color->g / 255.0f,
                        bg_color->b / 255.0f,
                        bg_color->a / 255.0f);
    }

    /* Bind atlas texture */
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas->texture_id);

    if (g_uniform_text_atlas >= 0 && glUniform1i_ptr) {
        glUniform1i_ptr(g_uniform_text_atlas, 0);
    }

    /* Draw each character as a textured quad.
     * y parameter is at baseline — convert to top of glyph:
     * glyph top = y - baseline, glyph bottom = y - baseline + tf_YSize */
    WORD glyph_top = y - rctx->font->tf_Baseline;
    UWORD glyph_height = rctx->font->tf_YSize;
    cur_x = (float)x;

    glBegin(GL_QUADS);
    for (i = 0; i < count; i++) {
        UBYTE ch = (UBYTE)string[i];
        struct GlyphInfo *glyph = &atlas->glyphs[ch];
        float gx, u0, v0, u1, v1;

        if (glyph->width == 0) {
            /* Character not in font — advance by average width */
            cur_x += rctx->font->tf_XSize;
            continue;
        }

        /* Apply kerning */
        gx = cur_x + glyph->kern;

        /* UV coordinates in atlas */
        u0 = (float)glyph->atlas_x * inv_w;
        v0 = (float)glyph->atlas_y * inv_h;
        u1 = (float)(glyph->atlas_x + glyph->width) * inv_w;
        v1 = (float)(glyph->atlas_y + glyph->height) * inv_h;

        /* Emit quad: glyph_width wide, font height tall */
        glTexCoord2f(u0, v0); glVertex2f(gx, (float)glyph_top);
        glTexCoord2f(u1, v0); glVertex2f(gx + glyph->width, (float)glyph_top);
        glTexCoord2f(u1, v1); glVertex2f(gx + glyph->width, (float)(glyph_top + glyph_height));
        glTexCoord2f(u0, v1); glVertex2f(gx, (float)(glyph_top + glyph_height));

        /* Advance cursor */
        cur_x += glyph->space;
    }
    glEnd();

    /* Restore state */
    glDisable(GL_TEXTURE_2D);
    glUseProgram_ptr(0);
    glDisable(GL_BLEND);

    OpenGL_FlushIfNotBatching(rctx);

    D(bug("[ZuneGfx:OpenGL] DrawText: %d chars at (%d,%d)\n", count, x, y));
}

/*****************************************************************************/
/* OpenGL Polygon Fill                                                       */
/*****************************************************************************/

void OpenGLFillPolygon(struct RenderContext *rctx, struct ZunePoint *points,
                       UWORD count, struct ZuneBrush *brush, BOOL antialias)
{
    UWORD i;

    if (!rctx || !points || count < 3 || !brush) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rctx)) {
        ZuneFallback_FillPolygon(rctx, points, count, brush, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rctx);

    /* Set color from brush */
    if (brush->type == ZUNE_BRUSH_TYPE_SOLID) {
        ULONG color = brush->data.solid.color;
        glColor4ub(
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF,
            (color >> 24) & 0xFF
        );
    } else {
        /* Non-solid brush — fall back */
        ZuneFallback_FillPolygon(rctx, points, count, brush, antialias);
        return;
    }

    /*
     * Use GL_TRIANGLE_FAN for convex polygons.
     * For non-convex polygons this may produce incorrect results,
     * but for Scintilla's use cases (markers, arrows) polygons are convex.
     */
    glBegin(GL_TRIANGLE_FAN);
    for (i = 0; i < count; i++) {
        glVertex2i(points[i].x, points[i].y);
    }
    glEnd();

    if (antialias) {
        /* Draw antialiased outline for smooth edges */
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        glBegin(GL_LINE_LOOP);
        for (i = 0; i < count; i++) {
            glVertex2i(points[i].x, points[i].y);
        }
        glEnd();

        glDisable(GL_LINE_SMOOTH);
    }

    OpenGL_FlushIfNotBatching(rctx);
}
