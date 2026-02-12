/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: Layer Compositor - Per-window GPU compositing

    This module implements a layer compositor that renders individual windows
    as textured quads with per-window alpha, shadows, and other effects.
    It integrates with the screen-level GLCompositor via shared GL contexts.

    Architecture:
    - LayerCompositor operates per-screen, managing a list of registered windows
    - Each window has a GL texture (either shared from zunegfx FBO or uploaded)
    - Windows are rendered back-to-front using the composite shader
    - The GLCompositor delegates to LayerCompositor for screens that have one active
    - GL context is shared with GLCompositor via the published semaphore

    Zero-copy path (zunegfx windows):
    - Windows using zunegfx OpenGL backend have FBOs with texture attachments
    - These textures are directly accessible from the compositor's shared context
    - No pixel readback or upload is needed

    Legacy path (standard RastPort windows):
    - Window bitmap pixels are read via HIDD_BM_GetImage or ReadPixelArray
    - Uploaded to a GL texture via glTexImage2D/glTexSubImage2D
*/

#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include <GL/gl.h>
#include <GL/gla.h>

#define DEBUG 1
#include <aros/debug.h>

#include "zunegfx_intern.h"
#include "include/zunegfx.h"
#include "backends/opengl/opengl_backend.h"

/* ────────────────────────────────────────────────────────────────────────── */
/* GL Extension Function Pointer Types                                       */
/* ────────────────────────────────────────────────────────────────────────── */

typedef GLuint (*PFNGLCREATESHADERPROC_)(GLenum type);
typedef void   (*PFNGLSHADERSOURCEPROC_)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void   (*PFNGLCOMPILESHADERPROC_)(GLuint shader);
typedef GLuint (*PFNGLCREATEPROGRAMPROC_)(void);
typedef void   (*PFNGLATTACHSHADERPROC_)(GLuint program, GLuint shader);
typedef void   (*PFNGLLINKPROGRAMPROC_)(GLuint program);
typedef void   (*PFNGLUSEPROGRAMPROC_)(GLuint program);
typedef GLint  (*PFNGLGETUNIFORMLOCATIONPROC_)(GLuint program, const GLchar *name);
typedef void   (*PFNGLUNIFORM1IPROC_)(GLint location, GLint v0);
typedef void   (*PFNGLUNIFORM1FPROC_)(GLint location, GLfloat v0);
typedef void   (*PFNGLUNIFORM2FPROC_)(GLint location, GLfloat v0, GLfloat v1);
typedef void   (*PFNGLUNIFORM4FPROC_)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void   (*PFNGLDELETESHADERPROC_)(GLuint shader);
typedef void   (*PFNGLDELETEPROGRAMPROC_)(GLuint program);
typedef void   (*PFNGLGETSHADERIVPROC_)(GLuint shader, GLenum pname, GLint *params);
typedef void   (*PFNGLGETPROGRAMIVPROC_)(GLuint program, GLenum pname, GLint *params);
typedef void   (*PFNGLGENBUFFERSPROC_)(GLsizei n, GLuint *buffers);
typedef void   (*PFNGLBINDBUFFERPROC_)(GLenum target, GLuint buffer);
typedef void   (*PFNGLBUFFERDATAPROC_)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void   (*PFNGLDELETEBUFFERSPROC_)(GLsizei n, const GLuint *buffers);
typedef void   (*PFNGLENABLEVERTEXATTRIBARRAYPROC_)(GLuint index);
typedef void   (*PFNGLVERTEXATTRIBPOINTERPROC_)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLint  (*PFNGLGETATTRIBLOCATIONPROC_)(GLuint program, const GLchar *name);
typedef void   (*PFNGLBINDFRAMEBUFFERPROC_)(GLenum target, GLuint framebuffer);

/* GL constants */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER     0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW      0x88E4
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER      0x8D40
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS   0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS      0x8B82
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER    0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER  0x8B30
#endif

/* ────────────────────────────────────────────────────────────────────────── */
/* Compositor Structures                                                     */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Per-window compositing data.
 * Stored in a doubly-linked list sorted by Layer z-order (back to front).
 */
struct CompositorWindow
{
    struct MinNode      node;
    struct Window       *window;        /* The Intuition window */
    struct DrawingBoard *board;         /* Off-screen FBO (zunegfx) or NULL */
    APTR                gl_context;     /* Window's GL context (for FBO texture) */
    UBYTE               alpha;          /* Per-window alpha 0-255 */
    BOOL                dirty;          /* Needs texture re-upload */

    /* GL texture for this window's content */
    ULONG               texture_id;     /* GL texture ID */
    BOOL                is_zunegfx;     /* TRUE = shared FBO texture, no upload */
    UWORD               tex_width;      /* Allocated texture width */
    UWORD               tex_height;     /* Allocated texture height */
};

/*
 * Per-screen layer compositor.
 * Owns a GL context shared with the GLCompositor master.
 */
struct LayerCompositor
{
    struct Screen           *screen;
    APTR                    gl_context;         /* Our GL context (shared with master) */
    BOOL                    active;
    BOOL                    gl_initialized;     /* Shaders compiled, VBO created */

    struct SignalSemaphore  lock;               /* Protects the window list */
    struct MinList          windows;            /* List of CompositorWindow (back-to-front) */
    UWORD                   window_count;

    /* Shadow parameters */
    WORD                    shadow_offsetX;
    WORD                    shadow_offsetY;
    UWORD                   shadow_blur;
    UBYTE                   shadow_alpha;

    /* Shaders (compiled in our shared context) */
    ULONG                   composite_shader;
    ULONG                   shadow_shader;

    /* Composite shader uniforms */
    LONG                    u_texture;
    LONG                    u_alpha;
    LONG                    u_screen_size;

    /* Shadow shader uniforms */
    LONG                    u_shadow_screen_size;
    LONG                    u_shadow_window_pos;
    LONG                    u_shadow_window_size;
    LONG                    u_shadow_offset;
    LONG                    u_shadow_color;
    LONG                    u_shadow_blur;

    /* Quad VBO */
    ULONG                   quad_vbo;

    /* GL extension function pointers */
    PFNGLCREATESHADERPROC_          glCreateShader;
    PFNGLSHADERSOURCEPROC_          glShaderSource;
    PFNGLCOMPILESHADERPROC_         glCompileShader;
    PFNGLCREATEPROGRAMPROC_         glCreateProgram;
    PFNGLATTACHSHADERPROC_          glAttachShader;
    PFNGLLINKPROGRAMPROC_           glLinkProgram;
    PFNGLUSEPROGRAMPROC_            glUseProgram;
    PFNGLGETUNIFORMLOCATIONPROC_    glGetUniformLocation;
    PFNGLUNIFORM1IPROC_            glUniform1i;
    PFNGLUNIFORM1FPROC_            glUniform1f;
    PFNGLUNIFORM2FPROC_            glUniform2f;
    PFNGLUNIFORM4FPROC_            glUniform4f;
    PFNGLDELETESHADERPROC_          glDeleteShader;
    PFNGLDELETEPROGRAMPROC_         glDeleteProgram;
    PFNGLGETSHADERIVPROC_           glGetShaderiv;
    PFNGLGETPROGRAMIVPROC_          glGetProgramiv;
    PFNGLGENBUFFERSPROC_            glGenBuffers;
    PFNGLBINDBUFFERPROC_            glBindBuffer;
    PFNGLBUFFERDATAPROC_            glBufferData;
    PFNGLDELETEBUFFERSPROC_         glDeleteBuffers;
    PFNGLENABLEVERTEXATTRIBARRAYPROC_ glEnableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC_  glVertexAttribPointer;
    PFNGLGETATTRIBLOCATIONPROC_    glGetAttribLocation;
    PFNGLBINDFRAMEBUFFERPROC_       glBindFramebuffer;
};

/* ────────────────────────────────────────────────────────────────────────── */
/* GLCompositor Semaphore (for discovering master GL context)                */
/* ────────────────────────────────────────────────────────────────────────── */

#define GLCOMPOSITOR_SEMAPHORE_NAME "GLCompositorMasterContext"

struct GLCompositorSemaphore
{
    struct SignalSemaphore   sem;
    APTR                    master_context;
};


/* ────────────────────────────────────────────────────────────────────────── */
/* Shader Sources (identical to GLCompositor)                                */
/* ────────────────────────────────────────────────────────────────────────── */

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

static const GLchar *g_composite_fs =
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
    "}\n";

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

/* Unit quad vertices: pos.x, pos.y, tex.u, tex.v */
static const GLfloat g_quad_vertices[] = {
    0.0f, 0.0f,  0.0f, 0.0f,
    1.0f, 0.0f,  1.0f, 0.0f,
    1.0f, 1.0f,  1.0f, 1.0f,
    0.0f, 1.0f,  0.0f, 1.0f,
};

/* ────────────────────────────────────────────────────────────────────────── */
/* GL Extension Loading                                                      */
/* ────────────────────────────────────────────────────────────────────────── */

static BOOL LoadExtensions(struct LayerCompositor *comp)
{
    #define LOAD_EXT(name) \
        comp->name = (typeof(comp->name))glAGetProcAddress((const GLubyte *)#name); \
        if (!comp->name) { \
            D(bug("[LayerCompositor] Failed to load %s\n", #name)); \
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
    comp->glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC_)glAGetProcAddress((const GLubyte *)"glBindFramebuffer");
    if (!comp->glBindFramebuffer)
        comp->glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC_)glAGetProcAddress((const GLubyte *)"glBindFramebufferEXT");

    #undef LOAD_EXT

    if (!comp->glCreateShader || !comp->glShaderSource ||
        !comp->glCompileShader || !comp->glCreateProgram ||
        !comp->glAttachShader || !comp->glLinkProgram ||
        !comp->glUseProgram || !comp->glGetUniformLocation)
    {
        D(bug("[LayerCompositor] Missing required GL extensions\n"));
        return FALSE;
    }

    D(bug("[LayerCompositor] GL extensions loaded\n"));
    return TRUE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Shader Compilation                                                        */
/* ────────────────────────────────────────────────────────────────────────── */

static GLuint CompileShader(struct LayerCompositor *comp, GLenum type, const GLchar *source)
{
    GLuint shader;
    GLint status;

    shader = comp->glCreateShader(type);
    if (shader == 0)
        return 0;

    comp->glShaderSource(shader, 1, &source, NULL);
    comp->glCompileShader(shader);

    if (comp->glGetShaderiv)
    {
        comp->glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status)
        {
            D(bug("[LayerCompositor] Shader compile failed (type=%s)\n",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment"));
            comp->glDeleteShader(shader);
            return 0;
        }
    }

    return shader;
}

static GLuint LinkProgram(struct LayerCompositor *comp, GLuint vs, GLuint fs)
{
    GLuint prog;
    GLint status;

    prog = comp->glCreateProgram();
    if (prog == 0)
        return 0;

    comp->glAttachShader(prog, vs);
    comp->glAttachShader(prog, fs);
    comp->glLinkProgram(prog);

    if (comp->glGetProgramiv)
    {
        comp->glGetProgramiv(prog, GL_LINK_STATUS, &status);
        if (!status)
        {
            D(bug("[LayerCompositor] Program link failed\n"));
            comp->glDeleteProgram(prog);
            return 0;
        }
    }

    return prog;
}

static BOOL CompileCompositeShader(struct LayerCompositor *comp)
{
    GLuint vs, fs;

    vs = CompileShader(comp, GL_VERTEX_SHADER, g_composite_vs);
    if (!vs) return FALSE;

    fs = CompileShader(comp, GL_FRAGMENT_SHADER, g_composite_fs);
    if (!fs)
    {
        comp->glDeleteShader(vs);
        return FALSE;
    }

    comp->composite_shader = LinkProgram(comp, vs, fs);
    comp->glDeleteShader(vs);
    comp->glDeleteShader(fs);

    if (!comp->composite_shader)
        return FALSE;

    comp->u_texture = comp->glGetUniformLocation(comp->composite_shader, "u_texture");
    comp->u_alpha = comp->glGetUniformLocation(comp->composite_shader, "u_alpha");
    comp->u_screen_size = comp->glGetUniformLocation(comp->composite_shader, "u_screen_size");

    D(bug("[LayerCompositor] Composite shader compiled (prog=%d)\n", comp->composite_shader));
    return TRUE;
}

static BOOL CompileShadowShader(struct LayerCompositor *comp)
{
    GLuint vs, fs;

    vs = CompileShader(comp, GL_VERTEX_SHADER, g_shadow_vs);
    if (!vs) return FALSE;

    fs = CompileShader(comp, GL_FRAGMENT_SHADER, g_shadow_fs);
    if (!fs)
    {
        comp->glDeleteShader(vs);
        return FALSE;
    }

    comp->shadow_shader = LinkProgram(comp, vs, fs);
    comp->glDeleteShader(vs);
    comp->glDeleteShader(fs);

    if (!comp->shadow_shader)
        return FALSE;

    comp->u_shadow_screen_size = comp->glGetUniformLocation(comp->shadow_shader, "u_screen_size");
    comp->u_shadow_window_pos = comp->glGetUniformLocation(comp->shadow_shader, "u_window_pos");
    comp->u_shadow_window_size = comp->glGetUniformLocation(comp->shadow_shader, "u_window_size");
    comp->u_shadow_offset = comp->glGetUniformLocation(comp->shadow_shader, "u_shadow_offset");
    comp->u_shadow_color = comp->glGetUniformLocation(comp->shadow_shader, "u_shadow_color");
    comp->u_shadow_blur = comp->glGetUniformLocation(comp->shadow_shader, "u_shadow_blur");

    D(bug("[LayerCompositor] Shadow shader compiled (prog=%d)\n", comp->shadow_shader));
    return TRUE;
}

static void CreateQuadVBO(struct LayerCompositor *comp)
{
    if (!comp->glGenBuffers)
        return;

    comp->glGenBuffers(1, &comp->quad_vbo);
    comp->glBindBuffer(GL_ARRAY_BUFFER, comp->quad_vbo);
    comp->glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertices),
                       g_quad_vertices, GL_STATIC_DRAW);
    comp->glBindBuffer(GL_ARRAY_BUFFER, 0);

    D(bug("[LayerCompositor] Quad VBO created (id=%d)\n", comp->quad_vbo));
}

/* ────────────────────────────────────────────────────────────────────────── */
/* GL Initialization (lazy, on first render)                                 */
/* ────────────────────────────────────────────────────────────────────────── */

static BOOL EnsureGLInitialized(struct LayerCompositor *comp)
{
    if (comp->gl_initialized)
        return TRUE;

    if (!comp->gl_context)
        return FALSE;

    glAMakeCurrent(comp->gl_context);

    if (!LoadExtensions(comp))
        return FALSE;

    if (!CompileCompositeShader(comp))
        return FALSE;

    /* Shadow shader is optional */
    if (!CompileShadowShader(comp))
    {
        D(bug("[LayerCompositor] Shadow shader failed (non-fatal)\n"));
    }

    CreateQuadVBO(comp);

    comp->gl_initialized = TRUE;
    D(bug("[LayerCompositor] GL initialized successfully\n"));
    return TRUE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Window Texture Management                                                 */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Ensure the GL texture for a window is valid and up-to-date.
 * For zunegfx windows: bind the FBO texture directly (zero-copy).
 * For legacy windows: read pixels and upload to texture.
 */
static BOOL EnsureWindowTexture(struct LayerCompositor *comp,
                                struct CompositorWindow *cw)
{
    struct Window *win = cw->window;
    UWORD width, height;

    if (!win)
        return FALSE;

    /* Calculate inner window dimensions */
    width = win->Width - win->BorderLeft - win->BorderRight;
    height = win->Height - win->BorderTop - win->BorderBottom;

    if (width == 0 || height == 0)
        return FALSE;

    /* zunegfx path: use the DrawingBoard's FBO texture directly */
    if (cw->board && cw->board->backend_data)
    {
        OpenGLFBOData *fbo = (OpenGLFBOData *)cw->board->backend_data;
        if (fbo->valid && fbo->texture_id)
        {
            cw->texture_id = fbo->texture_id;
            cw->is_zunegfx = TRUE;
            cw->tex_width = fbo->width;
            cw->tex_height = fbo->height;
            cw->dirty = FALSE;
            return TRUE;
        }
    }

    /* Legacy path: upload pixels from window's RastPort */
    if (!cw->dirty && cw->texture_id &&
        cw->tex_width == width && cw->tex_height == height)
    {
        return TRUE;  /* Texture is still valid */
    }

    /* Create texture if needed */
    if (!cw->texture_id)
    {
        glGenTextures(1, &cw->texture_id);
        glBindTexture(GL_TEXTURE_2D, cw->texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        cw->is_zunegfx = FALSE;
    }

    /* Read pixels from window RastPort and upload */
    {
        ULONG modulo = width * 4;
        APTR tmpbuf = AllocMem(modulo * height, MEMF_ANY);
        if (!tmpbuf)
        {
            D(bug("[LayerCompositor] Failed to alloc temp buffer for window %p\n", win));
            return FALSE;
        }

        /* Read from inner window area */
        ReadPixelArray(tmpbuf, 0, 0, modulo,
                       win->RPort,
                       win->BorderLeft, win->BorderTop,
                       width, height,
                       RECTFMT_ARGB);

        glBindTexture(GL_TEXTURE_2D, cw->texture_id);

        if (cw->tex_width != width || cw->tex_height != height)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                         0, GL_BGRA, GL_UNSIGNED_BYTE, tmpbuf);
            cw->tex_width = width;
            cw->tex_height = height;
        }
        else
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                            GL_BGRA, GL_UNSIGNED_BYTE, tmpbuf);
        }

        FreeMem(tmpbuf, modulo * height);
    }

    cw->dirty = FALSE;
    return TRUE;
}

static void FreeWindowTexture(struct CompositorWindow *cw)
{
    if (cw->texture_id && !cw->is_zunegfx)
    {
        glDeleteTextures(1, &cw->texture_id);
    }
    cw->texture_id = 0;
    cw->tex_width = 0;
    cw->tex_height = 0;
    cw->is_zunegfx = FALSE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Rendering                                                                 */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Draw a shadow behind a window using the SDF shadow shader.
 */
static void DrawWindowShadow(struct LayerCompositor *comp,
                             struct CompositorWindow *cw,
                             GLfloat screenw, GLfloat screenh)
{
    struct Window *win = cw->window;
    GLint a_position;
    GLfloat win_x, win_y, win_w, win_h;

    if (!comp->shadow_shader || comp->shadow_alpha == 0 || comp->shadow_blur == 0)
        return;

    /* Window inner area position on screen */
    win_x = (GLfloat)(win->LeftEdge + win->BorderLeft);
    win_y = (GLfloat)(win->TopEdge + win->BorderTop);
    win_w = (GLfloat)(win->Width - win->BorderLeft - win->BorderRight);
    win_h = (GLfloat)(win->Height - win->BorderTop - win->BorderBottom);

    comp->glUseProgram(comp->shadow_shader);

    comp->glUniform2f(comp->u_shadow_screen_size, screenw, screenh);
    comp->glUniform2f(comp->u_shadow_window_pos, win_x, win_y);
    comp->glUniform2f(comp->u_shadow_window_size, win_w, win_h);
    comp->glUniform2f(comp->u_shadow_offset,
                      (GLfloat)comp->shadow_offsetX, (GLfloat)comp->shadow_offsetY);
    comp->glUniform4f(comp->u_shadow_color,
                      0.0f, 0.0f, 0.0f, (GLfloat)comp->shadow_alpha / 255.0f);
    comp->glUniform1f(comp->u_shadow_blur, (GLfloat)comp->shadow_blur);

    /* Draw using unit quad VBO */
    comp->glBindBuffer(GL_ARRAY_BUFFER, comp->quad_vbo);
    a_position = comp->glGetAttribLocation(comp->shadow_shader, "a_position");
    if (a_position >= 0)
    {
        comp->glEnableVertexAttribArray(a_position);
        comp->glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE,
                                    4 * sizeof(GLfloat), (void *)0);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    comp->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*
 * Draw a single window as a textured quad with alpha.
 */
static void DrawWindowQuad(struct LayerCompositor *comp,
                           struct CompositorWindow *cw,
                           GLfloat screenw, GLfloat screenh)
{
    struct Window *win = cw->window;
    GLfloat x0, y0, x1, y1;
    GLfloat alpha;
    GLint a_position, a_texcoord;
    GLfloat vertices[16];

    /* Window inner area position on screen */
    x0 = (GLfloat)(win->LeftEdge + win->BorderLeft);
    y0 = (GLfloat)(win->TopEdge + win->BorderTop);
    x1 = x0 + (GLfloat)(win->Width - win->BorderLeft - win->BorderRight);
    y1 = y0 + (GLfloat)(win->Height - win->BorderTop - win->BorderBottom);

    alpha = (GLfloat)cw->alpha / 255.0f;

    comp->glUseProgram(comp->composite_shader);
    comp->glUniform2f(comp->u_screen_size, screenw, screenh);
    comp->glUniform1i(comp->u_texture, 0);
    comp->glUniform1f(comp->u_alpha, alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cw->texture_id);

    /* Full texture mapped to window area */
    vertices[0]  = x0; vertices[1]  = y0; vertices[2]  = 0.0f; vertices[3]  = 0.0f;
    vertices[4]  = x1; vertices[5]  = y0; vertices[6]  = 1.0f; vertices[7]  = 0.0f;
    vertices[8]  = x1; vertices[9]  = y1; vertices[10] = 1.0f; vertices[11] = 1.0f;
    vertices[12] = x0; vertices[13] = y1; vertices[14] = 0.0f; vertices[15] = 1.0f;

    a_position = comp->glGetAttribLocation(comp->composite_shader, "a_position");
    a_texcoord = comp->glGetAttribLocation(comp->composite_shader, "a_texcoord");

    if (a_position >= 0)
    {
        comp->glEnableVertexAttribArray(a_position);
        comp->glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE,
                                    4 * sizeof(GLfloat), &vertices[0]);
    }
    if (a_texcoord >= 0)
    {
        comp->glEnableVertexAttribArray(a_texcoord);
        comp->glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE,
                                    4 * sizeof(GLfloat), &vertices[2]);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Window Texture Cache Helpers                                              */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Find a cached CompositorWindow for a given Intuition Window.
 * Returns NULL if not found. Must be called with comp->lock held.
 */
static struct CompositorWindow *FindCachedWindow(struct LayerCompositor *comp,
                                                  struct Window *win)
{
    struct CompositorWindow *cw;

    ForeachNode(&comp->windows, cw)
    {
        if (cw->window == win)
            return cw;
    }
    return NULL;
}

/*
 * Get or create a CompositorWindow cache entry for a given window.
 * Must be called with comp->lock held.
 */
static struct CompositorWindow *GetOrCreateCachedWindow(struct LayerCompositor *comp,
                                                         struct Window *win)
{
    struct CompositorWindow *cw;

    cw = FindCachedWindow(comp, win);
    if (cw)
        return cw;

    /* Create a new cache entry */
    cw = AllocMem(sizeof(struct CompositorWindow), MEMF_ANY | MEMF_CLEAR);
    if (!cw)
        return NULL;

    cw->window = win;
    cw->alpha = 255;        /* Fully opaque by default */
    cw->dirty = TRUE;
    cw->board = NULL;
    cw->gl_context = NULL;
    cw->is_zunegfx = FALSE;

    AddTail((struct List *)&comp->windows, (struct Node *)cw);
    comp->window_count++;

    return cw;
}

/*
 * Mark all cached windows as "not seen" before a render pass.
 * After rendering, unseen windows can be cleaned up.
 * We repurpose the 'dirty' flag is not suitable, so we use a simple
 * approach: walk the cache after rendering and remove stale entries.
 */
static void PurgeStaleWindows(struct LayerCompositor *comp,
                               struct Window *firstWindow)
{
    struct CompositorWindow *cw, *next;

    ForeachNodeSafe(&comp->windows, cw, next)
    {
        struct Window *win;
        BOOL found = FALSE;

        /* Check if this cached window still exists on the screen */
        for (win = firstWindow; win; win = win->NextWindow)
        {
            if (win == cw->window)
            {
                found = TRUE;
                break;
            }
        }

        if (!found)
        {
            Remove((struct Node *)cw);
            comp->window_count--;
            FreeWindowTexture(cw);
            FreeMem(cw, sizeof(struct CompositorWindow));
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════ */
/* PUBLIC API: Internal implementations called by library wrappers         */
/* ════════════════════════════════════════════════════════════════════════ */

/*
 * CreateLayerCompositorInternal - Create a LayerCompositor for a screen.
 * Discovers the GLCompositor master context via the published semaphore.
 */
struct LayerCompositor *CreateLayerCompositorInternal(struct Screen *screen)
{
    struct GLCompositorSemaphore *glsem;
    APTR masterCtx = NULL;

    if (!screen)
        return NULL;

    /* Try to discover the GLCompositor master context */
    Forbid();
    glsem = (struct GLCompositorSemaphore *)FindSemaphore(GLCOMPOSITOR_SEMAPHORE_NAME);
    if (glsem)
        masterCtx = glsem->master_context;
    Permit();

    return CreateLayerCompositorSharedInternal(screen, masterCtx);
}

/*
 * CreateLayerCompositorSharedInternal - Create with an explicit shared context.
 */
struct LayerCompositor *CreateLayerCompositorSharedInternal(struct Screen *screen,
                                                            APTR masterGLContext)
{
    struct LayerCompositor *comp;

    if (!screen)
        return NULL;

    comp = AllocMem(sizeof(struct LayerCompositor), MEMF_ANY | MEMF_CLEAR);
    if (!comp)
        return NULL;

    comp->screen = screen;
    comp->active = FALSE;
    comp->gl_initialized = FALSE;
    comp->window_count = 0;

    NEWLIST(&comp->windows);
    InitSemaphore(&comp->lock);

    /* Default shadow parameters */
    comp->shadow_offsetX = 4;
    comp->shadow_offsetY = 4;
    comp->shadow_blur = 12;
    comp->shadow_alpha = 128;

    /* Create a GL context shared with the master.
     * We need a friend bitmap from the screen for the Gallium driver lookup. */
    if (masterGLContext)
    {
        struct TagItem ctxtags[] =
        {
            { GLA_Headless,         GL_TRUE },
            { GLA_ShareContext,     (IPTR)masterGLContext },
            { GLA_BitsPerPixel,     32 },
            { GLA_Width,            screen->Width },
            { GLA_Height,           screen->Height },
            { GLA_NoDepth,          GL_TRUE },
            { GLA_NoStencil,        GL_TRUE },
            { GLA_NoAccum,          GL_TRUE },
            { TAG_DONE,             0 }
        };

        D(bug("[LayerCompositor] Creating shared GL context (master=%p, %dx%d)\n",
              masterGLContext, screen->Width, screen->Height));
        comp->gl_context = glACreateContext(ctxtags);

        if (!comp->gl_context)
        {
            D(bug("[LayerCompositor] Failed to create shared GL context\n"));
        }
        else
        {
            D(bug("[LayerCompositor] GL context created: %p\n", comp->gl_context));
        }
    }
    else
    {
        D(bug("[LayerCompositor] No master GL context available - GPU compositing disabled\n"));
    }

    D(bug("[LayerCompositor] Created compositor %p for screen %p\n", comp, screen));
    return comp;
}

/*
 * DestroyLayerCompositorInternal - Clean up and free a LayerCompositor.
 */
void DestroyLayerCompositorInternal(struct LayerCompositor *comp)
{
    struct CompositorWindow *cw;

    if (!comp)
        return;

    D(bug("[LayerCompositor] Destroying compositor %p\n", comp));

    comp->active = FALSE;

    ObtainSemaphore(&comp->lock);

    /* Free all registered windows */
    while (!IsListEmpty((struct List *)&comp->windows))
    {
        cw = (struct CompositorWindow *)RemHead((struct List *)&comp->windows);

        if (comp->gl_context)
        {
            glAMakeCurrent(comp->gl_context);
            FreeWindowTexture(cw);
        }

        FreeMem(cw, sizeof(struct CompositorWindow));
    }

    /* Destroy GL resources */
    if (comp->gl_context)
    {
        glAMakeCurrent(comp->gl_context);

        if (comp->glUseProgram)
            comp->glUseProgram(0);

        if (comp->composite_shader && comp->glDeleteProgram)
            comp->glDeleteProgram(comp->composite_shader);

        if (comp->shadow_shader && comp->glDeleteProgram)
            comp->glDeleteProgram(comp->shadow_shader);

        if (comp->quad_vbo && comp->glDeleteBuffers)
            comp->glDeleteBuffers(1, &comp->quad_vbo);

        glADestroyContext(comp->gl_context);
    }

    ReleaseSemaphore(&comp->lock);

    FreeMem(comp, sizeof(struct LayerCompositor));
}

/*
 * ActivateLayerCompositorInternal - Enable compositing on this screen.
 */
BOOL ActivateLayerCompositorInternal(struct LayerCompositor *comp)
{
    if (!comp)
        return FALSE;

    if (!comp->gl_context)
    {
        D(bug("[LayerCompositor] Cannot activate - no GL context\n"));
        return FALSE;
    }

    comp->active = TRUE;

    D(bug("[LayerCompositor] Activated compositor %p\n", comp));
    return TRUE;
}

/*
 * DeactivateLayerCompositorInternal - Disable compositing.
 */
void DeactivateLayerCompositorInternal(struct LayerCompositor *comp)
{
    if (!comp)
        return;

    comp->active = FALSE;
    D(bug("[LayerCompositor] Deactivated compositor %p\n", comp));
}

/*
 * CompositorRegisterWindowInternal - Register a window for compositing.
 */
struct CompositorWindow *CompositorRegisterWindowInternal(
    struct LayerCompositor *comp,
    struct Window *window,
    APTR glContext,
    struct DrawingBoard *board,
    UBYTE alpha)
{
    struct CompositorWindow *cw;

    if (!comp || !window)
        return NULL;

    /* Check if already registered */
    ObtainSemaphoreShared(&comp->lock);
    ForeachNode(&comp->windows, cw)
    {
        if (cw->window == window)
        {
            ReleaseSemaphore(&comp->lock);
            D(bug("[LayerCompositor] Window %p already registered\n", window));
            return cw;
        }
    }
    ReleaseSemaphore(&comp->lock);

    cw = AllocMem(sizeof(struct CompositorWindow), MEMF_ANY | MEMF_CLEAR);
    if (!cw)
        return NULL;

    cw->window = window;
    cw->board = board;
    cw->gl_context = glContext;
    cw->alpha = alpha;
    cw->dirty = TRUE;

    ObtainSemaphore(&comp->lock);
    AddTail((struct List *)&comp->windows, (struct Node *)cw);
    comp->window_count++;
    ReleaseSemaphore(&comp->lock);

    D(bug("[LayerCompositor] Registered window %p (board=%p, alpha=%d)\n",
          window, board, alpha));

    return cw;
}

/*
 * CompositorUnregisterWindowInternal - Remove a window from the compositor.
 */
void CompositorUnregisterWindowInternal(struct LayerCompositor *comp,
                                        struct Window *window)
{
    struct CompositorWindow *cw, *next;

    if (!comp || !window)
        return;

    ObtainSemaphore(&comp->lock);

    ForeachNodeSafe(&comp->windows, cw, next)
    {
        if (cw->window == window)
        {
            Remove((struct Node *)cw);
            comp->window_count--;

            if (comp->gl_context)
            {
                glAMakeCurrent(comp->gl_context);
                FreeWindowTexture(cw);
            }

            FreeMem(cw, sizeof(struct CompositorWindow));

            D(bug("[LayerCompositor] Unregistered window %p\n", window));
            break;
        }
    }

    ReleaseSemaphore(&comp->lock);
}

/*
 * CompositorSetWindowAlphaInternal - Change a window's alpha value.
 */
void CompositorSetWindowAlphaInternal(struct LayerCompositor *comp,
                                      struct Window *window,
                                      UBYTE alpha)
{
    struct CompositorWindow *cw;

    if (!comp || !window)
        return;

    ObtainSemaphoreShared(&comp->lock);

    ForeachNode(&comp->windows, cw)
    {
        if (cw->window == window)
        {
            cw->alpha = alpha;
            D(bug("[LayerCompositor] Window %p alpha set to %d\n", window, alpha));
            break;
        }
    }

    ReleaseSemaphore(&comp->lock);
}

/*
 * CompositorMarkWindowDirtyInternal - Mark a window's texture as needing update.
 */
void CompositorMarkWindowDirtyInternal(struct LayerCompositor *comp,
                                       struct Window *window)
{
    struct CompositorWindow *cw;

    if (!comp || !window)
        return;

    ObtainSemaphoreShared(&comp->lock);

    ForeachNode(&comp->windows, cw)
    {
        if (cw->window == window)
        {
            cw->dirty = TRUE;
            break;
        }
    }

    ReleaseSemaphore(&comp->lock);
}

/*
 * CompositorFindWindowInternal - Find a registered window.
 */
struct CompositorWindow *CompositorFindWindowInternal(struct LayerCompositor *comp,
                                                      struct Window *window)
{
    struct CompositorWindow *cw;

    if (!comp || !window)
        return NULL;

    ObtainSemaphoreShared(&comp->lock);

    ForeachNode(&comp->windows, cw)
    {
        if (cw->window == window)
        {
            ReleaseSemaphore(&comp->lock);
            return cw;
        }
    }

    ReleaseSemaphore(&comp->lock);
    return NULL;
}

/*
 * CompositorUpdateInternal - Render all windows on the screen.
 *
 * This is the main rendering entry point. It automatically discovers
 * ALL windows on the screen by walking Screen->FirstWindow, then
 * renders them back-to-front using the Layer z-order.
 *
 * The compositor maintains a texture cache (comp->windows) to avoid
 * re-creating GL textures every frame. Stale entries (closed windows)
 * are purged after each render pass.
 *
 * NOTE: This renders into the current GL framebuffer. When called from
 * GLCompositor, FBO 0 is already bound and the viewport is set.
 * When called standalone, the caller must set up the framebuffer.
 */
void CompositorUpdateInternal(struct LayerCompositor *comp)
{
    struct Layer *layer;
    struct CompositorWindow *cw;
    GLfloat screenw, screenh;

    if (!comp || !comp->active)
        return;

    if (!comp->gl_context)
        return;

    if (!comp->screen)
        return;

    glAMakeCurrent(comp->gl_context);

    if (!EnsureGLInitialized(comp))
        return;

    ObtainSemaphore(&comp->lock);

    screenw = (GLfloat)comp->screen->Width;
    screenh = (GLfloat)comp->screen->Height;

    /* Render to default framebuffer */
    if (comp->glBindFramebuffer)
        comp->glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, (GLsizei)screenw, (GLsizei)screenh);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    /*
     * Walk layers back-to-front for correct z-order rendering.
     * Each Layer with a Window gets rendered as a textured quad.
     */
    LockLayerInfo(&comp->screen->LayerInfo);

    for (layer = comp->screen->LayerInfo.check_lp; layer; layer = layer->front)
    {
        struct Window *win = (struct Window *)layer->Window;
        if (!win)
            continue;

        /* Get or create a texture cache entry for this window */
        cw = GetOrCreateCachedWindow(comp, win);
        if (!cw)
            continue;

        /* Mark all windows as dirty every frame for now (Option A: simple) */
        cw->dirty = TRUE;

        if (!EnsureWindowTexture(comp, cw))
            continue;

        /* Draw shadow first (behind the window) */
        DrawWindowShadow(comp, cw, screenw, screenh);

        /* Draw the window content */
        DrawWindowQuad(comp, cw, screenw, screenh);
    }

    UnlockLayerInfo(&comp->screen->LayerInfo);

    /* Purge cache entries for windows that no longer exist */
    PurgeStaleWindows(comp, comp->screen->FirstWindow);

    glDisable(GL_BLEND);

    ReleaseSemaphore(&comp->lock);
}

/*
 * CompositorRefreshInternal - Force re-upload of all window textures
 * and trigger a full re-render.
 */
void CompositorRefreshInternal(struct LayerCompositor *comp)
{
    struct CompositorWindow *cw;

    if (!comp)
        return;

    ObtainSemaphore(&comp->lock);

    /* Mark all cached textures as dirty so they get re-uploaded */
    ForeachNode(&comp->windows, cw)
    {
        cw->dirty = TRUE;
    }

    ReleaseSemaphore(&comp->lock);

    /* Trigger a full re-render (which auto-discovers all windows) */
    CompositorUpdateInternal(comp);
}

/*
 * CompositorSetShadowInternal - Configure shadow parameters.
 */
void CompositorSetShadowInternal(struct LayerCompositor *comp,
                                 WORD offsetX, WORD offsetY,
                                 UWORD blur, UBYTE alpha)
{
    if (!comp)
        return;

    comp->shadow_offsetX = offsetX;
    comp->shadow_offsetY = offsetY;
    comp->shadow_blur = blur;
    comp->shadow_alpha = alpha;

    D(bug("[LayerCompositor] Shadow: offset=(%d,%d) blur=%d alpha=%d\n",
          offsetX, offsetY, blur, alpha));
}
