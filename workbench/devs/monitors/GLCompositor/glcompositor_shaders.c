/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: OpenGL GPU Compositor - Shader management
*/

#define DEBUG 1
#include <aros/debug.h>

#include <GL/gl.h>
#include <GL/gla.h>

#include "glcompositor_intern.h"

/* ────────────────────────────────────────────────────────────────────────── */
/* Shader Sources                                                            */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Composite Vertex Shader
 * Transforms screen-space pixel coordinates to clip space.
 * a_position: pixel coords (0..screen_width, 0..screen_height)
 * a_texcoord: texture coords (0..1)
 */
static const GLchar *g_composite_vs =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "uniform vec2 u_screen_size;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord;\n"
    "    vec2 ndc = a_position / u_screen_size * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);\n"
    "}\n";

/*
 * Composite Fragment Shader
 * Samples texture and applies global alpha.
 */
static const GLchar *g_composite_fs =
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
    "}\n";

/*
 * Shadow Vertex Shader
 * Expands a unit quad to cover shadow area around window.
 */
static const GLchar *g_shadow_vs =
    "attribute vec2 a_position;\n"
    "varying vec2 v_local_pos;\n"
    "uniform vec2 u_screen_size;\n"
    "uniform vec2 u_window_pos;\n"
    "uniform vec2 u_window_size;\n"
    "uniform vec2 u_shadow_offset;\n"
    "uniform float u_shadow_blur;\n"
    "void main() {\n"
    "    float expand = u_shadow_blur * 2.0;\n"
    "    vec2 size = u_window_size + vec2(expand);\n"
    "    vec2 origin = u_window_pos + u_shadow_offset - vec2(u_shadow_blur);\n"
    "    vec2 pos = origin + a_position * size;\n"
    "    vec2 ndc = pos / u_screen_size * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);\n"
    "    v_local_pos = a_position * size - vec2(u_shadow_blur);\n"
    "}\n";

/*
 * Shadow Fragment Shader (SDF-based)
 * Uses a signed distance field for a rounded rectangle to produce
 * a smooth, soft shadow.
 */
static const GLchar *g_shadow_fs =
    "varying vec2 v_local_pos;\n"
    "uniform vec2 u_window_size;\n"
    "uniform vec4 u_shadow_color;\n"
    "uniform float u_shadow_blur;\n"
    "\n"
    "float sdRoundedRect(vec2 p, vec2 halfSize, float radius) {\n"
    "    vec2 q = abs(p) - halfSize + vec2(radius);\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 center = u_window_size * 0.5;\n"
    "    float dist = sdRoundedRect(v_local_pos - center, center, 6.0);\n"
    "    float shadow = 1.0 - smoothstep(-u_shadow_blur, u_shadow_blur * 0.25, dist);\n"
    "    gl_FragColor = vec4(u_shadow_color.rgb, u_shadow_color.a * shadow);\n"
    "}\n";

/* ────────────────────────────────────────────────────────────────────────── */
/* GL Extension Loading                                                      */
/* ────────────────────────────────────────────────────────────────────────── */

BOOL GLCompositor_LoadExtensions(struct HIDDCompositorData *compdata)
{
    #define LOAD_EXT(name) \
        compdata->gpu.name = (typeof(compdata->gpu.name))glAGetProcAddress((const GLubyte *)#name); \
        if (!compdata->gpu.name) { \
            D(bug("[GLCompositor] Failed to load %s\n", #name)); \
        }

    LOAD_EXT(glCreateShader);
    LOAD_EXT(glShaderSource);
    LOAD_EXT(glCompileShader);
    LOAD_EXT(glCreateProgram);
    LOAD_EXT(glAttachShader);
    LOAD_EXT(glLinkProgram);
    LOAD_EXT(glUseProgram);
    LOAD_EXT(glGetUniformLocation);
    LOAD_EXT(glUniform1i);
    LOAD_EXT(glUniform1f);
    LOAD_EXT(glUniform2f);
    LOAD_EXT(glUniform4f);
    LOAD_EXT(glDeleteShader);
    LOAD_EXT(glDeleteProgram);
    LOAD_EXT(glGetShaderiv);
    LOAD_EXT(glGetProgramiv);
    LOAD_EXT(glGenBuffers);
    LOAD_EXT(glBindBuffer);
    LOAD_EXT(glBufferData);
    LOAD_EXT(glDeleteBuffers);
    LOAD_EXT(glEnableVertexAttribArray);
    LOAD_EXT(glVertexAttribPointer);
    LOAD_EXT(glGetAttribLocation);

    /* Optional */
    compdata->gpu.glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC_)glAGetProcAddress((const GLubyte *)"glBindFramebuffer");
    if (!compdata->gpu.glBindFramebuffer)
        compdata->gpu.glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC_)glAGetProcAddress((const GLubyte *)"glBindFramebufferEXT");

    #undef LOAD_EXT

    /* Check minimum requirements */
    if (!compdata->gpu.glCreateShader || !compdata->gpu.glShaderSource ||
        !compdata->gpu.glCompileShader || !compdata->gpu.glCreateProgram ||
        !compdata->gpu.glAttachShader || !compdata->gpu.glLinkProgram ||
        !compdata->gpu.glUseProgram || !compdata->gpu.glGetUniformLocation)
    {
        D(bug("[GLCompositor] Missing required GL extensions\n"));
        return FALSE;
    }

    D(bug("[GLCompositor] GL extensions loaded successfully\n"));
    return TRUE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Shader Compilation                                                        */
/* ────────────────────────────────────────────────────────────────────────── */

static GLuint CompileShader(struct HIDDCompositorData *compdata, GLenum type, const GLchar *source)
{
    GLuint shader;
    GLint status;

    shader = compdata->gpu.glCreateShader(type);
    if (shader == 0)
        return 0;

    compdata->gpu.glShaderSource(shader, 1, &source, NULL);
    compdata->gpu.glCompileShader(shader);

    if (compdata->gpu.glGetShaderiv)
    {
        compdata->gpu.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status)
        {
            D(bug("[GLCompositor] Shader compile failed (type=%s)\n",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment"));
            compdata->gpu.glDeleteShader(shader);
            return 0;
        }
    }

    return shader;
}

static GLuint LinkProgram(struct HIDDCompositorData *compdata, GLuint vs, GLuint fs)
{
    GLuint prog;
    GLint status;

    prog = compdata->gpu.glCreateProgram();
    if (prog == 0)
        return 0;

    compdata->gpu.glAttachShader(prog, vs);
    compdata->gpu.glAttachShader(prog, fs);
    compdata->gpu.glLinkProgram(prog);

    if (compdata->gpu.glGetProgramiv)
    {
        compdata->gpu.glGetProgramiv(prog, GL_LINK_STATUS, &status);
        if (!status)
        {
            D(bug("[GLCompositor] Program link failed\n"));
            compdata->gpu.glDeleteProgram(prog);
            return 0;
        }
    }

    return prog;
}

BOOL GLCompositor_CompileCompositeShader(struct HIDDCompositorData *compdata)
{
    GLuint vs, fs;

    vs = CompileShader(compdata, GL_VERTEX_SHADER, g_composite_vs);
    if (!vs) return FALSE;

    fs = CompileShader(compdata, GL_FRAGMENT_SHADER, g_composite_fs);
    if (!fs)
    {
        compdata->gpu.glDeleteShader(vs);
        return FALSE;
    }

    compdata->gpu.composite_shader = LinkProgram(compdata, vs, fs);

    compdata->gpu.glDeleteShader(vs);
    compdata->gpu.glDeleteShader(fs);

    if (!compdata->gpu.composite_shader)
        return FALSE;

    /* Get uniform locations */
    compdata->gpu.u_texture = compdata->gpu.glGetUniformLocation(
        compdata->gpu.composite_shader, "u_texture");
    compdata->gpu.u_alpha = compdata->gpu.glGetUniformLocation(
        compdata->gpu.composite_shader, "u_alpha");
    compdata->gpu.u_screen_size = compdata->gpu.glGetUniformLocation(
        compdata->gpu.composite_shader, "u_screen_size");

    D(bug("[GLCompositor] Composite shader compiled (prog=%d, u_texture=%d, u_alpha=%d, u_screen_size=%d)\n",
          compdata->gpu.composite_shader, compdata->gpu.u_texture,
          compdata->gpu.u_alpha, compdata->gpu.u_screen_size));

    return TRUE;
}

BOOL GLCompositor_CompileShadowShader(struct HIDDCompositorData *compdata)
{
    GLuint vs, fs;

    vs = CompileShader(compdata, GL_VERTEX_SHADER, g_shadow_vs);
    if (!vs) return FALSE;

    fs = CompileShader(compdata, GL_FRAGMENT_SHADER, g_shadow_fs);
    if (!fs)
    {
        compdata->gpu.glDeleteShader(vs);
        return FALSE;
    }

    compdata->gpu.shadow_shader = LinkProgram(compdata, vs, fs);

    compdata->gpu.glDeleteShader(vs);
    compdata->gpu.glDeleteShader(fs);

    if (!compdata->gpu.shadow_shader)
        return FALSE;

    /* Get uniform locations */
    compdata->gpu.u_shadow_screen_size = compdata->gpu.glGetUniformLocation(
        compdata->gpu.shadow_shader, "u_screen_size");
    compdata->gpu.u_shadow_window_pos = compdata->gpu.glGetUniformLocation(
        compdata->gpu.shadow_shader, "u_window_pos");
    compdata->gpu.u_shadow_window_size = compdata->gpu.glGetUniformLocation(
        compdata->gpu.shadow_shader, "u_window_size");
    compdata->gpu.u_shadow_offset = compdata->gpu.glGetUniformLocation(
        compdata->gpu.shadow_shader, "u_shadow_offset");
    compdata->gpu.u_shadow_color = compdata->gpu.glGetUniformLocation(
        compdata->gpu.shadow_shader, "u_shadow_color");
    compdata->gpu.u_shadow_blur = compdata->gpu.glGetUniformLocation(
        compdata->gpu.shadow_shader, "u_shadow_blur");

    D(bug("[GLCompositor] Shadow shader compiled (prog=%d)\n", compdata->gpu.shadow_shader));

    return TRUE;
}

void GLCompositor_DestroyShaders(struct HIDDCompositorData *compdata)
{
    if (compdata->gpu.glUseProgram)
        compdata->gpu.glUseProgram(0);

    if (compdata->gpu.composite_shader && compdata->gpu.glDeleteProgram)
    {
        compdata->gpu.glDeleteProgram(compdata->gpu.composite_shader);
        compdata->gpu.composite_shader = 0;
    }

    if (compdata->gpu.shadow_shader && compdata->gpu.glDeleteProgram)
    {
        compdata->gpu.glDeleteProgram(compdata->gpu.shadow_shader);
        compdata->gpu.shadow_shader = 0;
    }

    compdata->gpu.shaders_valid = FALSE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Quad VBO                                                                  */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Unit quad: 4 vertices with position (0..1) and texcoord (0..1).
 * Used for both compositing (scaled to bitmap rect) and shadows (scaled to shadow area).
 * Layout: pos.x, pos.y, tex.u, tex.v
 */
static const GLfloat g_quad_vertices[] = {
    0.0f, 0.0f,  0.0f, 0.0f,
    1.0f, 0.0f,  1.0f, 0.0f,
    1.0f, 1.0f,  1.0f, 1.0f,
    0.0f, 1.0f,  0.0f, 1.0f,
};

void GLCompositor_CreateQuadVBO(struct HIDDCompositorData *compdata)
{
    if (!compdata->gpu.glGenBuffers)
        return;

    compdata->gpu.glGenBuffers(1, &compdata->gpu.quad_vbo);
    compdata->gpu.glBindBuffer(GL_ARRAY_BUFFER, compdata->gpu.quad_vbo);
    compdata->gpu.glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertices),
                               g_quad_vertices, GL_STATIC_DRAW);
    compdata->gpu.glBindBuffer(GL_ARRAY_BUFFER, 0);

    D(bug("[GLCompositor] Quad VBO created (id=%d)\n", compdata->gpu.quad_vbo));
}

void GLCompositor_DestroyQuadVBO(struct HIDDCompositorData *compdata)
{
    if (compdata->gpu.quad_vbo && compdata->gpu.glDeleteBuffers)
    {
        compdata->gpu.glDeleteBuffers(1, &compdata->gpu.quad_vbo);
        compdata->gpu.quad_vbo = 0;
    }
}
