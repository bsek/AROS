/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Implementation

    This file implements the OpenGL rendering backend for ZuneRenderer.
    It uses AROS's gl.library (mesa3dgl.library or hostgl.library) to
    provide hardware-accelerated 2D rendering.

    The backend can work with:
    - mesa3dgl.library: Software Mesa3D implementation (native AROS)
    - hostgl.library: Hardware-accelerated passthrough (hosted AROS on X11)

    Context Management:
    - Each RenderPort/DrawingBoard gets its own GL context
    - Contexts are created on-demand when OpenGL rendering is first used
    - The backend manages context switching transparently

    For DrawingBoards (off-screen rendering without a Window):
    - We use GLA_RastPort with explicit GLA_Width/GLA_Height
    - This requires the Mesa modification that allows RastPort-only mode
*/

#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <utility/tagitem.h>
#include <math.h>
#include <string.h>

/* GL includes */
#include <GL/gl.h>
#include <GL/gla.h>
#include <GL/glext.h>

/* CyberGraphics library base - opened elsewhere */
extern struct Library *CyberGfxBase;

/*****************************************************************************/
/* Shader Function Pointers                                                  */
/*****************************************************************************/

/*
 * Shader function types are defined in GL/glext.h which is included via GL/gl.h.
 * GL 2.0 constants (GL_FRAGMENT_SHADER, GL_VERTEX_SHADER, etc.) are also defined there.
 */

/* Shader function pointers - loaded via glAGetProcAddress */
static PFNGLCREATESHADERPROC        glCreateShader_ptr      = NULL;
static PFNGLSHADERSOURCEPROC        glShaderSource_ptr      = NULL;
static PFNGLCOMPILESHADERPROC       glCompileShader_ptr     = NULL;
static PFNGLGETSHADERINFOLOGPROC    glGetShaderInfoLog_ptr  = NULL;
static PFNGLGETSHADERIVPROC         glGetShaderiv_ptr       = NULL;
static PFNGLCREATEPROGRAMPROC       glCreateProgram_ptr     = NULL;
static PFNGLATTACHSHADERPROC        glAttachShader_ptr      = NULL;
static PFNGLLINKPROGRAMPROC         glLinkProgram_ptr       = NULL;
static PFNGLGETPROGRAMINFOLOGPROC   glGetProgramInfoLog_ptr = NULL;
static PFNGLGETPROGRAMIVPROC        glGetProgramiv_ptr      = NULL;
static PFNGLUSEPROGRAMPROC          glUseProgram_ptr        = NULL;
static PFNGLDETACHSHADERPROC        glDetachShader_ptr      = NULL;
static PFNGLDELETESHADERPROC        glDeleteShader_ptr      = NULL;
static PFNGLDELETEPROGRAMPROC       glDeleteProgram_ptr     = NULL;
static PFNGLGETUNIFORMLOCATIONPROC  glGetUniformLocation_ptr = NULL;
static PFNGLUNIFORM1FPROC           glUniform1f_ptr         = NULL;
static PFNGLUNIFORM2FPROC           glUniform2f_ptr         = NULL;
static PFNGLUNIFORM4FPROC           glUniform4f_ptr         = NULL;

/*****************************************************************************/
/* FBO Function Pointer Types and Pointers                                   */
/*****************************************************************************/

/*
 * FBO (Framebuffer Object) support for off-screen rendering.
 *
 * Architecture:
 * - Each Window gets its own GL context (via glACreateContext)
 * - Each DrawingBoard within a window gets its own FBO
 * - Switching between DrawingBoards is done via glBindFramebuffer (fast)
 * - No need for glASetRast when switching between DrawingBoards
 */

/* FBO function pointer types */
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);

/* FBO function pointers - loaded via glAGetProcAddress */
static PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers_ptr = NULL;
static PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers_ptr = NULL;
static PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer_ptr = NULL;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus_ptr = NULL;
static PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D_ptr = NULL;
static PFNGLGENRENDERBUFFERSPROC        glGenRenderbuffers_ptr = NULL;
static PFNGLDELETERENDERBUFFERSPROC     glDeleteRenderbuffers_ptr = NULL;
static PFNGLBINDRENDERBUFFERPROC        glBindRenderbuffer_ptr = NULL;
static PFNGLRENDERBUFFERSTORAGEPROC     glRenderbufferStorage_ptr = NULL;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ptr = NULL;

/* FBO constants - may not be defined in older GL headers */
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_STENCIL_ATTACHMENT             0x8D20
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_UNSUPPORTED        0x8CDD
#endif

/* FBO availability flag */
static BOOL g_fbo_available = FALSE;

/*****************************************************************************/
/* VBO Function Pointer Types and Pointers                                   */
/*****************************************************************************/

/* VBO function pointer types */
typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);

/* VBO function pointers */
static PFNGLGENBUFFERSPROC              glGenBuffers_ptr = NULL;
static PFNGLDELETEBUFFERSPROC           glDeleteBuffers_ptr = NULL;
static PFNGLBINDBUFFERPROC              glBindBuffer_ptr = NULL;
static PFNGLBUFFERDATAPROC              glBufferData_ptr = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ptr = NULL;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ptr = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer_ptr = NULL;
static PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation_ptr = NULL;

/* VBO constants */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#endif

/* VBO state */
static BOOL g_vbo_available = FALSE;
static GLuint g_quad_vbo = 0;  /* Shared VBO for unit quad */

/* Quad vertex data: position (x,y) + texcoord (s,t) */
static const GLfloat g_quad_vertices[] = {
    /* x,    y,    s,    t */
    0.0f, 0.0f, 0.0f, 0.0f,  /* Bottom-left */
    1.0f, 0.0f, 1.0f, 0.0f,  /* Bottom-right */
    1.0f, 1.0f, 1.0f, 1.0f,  /* Top-right */
    0.0f, 1.0f, 0.0f, 1.0f,  /* Top-left */
};

/* Attribute locations for VBO rendering */
static GLint g_attrib_position = -1;
static GLint g_attrib_texcoord = -1;

/* Shader state */
static BOOL g_shaders_available = FALSE;
static GLuint g_rounded_rect_program = 0;
static GLuint g_rounded_rect_vs = 0;
static GLuint g_rounded_rect_fs = 0;

/* Shader uniform locations */
static GLint g_uniform_rect_size = -1;
static GLint g_uniform_rect_radius = -1;
static GLint g_uniform_fill_color = -1;
static GLint g_uniform_border_color = -1;
static GLint g_uniform_border_width = -1;
static GLint g_uniform_has_border = -1;
static GLint g_uniform_has_fill = -1;

#define DEBUG 1
#include <aros/debug.h>

#include "../backend_interface.h"
#include "opengl_backend.h"

/*****************************************************************************/
/* Global GL Library Base                                                    */
/*****************************************************************************/

/*
 * GLBase is defined in zunerenderer_init.c and declared in zunerenderer_intern.h.
 * It is opened in DetectLibraries() in zunerenderer_core.c.
 */
extern struct Library *GLBase;

/*
 * Global OpenGL private data pointer
 * This is set during OpenGLInitBackend and provides access to the single
 * global GL context from all drawing functions.
 */
static OpenGLPrivateData *g_opengl_priv = NULL;

/*****************************************************************************/
/* Forward Declarations                                                      */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx);
static void OpenGLCleanupBackend(ZuneBackendContext *ctx);
static BOOL OpenGLIsAvailable(void);
static BOOL OpenGLIsCompatible(struct RenderPort *rp);
static ULONG OpenGLGetCapabilities(void);

static BOOL OpenGLInitRenderPort(struct RenderPort *rp);
static void OpenGLCleanupRenderPort(struct RenderPort *rp);

static BOOL OpenGLPrepareColor(struct RenderPort *rp,
                               struct InternalColor *color);
static void OpenGLReleaseColor(struct RenderPort *rp,
                               struct InternalColor *color);

static void OpenGLDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias);
static void OpenGLDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias);
static void OpenGLDrawRectangle(struct RenderPort *rp, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius, struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias);
static void OpenGLDrawCircle(struct RenderPort *rp, WORD center_x, WORD center_y,
                             UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias);

static void OpenGLClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color);

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out);
static void OpenGLUnlockPixels(struct DrawingBoard *board);
static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y);
static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color);

static void OpenGLBeginBatch(struct RenderPort *rp);
static void OpenGLEndBatch(struct RenderPort *rp);
static void OpenGLFlushBatch(struct RenderPort *rp);
static BOOL OpenGLIsBatching(struct RenderPort *rp);

static void OpenGLBlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height);
static void OpenGLBlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height);

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board);
void OpenGLCleanupDrawingBoard(struct DrawingBoard *board);

/* Helper functions */
static BOOL OpenGL_EnsureGlobalContext(struct Window *window);
static BOOL OpenGL_SwitchToWindow(struct RenderPort *rp);
static BOOL OpenGL_SwitchToDrawingBoard(struct RenderPort *rp);
static BOOL OpenGL_SwitchToTarget(struct RenderPort *rp);
static void OpenGL_SetupOrthoProjection(UWORD width, UWORD height);
static void OpenGL_SetColor(struct InternalColor *color);

/* Shader functions */
static BOOL OpenGL_LoadShaderFunctions(void);
static BOOL OpenGL_CreateRoundedRectShader(void);
static void OpenGL_DestroyShaders(void);

/* FBO functions */
static BOOL OpenGL_LoadFBOFunctions(void);
static OpenGLFBOData *OpenGL_CreateFBO(UWORD width, UWORD height);
static void OpenGL_DestroyFBO(OpenGLFBOData *fbo);
static BOOL OpenGL_BindFBO(OpenGLFBOData *fbo);
static void OpenGL_UnbindFBO(void);

/* Window context functions */
static OpenGLWindowContext *OpenGL_CreateWindowContext(struct Window *window);
static void OpenGL_DestroyWindowContext(OpenGLWindowContext *ctx);
static OpenGLWindowContext *OpenGL_FindWindowContext(struct Window *window);
static BOOL OpenGL_MakeContextCurrent(OpenGLWindowContext *ctx);
static BOOL OpenGL_SyncFBOToBitmap(struct RenderPort *rp);

/*****************************************************************************/
/* Rounded Rectangle Shader Source                                           */
/*****************************************************************************/

/*
 * Vertex Shader for Rounded Rectangle
 * Simply passes through position and texture coordinates
 */
static const GLchar *g_rounded_rect_vs_source =
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = gl_MultiTexCoord0.xy;\n"
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "}\n";

/*
 * Fragment Shader for Rounded Rectangle using SDF (Signed Distance Field)
 *
 * This shader calculates the signed distance from each pixel to the rounded
 * rectangle boundary. Pixels inside have negative distance, outside positive.
 * We use this to:
 * 1. Fill the interior with smooth antialiased edges
 * 2. Draw a border of specified width with antialiased edges
 */
static const GLchar *g_rounded_rect_fs_source =
    "varying vec2 v_texcoord;\n"
    "uniform vec2 u_size;\n"           /* Rectangle size in pixels */
    "uniform float u_radius;\n"        /* Corner radius in pixels */
    "uniform vec4 u_fill_color;\n"     /* Fill color RGBA */
    "uniform vec4 u_border_color;\n"   /* Border color RGBA */
    "uniform float u_border_width;\n"  /* Border width in pixels */
    "uniform float u_has_fill;\n"      /* 1.0 if filled, 0.0 otherwise */
    "uniform float u_has_border;\n"    /* 1.0 if has border, 0.0 otherwise */
    "\n"
    "/* SDF for a rounded rectangle */\n"
    "float sdRoundedRect(vec2 p, vec2 b, float r) {\n"
    "    vec2 q = abs(p) - b + vec2(r);\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    /* Convert texcoord (0-1) to pixel coordinates centered at origin */\n"
    "    vec2 pixelPos = (v_texcoord - 0.5) * u_size;\n"
    "    \n"
    "    /* Half-size of rectangle */\n"
    "    vec2 halfSize = u_size * 0.5;\n"
    "    \n"
    "    /* Calculate signed distance to rounded rect edge */\n"
    "    float dist = sdRoundedRect(pixelPos, halfSize, u_radius);\n"
    "    \n"
    "    /* Antialiasing: smooth transition over ~1.5 pixels */\n"
    "    float aa = 1.5;\n"
    "    \n"
    "    /* Start with transparent */\n"
    "    vec4 color = vec4(0.0);\n"
    "    \n"
    "    /* Fill: inside the shape (dist < 0) */\n"
    "    if (u_has_fill > 0.5) {\n"
    "        float fillAlpha = 1.0 - smoothstep(-aa, 0.0, dist);\n"
    "        color = u_fill_color * fillAlpha;\n"
    "    }\n"
    "    \n"
    "    /* Border: ring around the edge */\n"
    "    if (u_has_border > 0.5 && u_border_width > 0.0) {\n"
    "        /* Border is from (edge - border_width) to edge */\n"
    "        float innerDist = dist + u_border_width;\n"
    "        /* Alpha is 1 when between inner and outer edge */\n"
    "        float borderAlpha = (1.0 - smoothstep(-aa, 0.0, dist)) * smoothstep(-aa, 0.0, innerDist);\n"
    "        /* Blend border over fill */\n"
    "        color = mix(color, u_border_color, borderAlpha * u_border_color.a);\n"
    "    }\n"
    "    \n"
    "    gl_FragColor = color;\n"
    "}\n";

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
 *
 * For OpenGL backend, we:
 * 1. Read pixels from source RastPort using ReadPixelArray
 * 2. Upload them as a texture
 * 3. Draw the texture to the framebuffer at the destination position
 */
static void OpenGLCopyFromRastPort(struct RenderPort *rp, struct RastPort *src_rp,
                                   WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                                   UWORD width, UWORD height)
{
    UBYTE *pixelbuffer;
    UBYTE *flipped_buffer;
    GLuint texture;
    ULONG row, src_row, dst_row;

    D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: src(%d,%d) -> dst(%d,%d) %dx%d\n",
          src_x, src_y, dst_x, dst_y, width, height));

    if (!rp || !src_rp || !g_opengl_priv) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: Invalid parameters\n"));
        return;
    }

    if (!CyberGfxBase) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: CyberGfxBase not available\n"));
        return;
    }

    /* Ensure we have a GL context */
    if (!OpenGL_SwitchToTarget(rp)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: Failed to switch to target\n"));
        return;
    }

    /* Validate dimensions */
    if (width == 0 || height == 0) {
        return;
    }

    /* Check maximum texture size to avoid Mesa errors */
    {
        GLint max_texture_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (max_texture_size > 0 && ((GLint)width > max_texture_size || (GLint)height > max_texture_size)) {
            D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: Size %dx%d exceeds max texture size %d\n",
                  width, height, max_texture_size));
            return;
        }
    }

    /* Allocate buffer for pixel data (RGBA format, 4 bytes per pixel) */
    pixelbuffer = AllocVec(width * height * 4, MEMF_ANY);
    if (!pixelbuffer) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: Failed to allocate pixel buffer\n"));
        return;
    }

    /* Allocate buffer for Y-flipped data */
    flipped_buffer = AllocVec(width * height * 4, MEMF_ANY);
    if (!flipped_buffer) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: Failed to allocate flipped buffer\n"));
        FreeVec(pixelbuffer);
        return;
    }

    /* Read pixels from source RastPort */
    ReadPixelArray(pixelbuffer, 0, 0, width * 4,
                   src_rp, src_x, src_y, width, height, RECTFMT_RGBA);

    /* Legacy graphics.library draws often come back with alpha=0; force opaque */
    for (ULONG i = 3; i < (ULONG)width * height * 4; i += 4) {
        pixelbuffer[i] = 0xFF;
    }

    /*
     * Flip the image vertically because:
     * - Screen coordinates have Y=0 at top
     * - OpenGL has Y=0 at bottom
     * Our ortho projection flips rendering, but textures still need manual flip.
     */
    for (row = 0; row < height; row++) {
        src_row = row * width * 4;
        dst_row = (height - 1 - row) * width * 4;
        CopyMem(pixelbuffer + src_row, flipped_buffer + dst_row, width * 4);
    }

    /* Create a temporary texture to hold the RastPort contents */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    /* Set texture parameters for pixel-perfect rendering */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    /* Upload the pixel data to the texture */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, flipped_buffer);

    /* Draw the texture at the destination position */
    glEnable(GL_TEXTURE_2D);

    /* Use white color so texture colors come through unchanged */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    /* Note: Our ortho projection has Y flipped (0 at top, height at bottom) */
    glTexCoord2f(0.0f, 1.0f); glVertex2i(dst_x, dst_y);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(dst_x + width, dst_y);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(dst_x + width, dst_y + height);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(dst_x, dst_y + height);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    /* Clean up the texture */
    glDeleteTextures(1, &texture);

    FreeVec(flipped_buffer);
    FreeVec(pixelbuffer);

    D(bug("[ZuneRenderer:OpenGL] OpenGLCopyFromRastPort: Copy complete\n"));
}

/*****************************************************************************/
/* Backend Operations Table                                                  */
/*****************************************************************************/

ZuneBackendOps opengl_backend_ops = {
    .name = OPENGL_BACKEND_NAME,
    .type = BACKEND_OPENGL,
    .GetCapabilities = OpenGLGetCapabilities,

    .InitBackend = OpenGLInitBackend,
    .CleanupBackend = OpenGLCleanupBackend,
    .IsAvailable = OpenGLIsAvailable,
    .IsCompatible = OpenGLIsCompatible,

    .InitRenderPort = OpenGLInitRenderPort,
    .CleanupRenderPort = OpenGLCleanupRenderPort,

    .PrepareColor = OpenGLPrepareColor,
    .ReleaseColor = OpenGLReleaseColor,

    .DrawPixel = OpenGLDrawPixel,
    .DrawLine = OpenGLDrawLine,
    .DrawRectangle = OpenGLDrawRectangle,
    .DrawCircle = OpenGLDrawCircle,

    .ClearRenderPort = OpenGLClearRenderPort,

    .LockPixels = OpenGLLockPixels,
    .UnlockPixels = OpenGLUnlockPixels,
    .GetPixel = OpenGLGetPixel,
    .SetPixel = OpenGLSetPixel,

    .BeginBatch = OpenGLBeginBatch,
    .EndBatch = OpenGLEndBatch,
    .FlushBatch = OpenGLFlushBatch,
    .IsBatching = OpenGLIsBatching,

    .BlitRenderPorts = OpenGLBlitRenderPorts,
    .BlitToScreen = OpenGLBlitToScreen,

    .InitDrawingBoard = OpenGLInitDrawingBoard,
    .CleanupDrawingBoard = OpenGLCleanupDrawingBoard,
    .CopyFromDrawingBoard = OpenGL_SyncFBOToBitmap,

    .CopyFromRastPort = OpenGLCopyFromRastPort,

    .reserved = {NULL}
};

/*****************************************************************************/
/* Library Management                                                        */
/*****************************************************************************/

/*
 * OpenGL_CheckLibrary - Check if GL library is available
 *
 * The gl.library is opened centrally in DetectLibraries() (zunerenderer_core.c).
 * This function just checks if it's available and stores a reference.
 *
 * Returns TRUE if library is available.
 */
BOOL OpenGL_CheckLibrary(OpenGLPrivateData *priv)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: Checking gl.library\n"));

    if (!priv) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: NULL priv pointer\n"));
        return FALSE;
    }

    /* GLBase is opened in DetectLibraries() */
    if (GLBase) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: gl.library v%ld.%ld available\n",
              GLBase->lib_Version, GLBase->lib_Revision));

        priv->GLBase = GLBase;
        priv->gl_available = TRUE;

        return TRUE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckLibrary: gl.library not available\n"));

    priv->gl_available = FALSE;
    return FALSE;
}

/*
 * OpenGL_CheckCapabilities - Query GL capabilities
 *
 * Must be called after a GL context is current!
 * Queries the GL implementation for its capabilities.
 */
BOOL OpenGL_CheckCapabilities(OpenGLPrivateData *priv)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckCapabilities\n"));

    if (!priv || !priv->gl_available) {
        return FALSE;
    }

    /*
     * Note: We can't query GL capabilities until we have a valid context.
     * This function should be called after the first context is created.
     * For now, we set reasonable defaults.
     */

    priv->gl_version_major = 1;
    priv->gl_version_minor = 1;
    priv->max_texture_size = 1024;      /* Conservative default */
    priv->has_npot_textures = FALSE;    /* Assume no NPOT support */
    priv->has_framebuffers = FALSE;     /* Will check when context available */
    priv->has_shaders = FALSE;          /* Will check when context available */

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CheckCapabilities: Defaults set\n"));
    D(bug("[ZuneRenderer:OpenGL]   Version: %ld.%ld\n",
          priv->gl_version_major, priv->gl_version_minor));
    D(bug("[ZuneRenderer:OpenGL]   Max texture size: %ld\n",
          priv->max_texture_size));

    return TRUE;
}

/*****************************************************************************/
/* Debug/Info Functions                                                      */
/*****************************************************************************/

void OpenGL_DumpDebugInfo(OpenGLPrivateData *priv)
{
    D(bug("=== OpenGL Backend Debug Info ===\n"));

    if (!priv) {
        D(bug("  Status: Not initialized (NULL priv)\n"));
        D(bug("=== End OpenGL Debug Info ===\n"));
        return;
    }

    D(bug("  Status: %s\n", priv->initialized ? "Initialized" : "Not initialized"));
    D(bug("  GL Available: %s\n", priv->gl_available ? "Yes" : "No"));

    if (priv->GLBase) {
        D(bug("  gl.library: v%ld.%ld\n",
              priv->GLBase->lib_Version, priv->GLBase->lib_Revision));
    } else {
        D(bug("  gl.library: Not open\n"));
    }

    D(bug("  Global Context: %s (ptr=%p)\n",
          priv->context_created ? "Created" : "Not created",
          priv->gl_context));
    D(bug("  Current Window: %p (%dx%d)\n",
          priv->current_window, priv->current_width, priv->current_height));
    D(bug("  GL Version: %ld.%ld\n",
          priv->gl_version_major, priv->gl_version_minor));
    D(bug("  Max Texture Size: %ld\n", priv->max_texture_size));
    D(bug("  NPOT Textures: %s\n", priv->has_npot_textures ? "Yes" : "No"));
    D(bug("  Framebuffers: %s\n", priv->has_framebuffers ? "Yes" : "No"));
    D(bug("  Shaders: %s\n", priv->has_shaders ? "Yes" : "No"));
    D(bug("  Draw Calls: %ld\n", priv->draw_calls));
    D(bug("  SetRast Calls: %ld\n", priv->setrast_calls));
    D(bug("  Capabilities: 0x%08lx\n", OpenGLGetCapabilities()));

    D(bug("=== End OpenGL Debug Info ===\n"));
}

/*****************************************************************************/
/* Shader Functions                                                          */
/*****************************************************************************/

/*
 * OpenGL_LoadShaderFunctions - Load shader function pointers via glAGetProcAddress
 *
 * This must be called after a GL context is current.
 * Returns TRUE if all required shader functions were loaded.
 */
static BOOL OpenGL_LoadShaderFunctions(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadShaderFunctions: Loading shader functions\n"));

    glCreateShader_ptr = (PFNGLCREATESHADERPROC)glAGetProcAddress("glCreateShader");
    glShaderSource_ptr = (PFNGLSHADERSOURCEPROC)glAGetProcAddress("glShaderSource");
    glCompileShader_ptr = (PFNGLCOMPILESHADERPROC)glAGetProcAddress("glCompileShader");
    glGetShaderInfoLog_ptr = (PFNGLGETSHADERINFOLOGPROC)glAGetProcAddress("glGetShaderInfoLog");
    glGetShaderiv_ptr = (PFNGLGETSHADERIVPROC)glAGetProcAddress("glGetShaderiv");
    glCreateProgram_ptr = (PFNGLCREATEPROGRAMPROC)glAGetProcAddress("glCreateProgram");
    glAttachShader_ptr = (PFNGLATTACHSHADERPROC)glAGetProcAddress("glAttachShader");
    glLinkProgram_ptr = (PFNGLLINKPROGRAMPROC)glAGetProcAddress("glLinkProgram");
    glGetProgramInfoLog_ptr = (PFNGLGETPROGRAMINFOLOGPROC)glAGetProcAddress("glGetProgramInfoLog");
    glGetProgramiv_ptr = (PFNGLGETPROGRAMIVPROC)glAGetProcAddress("glGetProgramiv");
    glUseProgram_ptr = (PFNGLUSEPROGRAMPROC)glAGetProcAddress("glUseProgram");
    glDetachShader_ptr = (PFNGLDETACHSHADERPROC)glAGetProcAddress("glDetachShader");
    glDeleteShader_ptr = (PFNGLDELETESHADERPROC)glAGetProcAddress("glDeleteShader");
    glDeleteProgram_ptr = (PFNGLDELETEPROGRAMPROC)glAGetProcAddress("glDeleteProgram");
    glGetUniformLocation_ptr = (PFNGLGETUNIFORMLOCATIONPROC)glAGetProcAddress("glGetUniformLocation");
    glUniform1f_ptr = (PFNGLUNIFORM1FPROC)glAGetProcAddress("glUniform1f");
    glUniform2f_ptr = (PFNGLUNIFORM2FPROC)glAGetProcAddress("glUniform2f");
    glUniform4f_ptr = (PFNGLUNIFORM4FPROC)glAGetProcAddress("glUniform4f");

    /* Check if all required functions were loaded */
    if (!glCreateShader_ptr || !glShaderSource_ptr || !glCompileShader_ptr ||
        !glCreateProgram_ptr || !glAttachShader_ptr || !glLinkProgram_ptr ||
        !glUseProgram_ptr || !glGetUniformLocation_ptr ||
        !glUniform1f_ptr || !glUniform2f_ptr || !glUniform4f_ptr) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadShaderFunctions: Some functions not available\n"));
        D(bug("[ZuneRenderer:OpenGL]   glCreateShader: %p\n", glCreateShader_ptr));
        D(bug("[ZuneRenderer:OpenGL]   glShaderSource: %p\n", glShaderSource_ptr));
        D(bug("[ZuneRenderer:OpenGL]   glCompileShader: %p\n", glCompileShader_ptr));
        D(bug("[ZuneRenderer:OpenGL]   glCreateProgram: %p\n", glCreateProgram_ptr));
        D(bug("[ZuneRenderer:OpenGL]   glUseProgram: %p\n", glUseProgram_ptr));
        return FALSE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadShaderFunctions: All shader functions loaded\n"));
    return TRUE;
}

/*
 * OpenGL_LoadVBOFunctions - Load VBO function pointers via glAGetProcAddress
 *
 * This must be called after a GL context is current.
 * Returns TRUE if VBO functions are available.
 */
static BOOL OpenGL_LoadVBOFunctions(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadVBOFunctions: Loading VBO functions\n"));

    if (g_vbo_available) {
        return TRUE;
    }

    glGenBuffers_ptr = (PFNGLGENBUFFERSPROC)glAGetProcAddress("glGenBuffers");
    glDeleteBuffers_ptr = (PFNGLDELETEBUFFERSPROC)glAGetProcAddress("glDeleteBuffers");
    glBindBuffer_ptr = (PFNGLBINDBUFFERPROC)glAGetProcAddress("glBindBuffer");
    glBufferData_ptr = (PFNGLBUFFERDATAPROC)glAGetProcAddress("glBufferData");
    glEnableVertexAttribArray_ptr = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glAGetProcAddress("glEnableVertexAttribArray");
    glDisableVertexAttribArray_ptr = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)glAGetProcAddress("glDisableVertexAttribArray");
    glVertexAttribPointer_ptr = (PFNGLVERTEXATTRIBPOINTERPROC)glAGetProcAddress("glVertexAttribPointer");
    glGetAttribLocation_ptr = (PFNGLGETATTRIBLOCATIONPROC)glAGetProcAddress("glGetAttribLocation");

    /* Try ARB versions if core not available */
    if (!glGenBuffers_ptr) {
        glGenBuffers_ptr = (PFNGLGENBUFFERSPROC)glAGetProcAddress("glGenBuffersARB");
        glDeleteBuffers_ptr = (PFNGLDELETEBUFFERSPROC)glAGetProcAddress("glDeleteBuffersARB");
        glBindBuffer_ptr = (PFNGLBINDBUFFERPROC)glAGetProcAddress("glBindBufferARB");
        glBufferData_ptr = (PFNGLBUFFERDATAPROC)glAGetProcAddress("glBufferDataARB");
    }

    if (!glGenBuffers_ptr || !glBindBuffer_ptr || !glBufferData_ptr) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadVBOFunctions: VBO functions not available\n"));
        return FALSE;
    }

    g_vbo_available = TRUE;
    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadVBOFunctions: VBO functions loaded\n"));
    return TRUE;
}

/*
 * OpenGL_CreateQuadVBO - Create a shared VBO for unit quad rendering
 *
 * Returns TRUE on success.
 */
static BOOL OpenGL_CreateQuadVBO(void)
{
    if (!g_vbo_available || !glGenBuffers_ptr || !glBindBuffer_ptr || !glBufferData_ptr) {
        return FALSE;
    }

    if (g_quad_vbo != 0) {
        return TRUE;  /* Already created */
    }

    glGenBuffers_ptr(1, &g_quad_vbo);
    if (g_quad_vbo == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateQuadVBO: glGenBuffers failed\n"));
        return FALSE;
    }

    glBindBuffer_ptr(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData_ptr(GL_ARRAY_BUFFER, sizeof(g_quad_vertices), g_quad_vertices, GL_STATIC_DRAW);
    glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateQuadVBO: Created VBO %d\n", g_quad_vbo));
    return TRUE;
}

/*
 * OpenGL_DestroyQuadVBO - Destroy the shared quad VBO
 */
static void OpenGL_DestroyQuadVBO(void)
{
    if (g_quad_vbo != 0 && glDeleteBuffers_ptr) {
        glDeleteBuffers_ptr(1, &g_quad_vbo);
        g_quad_vbo = 0;
        D(bug("[ZuneRenderer:OpenGL] OpenGL_DestroyQuadVBO: VBO destroyed\n"));
    }
}

/*
 * OpenGL_CompileShader - Compile a shader from source
 *
 * Returns shader ID on success, 0 on failure.
 */
static GLuint OpenGL_CompileShader(GLenum type, const GLchar *source)
{
    GLuint shader;
    GLint compiled;
    char infoLog[512];
    GLsizei logLength;

    if (!glCreateShader_ptr || !glShaderSource_ptr || !glCompileShader_ptr) {
        return 0;
    }

    shader = glCreateShader_ptr(type);
    if (shader == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CompileShader: glCreateShader failed\n"));
        return 0;
    }

    glShaderSource_ptr(shader, 1, &source, NULL);
    glCompileShader_ptr(shader);

    /* Check compilation status */
    if (glGetShaderiv_ptr) {
        glGetShaderiv_ptr(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            if (glGetShaderInfoLog_ptr) {
                glGetShaderInfoLog_ptr(shader, sizeof(infoLog), &logLength, infoLog);
                D(bug("[ZuneRenderer:OpenGL] Shader compile error: %s\n", infoLog));
            }
            if (glDeleteShader_ptr) {
                glDeleteShader_ptr(shader);
            }
            return 0;
        }
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CompileShader: Shader %d compiled successfully\n", shader));
    return shader;
}

/*
 * OpenGL_CreateRoundedRectShader - Create the rounded rectangle shader program
 *
 * Returns TRUE on success.
 */
static BOOL OpenGL_CreateRoundedRectShader(void)
{
    GLint linked;
    char infoLog[512];
    GLsizei logLength;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateRoundedRectShader: Creating shader program\n"));

    if (!glCreateProgram_ptr || !glAttachShader_ptr || !glLinkProgram_ptr) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateRoundedRectShader: Missing shader functions\n"));
        return FALSE;
    }

    /* Compile vertex shader */
    g_rounded_rect_vs = OpenGL_CompileShader(GL_VERTEX_SHADER, g_rounded_rect_vs_source);
    if (g_rounded_rect_vs == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateRoundedRectShader: Vertex shader failed\n"));
        return FALSE;
    }

    /* Compile fragment shader */
    g_rounded_rect_fs = OpenGL_CompileShader(GL_FRAGMENT_SHADER, g_rounded_rect_fs_source);
    if (g_rounded_rect_fs == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateRoundedRectShader: Fragment shader failed\n"));
        if (glDeleteShader_ptr) glDeleteShader_ptr(g_rounded_rect_vs);
        g_rounded_rect_vs = 0;
        return FALSE;
    }

    /* Create and link program */
    g_rounded_rect_program = glCreateProgram_ptr();
    if (g_rounded_rect_program == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateRoundedRectShader: glCreateProgram failed\n"));
        if (glDeleteShader_ptr) {
            glDeleteShader_ptr(g_rounded_rect_vs);
            glDeleteShader_ptr(g_rounded_rect_fs);
        }
        g_rounded_rect_vs = 0;
        g_rounded_rect_fs = 0;
        return FALSE;
    }

    glAttachShader_ptr(g_rounded_rect_program, g_rounded_rect_vs);
    glAttachShader_ptr(g_rounded_rect_program, g_rounded_rect_fs);
    glLinkProgram_ptr(g_rounded_rect_program);

    /* Check link status */
    if (glGetProgramiv_ptr) {
        glGetProgramiv_ptr(g_rounded_rect_program, GL_LINK_STATUS, &linked);
        if (!linked) {
            if (glGetProgramInfoLog_ptr) {
                glGetProgramInfoLog_ptr(g_rounded_rect_program, sizeof(infoLog), &logLength, infoLog);
                D(bug("[ZuneRenderer:OpenGL] Program link error: %s\n", infoLog));
            }
            OpenGL_DestroyShaders();
            return FALSE;
        }
    }

    /* Get uniform locations */
    g_uniform_rect_size = glGetUniformLocation_ptr(g_rounded_rect_program, "u_size");
    g_uniform_rect_radius = glGetUniformLocation_ptr(g_rounded_rect_program, "u_radius");
    g_uniform_fill_color = glGetUniformLocation_ptr(g_rounded_rect_program, "u_fill_color");
    g_uniform_border_color = glGetUniformLocation_ptr(g_rounded_rect_program, "u_border_color");
    g_uniform_border_width = glGetUniformLocation_ptr(g_rounded_rect_program, "u_border_width");
    g_uniform_has_fill = glGetUniformLocation_ptr(g_rounded_rect_program, "u_has_fill");
    g_uniform_has_border = glGetUniformLocation_ptr(g_rounded_rect_program, "u_has_border");

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateRoundedRectShader: Program %d created\n", g_rounded_rect_program));
    D(bug("[ZuneRenderer:OpenGL]   u_size: %d\n", g_uniform_rect_size));
    D(bug("[ZuneRenderer:OpenGL]   u_radius: %d\n", g_uniform_rect_radius));
    D(bug("[ZuneRenderer:OpenGL]   u_fill_color: %d\n", g_uniform_fill_color));
    D(bug("[ZuneRenderer:OpenGL]   u_border_color: %d\n", g_uniform_border_color));
    D(bug("[ZuneRenderer:OpenGL]   u_border_width: %d\n", g_uniform_border_width));
    D(bug("[ZuneRenderer:OpenGL]   u_has_fill: %d\n", g_uniform_has_fill));
    D(bug("[ZuneRenderer:OpenGL]   u_has_border: %d\n", g_uniform_has_border));

    g_shaders_available = TRUE;
    return TRUE;
}

/*
 * OpenGL_DestroyShaders - Clean up shader resources
 */
static void OpenGL_DestroyShaders(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_DestroyShaders\n"));

    if (g_rounded_rect_program && glUseProgram_ptr) {
        glUseProgram_ptr(0);
    }

    if (g_rounded_rect_program && glDetachShader_ptr) {
        if (g_rounded_rect_vs) glDetachShader_ptr(g_rounded_rect_program, g_rounded_rect_vs);
        if (g_rounded_rect_fs) glDetachShader_ptr(g_rounded_rect_program, g_rounded_rect_fs);
    }

    if (g_rounded_rect_vs && glDeleteShader_ptr) {
        glDeleteShader_ptr(g_rounded_rect_vs);
        g_rounded_rect_vs = 0;
    }

    if (g_rounded_rect_fs && glDeleteShader_ptr) {
        glDeleteShader_ptr(g_rounded_rect_fs);
        g_rounded_rect_fs = 0;
    }

    if (g_rounded_rect_program && glDeleteProgram_ptr) {
        glDeleteProgram_ptr(g_rounded_rect_program);
        g_rounded_rect_program = 0;
    }

    g_shaders_available = FALSE;
    g_uniform_rect_size = -1;
    g_uniform_rect_radius = -1;
    g_uniform_fill_color = -1;
    g_uniform_border_color = -1;
    g_uniform_border_width = -1;
    g_uniform_has_fill = -1;
    g_uniform_has_border = -1;
}

/*
 * OpenGL_InitShaders - Initialize shaders after context creation
 *
 * Call this after the first GL context is created and made current.
 */
static BOOL OpenGL_InitShaders(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_InitShaders\n"));

    if (g_shaders_available) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_InitShaders: Shaders already initialized\n"));
        return TRUE;
    }

    /* Load shader function pointers */
    if (!OpenGL_LoadShaderFunctions()) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_InitShaders: Failed to load shader functions\n"));
        return FALSE;
    }

    /* Create the rounded rectangle shader program */
    if (!OpenGL_CreateRoundedRectShader()) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_InitShaders: Failed to create rounded rect shader\n"));
        return FALSE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_InitShaders: Shaders initialized successfully\n"));
    return TRUE;
}

/*****************************************************************************/
/* FBO (Framebuffer Object) Functions                                        */
/*****************************************************************************/

/*
 * OpenGL_LoadFBOFunctions - Load FBO function pointers via glAGetProcAddress
 *
 * This must be called after a GL context is current.
 * Returns TRUE if FBO functions are available.
 */
static BOOL OpenGL_LoadFBOFunctions(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadFBOFunctions: Loading FBO functions\n"));

    /* Already loaded? */
    if (g_fbo_available) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadFBOFunctions: FBO already initialized\n"));
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
        D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadFBOFunctions: Trying EXT versions\n"));
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

    /* Log all function pointers for debugging */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadFBOFunctions: Function pointers:\n"));
    D(bug("[ZuneRenderer:OpenGL]   glGenFramebuffers: %p\n", glGenFramebuffers_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glDeleteFramebuffers: %p\n", glDeleteFramebuffers_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glBindFramebuffer: %p\n", glBindFramebuffer_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glCheckFramebufferStatus: %p\n", glCheckFramebufferStatus_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glFramebufferTexture2D: %p\n", glFramebufferTexture2D_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glGenRenderbuffers: %p\n", glGenRenderbuffers_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glBindRenderbuffer: %p\n", glBindRenderbuffer_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glRenderbufferStorage: %p\n", glRenderbufferStorage_ptr));
    D(bug("[ZuneRenderer:OpenGL]   glFramebufferRenderbuffer: %p\n", glFramebufferRenderbuffer_ptr));

    /* Check if minimum required functions were loaded */
    if (!glGenFramebuffers_ptr || !glDeleteFramebuffers_ptr ||
        !glBindFramebuffer_ptr || !glCheckFramebufferStatus_ptr ||
        !glFramebufferTexture2D_ptr) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadFBOFunctions: FBO functions not available\n"));
        g_fbo_available = FALSE;
        return FALSE;
    }

    g_fbo_available = TRUE;
    D(bug("[ZuneRenderer:OpenGL] OpenGL_LoadFBOFunctions: FBO functions loaded successfully\n"));
    return TRUE;
}

/*
 * OpenGL_CreateFBO - Create a new Framebuffer Object
 *
 * Creates an FBO with a color texture attachment for off-screen rendering.
 * Returns the FBO data structure, or NULL on failure.
 */
static OpenGLFBOData *OpenGL_CreateFBO(UWORD width, UWORD height)
{
    OpenGLFBOData *fbo;
    GLuint fbo_id, texture_id;
    GLenum status;
    GLenum gl_error;
    GLint max_texture_size = 0;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Creating FBO %dx%d\n", width, height));

    /* Validate dimensions - must be non-zero */
    if (width == 0 || height == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Invalid dimensions (zero)\n"));
        return NULL;
    }

    if (!g_fbo_available || !glGenFramebuffers_ptr) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: FBO not available\n"));
        return NULL;
    }

    /* Ensure GL context is current before any GL operations */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        const GLubyte *gl_version, *gl_renderer, *gl_extensions;

        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Making context %p current\n", g_opengl_priv->gl_context));
        glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);

        /* Log GL info for debugging */
        gl_version = glGetString(GL_VERSION);
        gl_renderer = glGetString(GL_RENDERER);
        gl_extensions = glGetString(GL_EXTENSIONS);
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: GL_VERSION: %s\n", gl_version ? (char*)gl_version : "NULL"));
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: GL_RENDERER: %s\n", gl_renderer ? (char*)gl_renderer : "NULL"));

        /* Check if glGetString returned NULL - indicates no valid context */
        if (!gl_version) {
            D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: ERROR - glGetString(GL_VERSION) returned NULL!\n"));
            D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: This means the GL context is not properly current.\n"));
            return NULL;
        }

        /* Check maximum texture size and validate dimensions */
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: GL_MAX_TEXTURE_SIZE = %d\n", max_texture_size));
        if (max_texture_size > 0 && ((GLint)width > max_texture_size || (GLint)height > max_texture_size)) {
            D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Requested size %dx%d exceeds max texture size %d\n",
                  width, height, max_texture_size));
            return NULL;
        }

        /* Check for FBO extension */
        if (gl_extensions) {
            if (strstr((char*)gl_extensions, "GL_ARB_framebuffer_object")) {
                D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: GL_ARB_framebuffer_object supported\n"));
            } else if (strstr((char*)gl_extensions, "GL_EXT_framebuffer_object")) {
                D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: GL_EXT_framebuffer_object supported\n"));
            } else {
                D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: WARNING - No FBO extension found in GL_EXTENSIONS!\n"));
            }
        }
    } else {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: ERROR - No GL context available!\n"));
        return NULL;
    }

    /* Clear any pending GL errors */
    while ((gl_error = glGetError()) != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Clearing pending GL error 0x%04x\n", gl_error));
    }

    /* Allocate FBO data structure */
    fbo = AllocVec(sizeof(OpenGLFBOData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!fbo) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Failed to allocate FBO structure\n"));
        return NULL;
    }

    /* Generate FBO */
    glGenFramebuffers_ptr(1, &fbo_id);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glGenFramebuffers GL error 0x%04x\n", gl_error));
    }
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Generated FBO id=%d\n", fbo_id));
    if (fbo_id == 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glGenFramebuffers failed\n"));
        FreeVec(fbo);
        return NULL;
    }

    /* Bind the FBO */
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo_id);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glBindFramebuffer GL error 0x%04x\n", gl_error));
    }

    /* Create color texture attachment */
    glGenTextures(1, &texture_id);
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Generated texture id=%d\n", texture_id));
    glBindTexture(GL_TEXTURE_2D, texture_id);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glBindTexture GL error 0x%04x\n", gl_error));
    }

    /*
     * Create texture for FBO color attachment.
     *
     * AROS Mesa/SoftPipe has issues with certain format combinations and
     * crashes in _mesa_error -> fprintf when it encounters unsupported formats.
     * We use the most basic GL 1.1 compatible format specification.
     */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Calling glTexImage2D %dx%d\n", width, height));

    /* Set texture parameters BEFORE uploading data - some drivers require this */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glTexParameteri GL error 0x%04x\n", gl_error));
    }

    /* Use GL_RGBA with GL_UNSIGNED_BYTE - most compatible combination */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glTexImage2D GL error 0x%04x\n", gl_error));
        /* If GL_RGBA fails, the FBO creation will fail - clean up */
        glDeleteTextures(1, &texture_id);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers_ptr(1, &fbo_id);
        FreeVec(fbo);
        return NULL;
    }

    /* Attach texture to FBO */
    glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glFramebufferTexture2D GL error 0x%04x\n", gl_error));
    }

    /* Set draw buffer to the color attachment */
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glDrawBuffer GL error 0x%04x\n", gl_error));
    }

    /* Set read buffer to the color attachment */
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glReadBuffer GL error 0x%04x\n", gl_error));
    }

    /* Check FBO completeness */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Calling glCheckFramebufferStatus_ptr at %p with GL_FRAMEBUFFER=0x%04x\n",
          glCheckFramebufferStatus_ptr, GL_FRAMEBUFFER));

    /* Flush any pending operations before checking status */
    glFlush();

    status = glCheckFramebufferStatus_ptr(GL_FRAMEBUFFER);
    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glCheckFramebufferStatus returned 0x%04x (COMPLETE=0x%04x)\n",
          status, GL_FRAMEBUFFER_COMPLETE));

    /* Also try checking with GL_DRAW_FRAMEBUFFER if status is 0 */
    if (status == 0) {
        #ifndef GL_DRAW_FRAMEBUFFER
        #define GL_DRAW_FRAMEBUFFER 0x8CA9
        #endif
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Status was 0, trying GL_DRAW_FRAMEBUFFER\n"));
        status = glCheckFramebufferStatus_ptr(GL_DRAW_FRAMEBUFFER);
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) returned 0x%04x\n", status));
    }
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: FBO incomplete, status=0x%04x\n", status));
        switch (status) {
            case 0:
                D(bug("[ZuneRenderer:OpenGL]   Status 0 usually means no GL context is current!\n"));
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                D(bug("[ZuneRenderer:OpenGL]   GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n"));
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                D(bug("[ZuneRenderer:OpenGL]   GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n"));
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                D(bug("[ZuneRenderer:OpenGL]   GL_FRAMEBUFFER_UNSUPPORTED\n"));
                break;
            default:
                D(bug("[ZuneRenderer:OpenGL]   Unknown status\n"));
                break;
        }
        glDeleteTextures(1, &texture_id);
        glDeleteFramebuffers_ptr(1, &fbo_id);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
        FreeVec(fbo);
        return NULL;
    }

    /* Unbind FBO (return to default framebuffer) */
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

    /* Fill in FBO data */
    fbo->fbo_id = fbo_id;
    fbo->texture_id = texture_id;
    fbo->depth_rb_id = 0;
    fbo->width = width;
    fbo->height = height;
    fbo->valid = TRUE;
    fbo->dirty = FALSE;  /* Not dirty until we draw to it */
    fbo->parent_context = NULL;

    if (g_opengl_priv) {
        g_opengl_priv->fbos_created++;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateFBO: Created FBO %d with texture %d\n", fbo_id, texture_id));
    return fbo;
}

/*
 * OpenGL_DestroyFBO - Destroy a Framebuffer Object
 */
static void OpenGL_DestroyFBO(OpenGLFBOData *fbo)
{
    if (!fbo) return;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_DestroyFBO: Destroying FBO %d\n", fbo->fbo_id));

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
static BOOL OpenGL_BindFBO(OpenGLFBOData *fbo)
{
    if (!fbo || !fbo->valid || !glBindFramebuffer_ptr) {
        return FALSE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_BindFBO: Binding FBO %d (%dx%d)\n",
          fbo->fbo_id, fbo->width, fbo->height));

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
static void OpenGL_UnbindFBO(void)
{
    if (!glBindFramebuffer_ptr) return;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_UnbindFBO: Unbinding FBO\n"));
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
}

/*****************************************************************************/
/* Window Context Functions                                                  */
/*****************************************************************************/

/*
 * OpenGL_CreateWindowContext - Create a GL context for a window
 *
 * Each window gets its own GL context for independent rendering.
 */
static OpenGLWindowContext *OpenGL_CreateWindowContext(struct Window *window)
{
    OpenGLWindowContext *ctx;
    struct TagItem tags[10];
    int tag_idx = 0;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateWindowContext: Window %p\n", window));

    if (!window || !GLBase) {
        return NULL;
    }

    /* Allocate context structure */
    ctx = AllocVec(sizeof(OpenGLWindowContext), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ctx) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateWindowContext: Failed to allocate context\n"));
        return NULL;
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

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create GL context */
    ctx->gl_context = glACreateContext(tags);
    if (!ctx->gl_context) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateWindowContext: glACreateContext failed\n"));
        FreeVec(ctx);
        return NULL;
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

    D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateWindowContext: Created context %p for window %p\n",
          ctx->gl_context, window));

    return ctx;
}

/*
 * OpenGL_DestroyWindowContext - Destroy a window's GL context
 */
static void OpenGL_DestroyWindowContext(OpenGLWindowContext *ctx)
{
    OpenGLWindowContext **prev;

    if (!ctx) return;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_DestroyWindowContext: Context %p\n", ctx));

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
}

/*
 * OpenGL_FindWindowContext - Find the GL context for a window
 */
static OpenGLWindowContext *OpenGL_FindWindowContext(struct Window *window)
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
static BOOL OpenGL_MakeContextCurrent(OpenGLWindowContext *ctx)
{
    if (!ctx || !ctx->gl_context || !ctx->context_valid) {
        return FALSE;
    }

    /* Skip if already current */
    if (g_opengl_priv && g_opengl_priv->current_context == ctx) {
        return TRUE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGL_MakeContextCurrent: Context %p (window %p)\n",
          ctx->gl_context, ctx->window));

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

/*****************************************************************************/
/* Public Helper Functions                                                   */
/*****************************************************************************/

/*
 * OpenGL_SwapBuffers - Swap the OpenGL framebuffer to screen
 *
 * This function is called from zunerenderer_drawingboard.c when blitting
 * an OpenGL DrawingBoard to screen. For OpenGL, we don't do traditional
 * bitmap blitting - we just swap the GL framebuffer.
 *
 * This is an exported function that can be called from other modules.
 */
void OpenGL_SwapBuffers(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGL_SwapBuffers\n"));

    if (!g_opengl_priv || !g_opengl_priv->gl_context) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SwapBuffers: No GL context available\n"));
        return;
    }

    glFlush();
    glASwapBuffers((GLAContext)g_opengl_priv->gl_context);

    D(bug("[ZuneRenderer:OpenGL] OpenGL_SwapBuffers: Buffers swapped\n"));
}

/*
 * OpenGL_BlitToRastPortDirect - Blit GL framebuffer to RastPort using glASetRast
 *
 * This is the efficient way to blit OpenGL content to a RastPort. Instead of
 * using glReadPixels (which does a slow GPU->CPU->GPU roundtrip), we use
 * glASetRast to temporarily change the GL context's target RastPort, then
 * call glASwapBuffers which uses Mesa's BltPipeResourceRastPort for a direct
 * GPU-to-RastPort transfer.
 *
 * Parameters:
 *   dst_rp - Destination RastPort (e.g., window's RastPort)
 *   dst_x, dst_y - Destination coordinates in the RastPort
 *   width, height - Size of area to blit
 */
void OpenGL_BlitToRastPortDirect(struct RastPort *dst_rp, WORD dst_x, WORD dst_y,
                                 UWORD width, UWORD height)
{
    struct TagItem setrast_tags[8];
    int tag_idx = 0;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitToRastPortDirect: dst=%p (%d,%d) %dx%d\n",
          dst_rp, dst_x, dst_y, width, height));

    if (!g_opengl_priv || !g_opengl_priv->gl_context || !dst_rp) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitToRastPortDirect: Invalid parameters\n"));
        return;
    }

    /*
     * Use glASetRast to temporarily change the GL context's target to the
     * destination RastPort. We use GLA_RastPort with explicit dimensions
     * since we're blitting to a RastPort that may not have a Window.
     */
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

    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitToRastPortDirect: Calling glASetRast to change target\n"));

    /* Change the GL context's visible_rp to the destination RastPort */
    glASetRast((GLAContext)g_opengl_priv->gl_context, setrast_tags);

    /* Flush and swap - this will use BltPipeResourceRastPort internally */
    glFlush();
    glASwapBuffers((GLAContext)g_opengl_priv->gl_context);

    g_opengl_priv->setrast_calls++;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitToRastPortDirect: Blit complete (setrast_calls=%ld)\n",
          g_opengl_priv->setrast_calls));

    /*
     * Note: After this call, the GL context's visible_rp points to dst_rp.
     * The next drawing operation will switch back to the DrawingBoard's
     * RastPort via OpenGL_SwitchToDrawingBoard if needed.
     *
     * We don't restore here because:
     * 1. It would add an extra glASetRast call
     * 2. The next draw operation handles switching anyway
     * 3. The DrawingBoard's framebuffer content is preserved in GL memory
     */
}

/*
 * OpenGL_BlitFBOToRastPort - Blit FBO contents to a RastPort
 *
 * This function reads pixels from an FBO using glReadPixels and writes
 * them to the destination RastPort using WritePixelArray. This is the
 * safe method that avoids glASetRast crashes.
 *
 * Parameters:
 *   board - Source DrawingBoard with FBO (backend_data must be valid)
 *   dst_rp - Destination RastPort
 *   src_x, src_y - Source coordinates in the FBO
 *   dst_x, dst_y - Destination coordinates in the RastPort
 *   width, height - Size of area to blit
 */
void OpenGL_BlitFBOToRastPort(struct DrawingBoard *board, struct RastPort *dst_rp,
                              WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                              UWORD width, UWORD height)
{
    OpenGLFBOData *fbo;
    UBYTE *pixelbuffer;
    UBYTE *flipped_buffer;
    ULONG row, src_row, dst_row;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: board=%p src(%d,%d) -> dst(%d,%d) %dx%d\n",
          board, src_x, src_y, dst_x, dst_y, width, height));

    if (!board || !board->backend_data || !dst_rp) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Invalid parameters\n"));
        return;
    }

    if (!g_fbo_available || !glBindFramebuffer_ptr) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: FBO not available\n"));
        return;
    }

    if (!CyberGfxBase) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: CyberGfxBase not available\n"));
        return;
    }

    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo->valid) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: FBO not valid\n"));
        return;
    }

    /* Clamp dimensions to FBO size */
    if (src_x < 0) { dst_x -= src_x; width += src_x; src_x = 0; }
    if (src_y < 0) { dst_y -= src_y; height += src_y; src_y = 0; }
    if (src_x + width > fbo->width) width = fbo->width - src_x;
    if (src_y + height > fbo->height) height = fbo->height - src_y;

    if (width <= 0 || height <= 0) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Nothing to blit after clamping\n"));
        return;
    }

    /* Allocate buffers */
    pixelbuffer = AllocVec(width * height * 4, MEMF_ANY);
    if (!pixelbuffer) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Failed to allocate pixel buffer\n"));
        return;
    }

    flipped_buffer = AllocVec(width * height * 4, MEMF_ANY);
    if (!flipped_buffer) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Failed to allocate flipped buffer\n"));
        FreeVec(pixelbuffer);
        return;
    }

    /* Bind the FBO for reading */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Binding FBO %d for reading\n", fbo->fbo_id));
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo->fbo_id);

    /* Ensure GL operations are complete */
    glFlush();
    glFinish();

    /*
     * Read pixels from FBO
     * Note: glReadPixels reads from bottom-left, and OpenGL has Y=0 at bottom
     * We need to flip Y for screen coordinates
     */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Reading pixels from FBO at (%d,%d) size %dx%d\n",
          src_x, fbo->height - src_y - height, width, height));
    glReadPixels(src_x, fbo->height - src_y - height, width, height,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelbuffer);

    /* Debug: Check first few pixels */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: First pixel RGBA: %02x %02x %02x %02x\n",
          pixelbuffer[0], pixelbuffer[1], pixelbuffer[2], pixelbuffer[3]));
    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Pixel at (100,100) RGBA: %02x %02x %02x %02x\n",
          pixelbuffer[(100 * width + 100) * 4 + 0],
          pixelbuffer[(100 * width + 100) * 4 + 1],
          pixelbuffer[(100 * width + 100) * 4 + 2],
          pixelbuffer[(100 * width + 100) * 4 + 3]));

    /* Unbind FBO - and invalidate state so next SwitchToTarget re-binds */
    glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

    /* IMPORTANT: Invalidate current target state so next draw operation
     * will properly re-bind the FBO. Without this, OpenGL_SwitchToDrawingBoard
     * would skip the bind because it thinks we're already on the right target. */
    if (g_opengl_priv) {
        g_opengl_priv->current_target_type = OPENGL_TARGET_NONE;
        g_opengl_priv->current_board = NULL;
        g_opengl_priv->current_window = NULL;
    }

    /*
     * Flip the image vertically because:
     * - OpenGL has Y=0 at bottom
     * - Screen coordinates have Y=0 at top
     */
    for (row = 0; row < height; row++) {
        src_row = row * width * 4;
        dst_row = (height - 1 - row) * width * 4;
        CopyMem(pixelbuffer + src_row, flipped_buffer + dst_row, width * 4);
    }

    /* Write to destination RastPort */
    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Writing to RastPort at (%d,%d)\n", dst_x, dst_y));
    WritePixelArray(flipped_buffer, 0, 0, width * 4,
                    dst_rp, dst_x, dst_y,
                    width, height, RECTFMT_RGBA);

    FreeVec(flipped_buffer);
    FreeVec(pixelbuffer);

    D(bug("[ZuneRenderer:OpenGL] OpenGL_BlitFBOToRastPort: Blit complete\n"));
}

/*
 * OpenGL_SyncFBOToBitmap - Sync FBO contents to DrawingBoard's bitmap
 *
 * This is a convenience wrapper around OpenGL_BlitFBOToRastPort that
 * syncs the entire FBO to the DrawingBoard's own RastPort/bitmap.
 *
 * This is essential for mixed-mode rendering where both OpenGL and
 * CyberGfx draw to the same DrawingBoard.
 */
BOOL OpenGL_SyncFBOToBitmap(struct RenderPort *rp)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    /* Validate RenderPort */
    if (!rp) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: NULL RenderPort\n"));
        return FALSE;
    }

    board = rp->target_board;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: board=%p\n", board));

    /* Validate DrawingBoard */
    if (!board || !board->valid) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: Invalid or NULL board\n"));
        return FALSE;
    }

    if (!board->backend_data) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: No backend_data (FBO)\n"));
        return FALSE;
    }

    if (!board->rastport || !board->rastport->BitMap) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: No rastport or bitmap\n"));
        return FALSE;
    }

    fbo = (OpenGLFBOData *)board->backend_data;

    /* Validate FBO */
    if (!fbo->valid) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: FBO not valid\n"));
        return FALSE;
    }

    /*
     * Only sync if the FBO has been modified since last sync.
     * This avoids expensive glReadPixels + WritePixelArray operations
     * when the FBO content hasn't changed (e.g., during window resize
     * when blitting unchanged content).
     */
    if (!fbo->dirty) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFBOToBitmap: FBO not dirty, skipping sync\n"));
        return TRUE;
    }

    /* Use existing blit function to sync entire FBO to board's rastport */
    OpenGL_BlitFBOToRastPort(board, board->rastport,
                              0, 0, 0, 0,
                              board->width, board->height);

    /* Mark FBO as clean after successful sync */
    fbo->dirty = FALSE;

    return TRUE;
}

/*****************************************************************************/
/* Backend Implementation - Lifecycle                                        */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx)
{
    OpenGLPrivateData *priv;

    D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend\n"));

    if (!ctx) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: NULL context\n"));
        return FALSE;
    }

    /* Allocate private data */
    priv = AllocVec(sizeof(OpenGLPrivateData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!priv) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: Failed to allocate private data\n"));
        return FALSE;
    }

    /* Check if GL library is available (opened in DetectLibraries) */
    if (!OpenGL_CheckLibrary(priv)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: GL library not available\n"));
        FreeVec(priv);
        return FALSE;
    }

    /* Set default capabilities */
    OpenGL_CheckCapabilities(priv);

    /* Initialize global context state */
    priv->gl_context = NULL;
    priv->context_created = FALSE;
    priv->current_window = NULL;
    priv->current_width = 0;
    priv->current_height = 0;

    /* Mark as initialized */
    priv->initialized = TRUE;
    priv->draw_calls = 0;
    priv->setrast_calls = 0;

    /* Store private data in context AND global pointer */
    ctx->private_data = priv;
    g_opengl_priv = priv;  /* Global access for drawing functions */

    ctx->capabilities = BACKEND_CAP_BASIC |
                        BACKEND_CAP_OPENGL |
                        BACKEND_CAP_HARDWARE |
                        BACKEND_CAP_BLENDING |
                        BACKEND_CAP_ANTIALIASING |
                        BACKEND_CAP_TEXTURES;
    ctx->initialized = TRUE;

    D(bug("[ZuneRenderer:OpenGL] OpenGLInitBackend: Backend initialized successfully\n"));
    OpenGL_DumpDebugInfo(priv);

    return TRUE;
}

static void OpenGLCleanupBackend(ZuneBackendContext *ctx)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupBackend\n"));

    if (!ctx || !ctx->private_data) {
        return;
    }

    OpenGLPrivateData *priv = (OpenGLPrivateData *)ctx->private_data;

    /* Destroy VBO before destroying the GL context */
    OpenGL_DestroyQuadVBO();

    /* Destroy shaders before destroying the GL context */
    OpenGL_DestroyShaders();

    /* Destroy the global GL context if it exists */
    if (priv->gl_context) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupBackend: Destroying global GL context %p\n",
              priv->gl_context));
        glADestroyContext((GLAContext)priv->gl_context);
        priv->gl_context = NULL;
        priv->context_created = FALSE;
    }

    /* GL library is closed centrally in CleanupZuneRenderer() */
    priv->GLBase = NULL;
    priv->gl_available = FALSE;

    /* Clear global pointer */
    g_opengl_priv = NULL;

    /* Free private data */
    FreeVec(priv);
    ctx->private_data = NULL;
    ctx->initialized = FALSE;

    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupBackend: Cleanup complete\n"));
}

static BOOL OpenGLIsAvailable(void)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLIsAvailable: Checking for gl.library\n"));

    /*
     * Check if GLBase was opened in DetectLibraries().
     */
    if (GLBase) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLIsAvailable: gl.library v%ld.%ld found\n",
              GLBase->lib_Version, GLBase->lib_Revision));
        return TRUE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGLIsAvailable: gl.library not found\n"));
    return FALSE;
}

static BOOL OpenGLIsCompatible(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: RenderPort %p\n", rp));

    if (!rp) {
        /* NULL rp means checking general compatibility */
        return OpenGLIsAvailable();
    }

    /*
     * NEW ARCHITECTURE: OpenGL compatibility is based on having a Window.
     *
     * OpenGL requires a Window to create a GL context. The RenderPort should
     * have rp->window set (via CreateRenderPortForWindow) for OpenGL to work.
     *
     * With the new architecture:
     * - RenderPort is bound to a Window (required for GL context)
     * - DrawingBoards always have BitMap (for legacy compatibility)
     * - OpenGL adds FBO to DrawingBoard for accelerated rendering
     * - Switching targets uses glBindFramebuffer() (fast)
     */

    /* Check if RenderPort has a window - required for GL context */
    if (rp->window) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: Has window %p - compatible\n",
              rp->window));
        return TRUE;
    }

    /*
     * Legacy path: Check for DrawingBoard with parent_window
     */
    if (rp->target_board && rp->target_board->parent_window) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: DrawingBoard has parent_window %p - compatible\n",
              rp->target_board->parent_window));
        return TRUE;
    }

    /*
     * No Window means we can't create a GL context.
     * Fall back to CyberGraphics.
     */
    D(bug("[ZuneRenderer:OpenGL] OpenGLIsCompatible: No window - NOT compatible\n"));
    return FALSE;
}

static ULONG OpenGLGetCapabilities(void)
{
    if (!OpenGLIsAvailable()) {
        return 0;
    }

    return BACKEND_CAP_BASIC |
           BACKEND_CAP_OPENGL |
           BACKEND_CAP_HARDWARE |
           BACKEND_CAP_BLENDING |
           BACKEND_CAP_ANTIALIASING |
           BACKEND_CAP_TEXTURES;
}

/*****************************************************************************/
/* GL Context Management Helpers                                             */
/*****************************************************************************/

/*
 * OpenGL_EnsureGlobalContext - Create the single global GL context if needed
 *
 * Mesa3DGL on AROS only supports ONE GL context. This function creates it
 * on first use, bound to the given window. Subsequent calls to different
 * windows will use glASetRast() to switch the render target.
 *
 * Returns TRUE if global context is available.
 */
static BOOL OpenGL_EnsureGlobalContext(struct Window *window)
{
    struct TagItem tags[10];
    int tag_idx = 0;
    GLAContext gl_ctx;

    if (!g_opengl_priv || !GLBase) {
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: No priv or GLBase\n"));
        return FALSE;
    }

    /* If context already exists, we're good */
    if (g_opengl_priv->context_created && g_opengl_priv->gl_context) {
        return TRUE;
    }

    /* Need a window to create the initial context */
    if (!window) {
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: No window for initial context\n"));
        return FALSE;
    }

    D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: Creating global GL context for window %p\n", window));

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

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create the ONE global context */
    gl_ctx = glACreateContext(tags);
    if (!gl_ctx) {
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: glACreateContext FAILED!\n"));
        return FALSE;
    }

    D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: Global context created: %p\n", gl_ctx));

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

    /* Set up initial orthographic projection */
    OpenGL_SetupOrthoProjection(g_opengl_priv->current_width, g_opengl_priv->current_height);

    /* Initialize shaders now that we have a context */
    if (OpenGL_InitShaders()) {
        if (g_opengl_priv) {
            g_opengl_priv->has_shaders = TRUE;
        }
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: Shaders initialized\n"));
    } else {
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: Shaders not available (will use fallback)\n"));
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

        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: Testing FBO support with small texture\n"));

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
                    D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: FBO test PASSED\n"));
                } else {
                    D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: FBO test FAILED - status 0x%04x\n", test_status));
                }
            } else {
                D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: FBO test FAILED - glTexImage2D error\n"));
            }

            /* Cleanup test resources */
            glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
            glDeleteTextures(1, &test_tex);
            glDeleteFramebuffers_ptr(1, &test_fbo);
        }

        if (fbo_works && g_opengl_priv) {
            g_opengl_priv->has_framebuffers = TRUE;
            D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: FBO support enabled\n"));
        } else {
            g_fbo_available = FALSE;
            if (g_opengl_priv) {
                g_opengl_priv->has_framebuffers = FALSE;
            }
            D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: FBO support DISABLED (test failed)\n"));
        }
    } else {
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: FBO not available (DrawingBoard will use CyberGfx fallback)\n"));
    }

    /* Initialize VBO for efficient quad rendering */
    if (OpenGL_LoadVBOFunctions()) {
        if (OpenGL_CreateQuadVBO()) {
            D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: VBO initialized\n"));
        } else {
            D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: VBO creation failed\n"));
        }
    } else {
        D(bug("[ZuneRenderer:OpenGL] EnsureGlobalContext: VBO not available\n"));
    }

    return TRUE;
}

/*
 * OpenGL_SwitchToWindow - Switch the global GL context to render to a different window
 *
 * Uses glASetRast() to change the render target without creating a new context.
 * This is the key to supporting multiple windows with Mesa3DGL's single-context limitation.
 *
 * Returns TRUE if switch was successful.
 */
static BOOL OpenGL_SwitchToWindow(struct RenderPort *rp)
{
    struct Window *window = NULL;
    struct RastPort *rastport;
    UWORD width, height;

    if (!rp || !g_opengl_priv) {
        return FALSE;
    }

    /* Get the target window */
    if (rp->target_rp) {
        rastport = rp->target_rp;
        if (rastport->Layer && rastport->Layer->Window) {
            window = (struct Window *)rastport->Layer->Window;
        }
    }

    if (!window) {
        D(bug("[ZuneRenderer:OpenGL] SwitchToWindow: No window available for RenderPort %p\n", rp));
        return FALSE;
    }

    /* Ensure global context exists (creates it if this is the first window) */
    if (!OpenGL_EnsureGlobalContext(window)) {
        D(bug("[ZuneRenderer:OpenGL] SwitchToWindow: Failed to ensure global context\n"));
        return FALSE;
    }

    /* Calculate window dimensions */
    width = window->Width - window->BorderLeft - window->BorderRight;
    height = window->Height - window->BorderTop - window->BorderBottom;

    /* Check if we need to switch targets (including from DrawingBoard to Window) */
    if (g_opengl_priv->current_target_type != OPENGL_TARGET_WINDOW ||
        g_opengl_priv->current_window != window ||
        g_opengl_priv->current_width != width ||
        g_opengl_priv->current_height != height) {

        struct TagItem setrast_tags[6];
        int tag_idx = 0;

        D(bug("[ZuneRenderer:OpenGL] SwitchToWindow: Switching from window %p to %p (%dx%d)\n",
              g_opengl_priv->current_window, window, width, height));

        /* Build tags for glASetRast */
        setrast_tags[tag_idx].ti_Tag = GLA_Window;
        setrast_tags[tag_idx].ti_Data = (IPTR)window;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Left;
        setrast_tags[tag_idx].ti_Data = window->BorderLeft;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Top;
        setrast_tags[tag_idx].ti_Data = window->BorderTop;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Right;
        setrast_tags[tag_idx].ti_Data = window->BorderRight;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = GLA_Bottom;
        setrast_tags[tag_idx].ti_Data = window->BorderBottom;
        tag_idx++;

        setrast_tags[tag_idx].ti_Tag = TAG_DONE;
        setrast_tags[tag_idx].ti_Data = 0;

        /* Switch render target */
        glASetRast((GLAContext)g_opengl_priv->gl_context, setrast_tags);

        /* Update state */
        g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
        g_opengl_priv->current_window = window;
        g_opengl_priv->current_board = NULL;
        g_opengl_priv->current_width = width;
        g_opengl_priv->current_height = height;
        g_opengl_priv->setrast_calls++;

        /* Make context current (may be needed after SetRast) */
        glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);

        /* Re-setup orthographic projection for new dimensions */
        OpenGL_SetupOrthoProjection(width, height);

        /* Mark that we need to sync from RastPort before drawing */
        g_opengl_priv->needs_sync = TRUE;

        D(bug("[ZuneRenderer:OpenGL] SwitchToWindow: Switch complete (setrast_calls=%ld)\n",
              g_opengl_priv->setrast_calls));
    }

    return TRUE;
}

/*
 * OpenGL_SwitchToDrawingBoard - Switch the global GL context to render to a DrawingBoard
 *
 * Uses glASetRast() with GLA_RastPort and explicit dimensions to switch
 * the render target to an off-screen DrawingBoard.
 *
 * Returns TRUE if switch was successful.
 */
static BOOL OpenGL_SwitchToDrawingBoard(struct RenderPort *rp)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    if (!rp || !rp->target_board || !g_opengl_priv) {
        return FALSE;
    }

    board = rp->target_board;

    /*
     * FBO-based DrawingBoard switching
     *
     * Instead of using glASetRast (which causes crashes when switching between
     * targets with different dimensions), we use FBOs:
     * - Each DrawingBoard gets its own FBO
     * - Switching is done via glBindFramebuffer (very fast)
     * - No internal Mesa framebuffer reallocation needed
     */
    if (board->width == 0 || board->height == 0) {
        D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Invalid DrawingBoard dimensions (%dx%d)\n",
              board->width, board->height));
        return FALSE;
    }

    /*
     * We need a GL context before we can create/use FBOs.
     * If no context exists yet, we need to create one first.
     * This requires a window - check multiple sources:
     * 1. DrawingBoard's parent_window (set by user)
     * 2. RenderPort's target_rp Layer->Window
     */
    if (!g_opengl_priv->context_created) {
        struct Window *window = NULL;

        /* First, check if DrawingBoard has a parent_window set */
        if (board->parent_window) {
            window = board->parent_window;
            D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Using board->parent_window %p\n", window));
        }
        /* Otherwise, try to get window from RenderPort's target_rp */
        else if (rp->target_rp && rp->target_rp->Layer && rp->target_rp->Layer->Window) {
            window = (struct Window *)rp->target_rp->Layer->Window;
            D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Using target_rp->Layer->Window %p\n", window));
        }

        if (window) {
            /* Create context for the window */
            if (!OpenGL_EnsureGlobalContext(window)) {
                D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Failed to create window context\n"));
                return FALSE;
            }
        } else {
            /*
             * No window available - this is an error in the new architecture.
             * DrawingBoards must be created via CreateDrawingBoardForRenderPort()
             * which requires a RenderPort that was created via CreateRenderPortForWindow().
             * This ensures every DrawingBoard has an associated window for GL context creation.
             */
            D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: ERROR - No window available!\n"));
            D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: DrawingBoards require a window (use CreateDrawingBoardForRenderPort)\n"));
            return FALSE;
        }
    }

    /* Check if FBO support is available */
    if (!g_fbo_available) {
        /*
         * FBO not available (e.g., SoftPipe renderer).
         *
         * The legacy glASetRast method doesn't work reliably for off-screen
         * DrawingBoards because Mesa/SoftPipe can only render to window
         * RastPorts, not arbitrary bitmaps.
         *
         * Return FALSE to trigger fallback to CyberGfx/software rendering
         * for DrawingBoard operations. This ensures rendering actually works.
         */
        D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: FBO not available, falling back to CyberGfx\n"));
        return FALSE;
    }

    /* Get or create FBO for this DrawingBoard */
    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo) {
        /* Create FBO for this DrawingBoard */
        D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Creating FBO for board %p (%dx%d)\n",
              board, board->width, board->height));

        fbo = OpenGL_CreateFBO(board->width, board->height);
        if (!fbo) {
            D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Failed to create FBO\n"));
            return FALSE;
        }

        /* Store FBO in DrawingBoard */
        board->backend_data = fbo;

        /* Track parent context for cleanup */
        fbo->parent_context = g_opengl_priv->current_context;

        D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Created FBO %d for board %p\n",
              fbo->fbo_id, board));
    }

    /* Check if we need to switch to this FBO */
    if (g_opengl_priv->current_target_type != OPENGL_TARGET_DRAWINGBOARD ||
        g_opengl_priv->current_board != board) {

        D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Binding FBO %d for board %p (%dx%d)\n",
              fbo->fbo_id, board, board->width, board->height));

        /* Bind the FBO - this is much faster than glASetRast! */
        if (!OpenGL_BindFBO(fbo)) {
            D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Failed to bind FBO\n"));
            return FALSE;
        }

        /* Update state */
        g_opengl_priv->current_target_type = OPENGL_TARGET_DRAWINGBOARD;
        g_opengl_priv->current_board = board;
        g_opengl_priv->current_window = NULL;
        g_opengl_priv->current_width = board->width;
        g_opengl_priv->current_height = board->height;

        /* DrawingBoards don't need sync - they start fresh */
        g_opengl_priv->needs_sync = FALSE;

        D(bug("[ZuneRenderer:OpenGL] SwitchToDrawingBoard: Switch complete (fbo_switches=%ld)\n",
              g_opengl_priv->fbo_switches));
    }

    return TRUE;
}

/*
 * OpenGL_SwitchToTarget - Unified function to switch GL context to any target
 *
 * Determines if the RenderPort targets a Window or DrawingBoard and calls
 * the appropriate switching function.
 *
 * Returns TRUE if switch was successful.
 */
static BOOL OpenGL_SwitchToTarget(struct RenderPort *rp)
{
    BOOL result;

    if (!rp || !g_opengl_priv) {
        D(bug("[ZuneRenderer:OpenGL] SwitchToTarget: Invalid rp or g_opengl_priv\n"));
        return FALSE;
    }

    /* Check if this is a DrawingBoard target */
    if (rp->target_board) {
        D(bug("[ZuneRenderer:OpenGL] SwitchToTarget: Switching to DrawingBoard %p\n", rp->target_board));
        result = OpenGL_SwitchToDrawingBoard(rp);
        D(bug("[ZuneRenderer:OpenGL] SwitchToTarget: SwitchToDrawingBoard returned %d, current_target_type=%d\n",
              result, g_opengl_priv->current_target_type));
        return result;
    }

    /* Otherwise it's a Window-based RastPort */
    D(bug("[ZuneRenderer:OpenGL] SwitchToTarget: Switching to Window\n"));
    return OpenGL_SwitchToWindow(rp);
}

/*
 * Set up 2D orthographic projection for pixel-perfect rendering
 */
static void OpenGL_SetupOrthoProjection(UWORD width, UWORD height)
{
    D(bug("[ZuneRenderer:OpenGL] Setting up ortho projection %dx%d\n", width, height));

    /* Set viewport to full render area */
    glViewport(0, 0, width, height);

    /* Set up orthographic projection */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* Y-axis flipped: 0 at top, height at bottom (like 2D screen coords) */
    glOrtho(0, width, height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Enable blending for alpha */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Disable depth test for 2D */
    glDisable(GL_DEPTH_TEST);
}

/*
 * Set current GL color from InternalColor
 */
static inline void OpenGL_SetColor(struct InternalColor *color)
{
    if (!color) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    glColor4ub(color->r, color->g, color->b, color->a);
}

/*
 * Copy RastPort contents into OpenGL buffer
 *
 * This is needed because OpenGL has its own framebuffer that is separate
 * from the RastPort. When we swap buffers, we overwrite everything.
 * By copying the RastPort contents first, we preserve any graphics.library
 * rendering that was done before OpenGL operations.
 *
 * We use a texture-based approach instead of glDrawPixels, which is more
 * reliable across different GL implementations.
 */
static void OpenGL_SyncFromRastPort(struct RenderPort *rp)
{
    struct Window *window;
    struct RastPort *rastport;
    UWORD width, height;
    UBYTE *pixelbuffer;
    WORD x_offset, y_offset;
    GLuint texture;
    ULONG row, src_row, dst_row;
    UBYTE *flipped_buffer;

    if (!rp || !rp->target_rp || !CyberGfxBase || !g_opengl_priv) {
        return;
    }

    rastport = rp->target_rp;
    if (!rastport->Layer || !rastport->Layer->Window) {
        return;
    }

    window = (struct Window *)rastport->Layer->Window;
    x_offset = window->BorderLeft;
    y_offset = window->BorderTop;
    width = window->Width - window->BorderLeft - window->BorderRight;
    height = window->Height - window->BorderTop - window->BorderBottom;

    D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFromRastPort: Syncing %dx%d pixels\n", width, height));

    /* Validate dimensions */
    if (width == 0 || height == 0) {
        return;
    }

    /* Check maximum texture size to avoid Mesa errors */
    {
        GLint max_texture_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (max_texture_size > 0 && ((GLint)width > max_texture_size || (GLint)height > max_texture_size)) {
            D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFromRastPort: Size %dx%d exceeds max texture size %d\n",
                  width, height, max_texture_size));
            return;
        }
    }

    /* Allocate buffer for pixel data (RGBA format, 4 bytes per pixel) */
    pixelbuffer = AllocVec(width * height * 4, MEMF_ANY);
    if (!pixelbuffer) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFromRastPort: Failed to allocate pixel buffer\n"));
        return;
    }

    /* Allocate buffer for Y-flipped data */
    flipped_buffer = AllocVec(width * height * 4, MEMF_ANY);
    if (!flipped_buffer) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFromRastPort: Failed to allocate flipped buffer\n"));
        FreeVec(pixelbuffer);
        return;
    }

    /* Read pixels from RastPort in RGBA format (more compatible with GL) */
    ReadPixelArray(pixelbuffer, 0, 0, width * 4,
                   rastport, x_offset, y_offset,
                   width, height, RECTFMT_RGBA);

    /*
     * Flip the image vertically because:
     * - Screen coordinates have Y=0 at top
     * - OpenGL has Y=0 at bottom
     * Our ortho projection flips rendering, but textures still need manual flip.
     */
    for (row = 0; row < height; row++) {
        src_row = row * width * 4;
        dst_row = (height - 1 - row) * width * 4;
        CopyMem(pixelbuffer + src_row, flipped_buffer + dst_row, width * 4);
    }

    /* Create a temporary texture to hold the RastPort contents */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    /* Set texture parameters for pixel-perfect rendering */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    /* Upload the pixel data to the texture */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, flipped_buffer);

    /* Draw the texture as a fullscreen quad */
    glEnable(GL_TEXTURE_2D);

    /* Use white color so texture colors come through unchanged */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    /* Note: Our ortho projection has Y flipped (0 at top, height at bottom) */
    glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(width, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(width, height);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(0, height);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    /* Clean up the texture */
    glDeleteTextures(1, &texture);

    FreeVec(flipped_buffer);
    FreeVec(pixelbuffer);

    D(bug("[ZuneRenderer:OpenGL] OpenGL_SyncFromRastPort: Sync complete\n"));
}

/*
 * Check if we need to sync from RastPort and do it if necessary.
 * This should be called before any OpenGL drawing operation.
 */
static void OpenGL_SyncIfNeeded(struct RenderPort *rp)
{
    if (!g_opengl_priv || !g_opengl_priv->needs_sync) {
        return;
    }

//    OpenGL_SyncFromRastPort(rp);
    g_opengl_priv->needs_sync = FALSE;
}

/*
 * Flush OpenGL rendering to make it visible
 *
 * In non-batched mode, we need to flush and swap after each operation
 * to make the rendering visible immediately.
 */
static void OpenGL_FlushIfNotBatching(struct RenderPort *rp)
{
    if (!rp || rp->batching_enabled) {
        return;  /* Don't flush during batching */
    }

    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();

        /*
         * Call glASwapBuffers to present rendered content to screen.
         * This is only needed for window rendering - DrawingBoards use FBOs
         * which don't need swap buffers (content is read via glReadPixels).
         *
         * Note: When FBOs are not available, DrawingBoard rendering falls back
         * to CyberGfx, so we never have OPENGL_TARGET_DRAWINGBOARD without FBO.
         */
        if (g_opengl_priv->current_target_type == OPENGL_TARGET_WINDOW) {
            glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
            /* After swapping, we need to sync again next time */
            g_opengl_priv->needs_sync = TRUE;
        }
    }
}

/*****************************************************************************/
/* RenderPort Management                                                     */
/*****************************************************************************/

static BOOL OpenGLInitRenderPort(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLInitRenderPort: RenderPort %p\n", rp));

    if (!rp) {
        return FALSE;
    }

    /*
     * With the global context approach, we don't need per-RenderPort context.
     * The global context is created on first use and shared across all windows.
     * glASetRast() is used to switch render targets as needed.
     */
    rp->backend_context = NULL;

    return TRUE;
}

static void OpenGLCleanupRenderPort(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupRenderPort: RenderPort %p\n", rp));

    if (!rp) {
        return;
    }

    /*
     * With global context, nothing to clean up per-RenderPort.
     * The global context is destroyed in OpenGLCleanupBackend().
     */
}

/*****************************************************************************/
/* Color Management                                                          */
/*****************************************************************************/

static BOOL OpenGLPrepareColor(struct RenderPort *rp,
                               struct InternalColor *color)
{
    if (!color) {
        return FALSE;
    }

    /*
     * OpenGL uses the color components directly via glColor4ub().
     * No pen allocation needed.
     */
    color->pen = -1;
    color->pen_allocated = FALSE;

    return TRUE;
}

static void OpenGLReleaseColor(struct RenderPort *rp,
                               struct InternalColor *color)
{
    /* Nothing to release for OpenGL colors */
}

/*****************************************************************************/
/* Drawing Operations                                                        */
/*****************************************************************************/

static void OpenGLDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawPixel: (%d,%d)\n", x, y));

    if (!rp || !color) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rp)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawPixel: SwitchToTarget failed, using fallback\n"));
        ZuneFallback_DrawPixel(rp, x, y, color, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rp);
    OpenGL_SetColor(color);

    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();

    OpenGL_FlushIfNotBatching(rp);
}

static void OpenGLDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawLine: (%d,%d) to (%d,%d) width=%d\n",
          startX, startY, endX, endY, width));

    if (!rp || !color) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rp)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawLine: SwitchToTarget failed, using fallback\n"));
        ZuneFallback_DrawLine(rp, startX, startY, endX, endY, width, color, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rp);
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

    OpenGL_FlushIfNotBatching(rp);
}

/*
 * Helper to draw a quarter circle arc (for rounded corners)
 * quadrant: 0=top-right, 1=bottom-right, 2=bottom-left, 3=top-left
 */
static void OpenGL_DrawQuarterCircle(WORD cx, WORD cy, UWORD radius, int quadrant, BOOL filled)
{
    #define CORNER_SEGMENTS 16
    int i;
    float angle, angle_start, angle_step;

    /* Each quadrant is 90 degrees (PI/2) */
    angle_start = quadrant * 3.14159265f / 2.0f;
    angle_step = (3.14159265f / 2.0f) / CORNER_SEGMENTS;

    if (filled) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2i(cx, cy);  /* Center point */
        for (i = 0; i <= CORNER_SEGMENTS; i++) {
            angle = angle_start + i * angle_step;
            glVertex2f(cx + radius * cosf(angle),
                      cy - radius * sinf(angle));  /* Y is flipped in screen coords */
        }
        glEnd();
    } else {
        glBegin(GL_LINE_STRIP);
        for (i = 0; i <= CORNER_SEGMENTS; i++) {
            angle = angle_start + i * angle_step;
            glVertex2f(cx + radius * cosf(angle),
                      cy - radius * sinf(angle));
        }
        glEnd();
    }
    #undef CORNER_SEGMENTS
}

static void OpenGLDrawRectangle(struct RenderPort *rp, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius, struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawRectangle: (%d,%d) %dx%d filled=%d radius=%d antialias=%d\n",
          x, y, width, height, filled, corner_radius, antialias));
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawRectangle: fill_brush=%p border_color=%p border_width=%d\n",
          fill_brush, border_color, border_width));

    if (!rp) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawRectangle: rp is NULL, returning\n"));
        return;
    }

    if (!OpenGL_SwitchToTarget(rp)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawRectangle: SwitchToTarget failed, using fallback\n"));
        ZuneFallback_DrawRectangle(rp, x, y, width, height, border_width, corner_radius,
                                   fill_brush, border_color, filled, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rp);

    /* Clamp corner radius to half of smallest dimension */
    if (corner_radius > width / 2) corner_radius = width / 2;
    if (corner_radius > height / 2) corner_radius = height / 2;

    /* Simple rectangle (no rounded corners) */
    if (corner_radius == 0) {
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
                /* No AA outline needed - straight edges look correct */
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

        /*
         * Shader-based rounded rectangle rendering using SDF
         *
         * The shader calculates a signed distance field for each pixel:
         * - Negative distance = inside the rounded rect
         * - Positive distance = outside
         * - Zero = on the edge
         *
         * This gives us perfect antialiasing on all edges and corners
         * without needing to tessellate the curves into line segments.
         */
        if (g_shaders_available && g_rounded_rect_program && glUseProgram_ptr) {
            ULONG fill_color_val = 0;
            float fill_r = 0, fill_g = 0, fill_b = 0, fill_a = 0;
            float border_r = 0, border_g = 0, border_b = 0, border_a = 0;
            BOOL has_fill = (filled && fill_brush && fill_brush->type == ZUNE_BRUSH_TYPE_SOLID);
            BOOL has_border = (border_width > 0 && border_color);

            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Using shader for rounded rect\n"));

            /* Extract fill color */
            if (has_fill) {
                fill_color_val = fill_brush->data.solid.color;
                fill_r = ((fill_color_val >> 16) & 0xFF) / 255.0f;
                fill_g = ((fill_color_val >> 8) & 0xFF) / 255.0f;
                fill_b = (fill_color_val & 0xFF) / 255.0f;
                fill_a = ((fill_color_val >> 24) & 0xFF) / 255.0f;
            }

            /* Extract border color */
            if (has_border) {
                border_r = border_color->r / 255.0f;
                border_g = border_color->g / 255.0f;
                border_b = border_color->b / 255.0f;
                border_a = border_color->a / 255.0f;
            }

            /* Activate shader program */
            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Calling glUseProgram(%d)\n", g_rounded_rect_program));
            glUseProgram_ptr(g_rounded_rect_program);
            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: glUseProgram done\n"));

            /* Set uniforms */
            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Setting uniforms\n"));
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
            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Uniforms set\n"));

            /* Draw the quad using VBO if available, otherwise immediate mode */
            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Drawing quad (%d,%d)-(%d,%d)\n", x, y, x + width, y + height));

            if (g_vbo_available && g_quad_vbo != 0 && glBindBuffer_ptr) {
                /*
                 * VBO-based rendering:
                 * - Unit quad (0-1) stored in VBO
                 * - Transform via modelview matrix to target position/size
                 * - Reduces driver overhead vs immediate mode
                 */
                glPushMatrix();
                glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);
                glScalef((GLfloat)width, (GLfloat)height, 1.0f);

                /* Bind VBO and set up vertex/texcoord pointers */
                glBindBuffer_ptr(GL_ARRAY_BUFFER, g_quad_vbo);

                glEnableClientState(GL_VERTEX_ARRAY);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);

                /* Vertex data: x, y, s, t - stride is 4 floats */
                glVertexPointer(2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)0);
                glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

                /* Draw the quad */
                glDrawArrays(GL_QUADS, 0, 4);

                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glDisableClientState(GL_VERTEX_ARRAY);

                glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);
                glPopMatrix();

                D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Quad drawn via VBO\n"));
            } else {
                /* Fallback to immediate mode */
                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
                glTexCoord2f(1.0f, 0.0f); glVertex2i(x + width, y);
                glTexCoord2f(1.0f, 1.0f); glVertex2i(x + width, y + height);
                glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + height);
                glEnd();
                D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Quad drawn via immediate mode\n"));
            }

            /* Deactivate shader */
            glUseProgram_ptr(0);
        } else {
            /*
             * Fallback: Draw rounded rectangle using geometry (no shaders)
             * This is the old implementation for systems without shader support.
             */
            WORD r = corner_radius;

            D(bug("[ZuneRenderer:OpenGL] DrawRectangle: Using geometry fallback for rounded rect\n"));

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
                        int i;
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
                int i;
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

    OpenGL_FlushIfNotBatching(rp);
}

static void OpenGLDrawCircle(struct RenderPort *rp, WORD center_x, WORD center_y,
                             UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias)
{
    /* Use more segments for smoother circles, especially for AA */
    #define CIRCLE_SEGMENTS 64
    int i;
    float angle, angle_step;

    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawCircle: center=(%d,%d) radius=%d filled=%d antialias=%d\n",
          center_x, center_y, radius, filled, antialias));
    D(bug("[ZuneRenderer:OpenGL] OpenGLDrawCircle: fill_brush=%p border_color=%p border_width=%d\n",
          fill_brush, border_color, border_width));

    if (!rp) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawCircle: rp is NULL, returning\n"));
        return;
    }

    if (!OpenGL_SwitchToTarget(rp)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawCircle: SwitchToTarget failed, using fallback\n"));
        ZuneFallback_DrawCircle(rp, center_x, center_y, radius, border_width,
                                fill_brush, border_color, filled, antialias);
        return;
    }

    OpenGL_SyncIfNeeded(rp);

    angle_step = 2.0f * 3.14159265f / CIRCLE_SEGMENTS;

    /* Handle fill */
    if (filled && fill_brush) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLDrawCircle: drawing fill, brush type=%d\n", fill_brush->type));
        /* For now, only support solid color fills */
        if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
            ULONG color = fill_brush->data.solid.color;
            D(bug("[ZuneRenderer:OpenGL] OpenGLDrawCircle: solid color=0x%08lx\n", color));
            glColor4ub(
                (color >> 16) & 0xFF,  /* R */
                (color >> 8) & 0xFF,   /* G */
                color & 0xFF,          /* B */
                (color >> 24) & 0xFF   /* A */
            );

            /* Draw filled circle using triangle fan */
            glBegin(GL_TRIANGLE_FAN);
            glVertex2i(center_x, center_y);  /* Center point */
            for (i = 0; i <= CIRCLE_SEGMENTS; i++) {
                angle = i * angle_step;
                glVertex2f(center_x + radius * cosf(angle),
                          center_y + radius * sinf(angle));
            }
            glEnd();

            /*
             * For AA: draw outline in same fill color to smooth edges.
             * GL_POLYGON_SMOOTH is unreliable, but GL_LINE_SMOOTH works well.
             * This masks the jagged fill edges with a smooth AA outline.
             */
            if (antialias) {
                glEnable(GL_LINE_SMOOTH);
                glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

                /* Same color as fill - already set */
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

        /* Draw circle outline using line loop */
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

    OpenGL_FlushIfNotBatching(rp);
    #undef CIRCLE_SEGMENTS
}

static void OpenGLClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLClearRenderPort\n"));

    if (!rp || !color) {
        return;
    }

    if (!OpenGL_SwitchToTarget(rp)) {
        D(bug("[ZuneRenderer:OpenGL] OpenGLClearRenderPort: SwitchToTarget failed, using fallback\n"));
        ZuneFallback_ClearRenderPort(rp, color);
        return;
    }

    /* Clear overwrites everything, no need to sync from RastPort */
    if (g_opengl_priv) {
        g_opengl_priv->needs_sync = FALSE;
    }

    /* Set clear color */
    glClearColor(
        color->r / 255.0f,
        color->g / 255.0f,
        color->b / 255.0f,
        color->a / 255.0f
    );

    glClear(GL_COLOR_BUFFER_BIT);

    OpenGL_FlushIfNotBatching(rp);
}

/*****************************************************************************/
/* Direct Pixel Access                                                       */
/*****************************************************************************/

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLLockPixels: Not fully supported\n"));

    /*
     * OpenGL doesn't support direct pixel access in the same way.
     * For now, return NULL. Future: could use glReadPixels to read
     * into a buffer.
     */
    if (pitch_out) {
        *pitch_out = 0;
    }
    return NULL;
}

static void OpenGLUnlockPixels(struct DrawingBoard *board)
{
    /* Nothing to unlock */
}

static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y)
{
    /* TODO: Implement using glReadPixels */
    D(bug("[ZuneRenderer:OpenGL] OpenGLGetPixel: Not implemented\n"));
    return 0x00000000;
}

static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color)
{
    /* TODO: Would need to draw a point via the RenderPort */
    D(bug("[ZuneRenderer:OpenGL] OpenGLSetPixel: Not implemented\n"));
}

/*****************************************************************************/
/* Batching Operations                                                       */
/*****************************************************************************/

static void OpenGLBeginBatch(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLBeginBatch\n"));

    /*
     * OpenGL naturally batches commands. We could use this to
     * defer glASwapBuffers until EndBatch.
     */
    if (rp) {
        rp->batching_enabled = TRUE;
    }
}

static void OpenGLEndBatch(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLEndBatch\n"));

    if (!rp) {
        return;
    }

    rp->batching_enabled = FALSE;

    /* Flush and swap buffers using global context */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();
        glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
    }
}

static void OpenGLFlushBatch(struct RenderPort *rp)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLFlushBatch\n"));

    if (!rp) {
        return;
    }

    /* Flush and swap buffers using global context */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();
        glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
    }
}

static BOOL OpenGLIsBatching(struct RenderPort *rp)
{
    if (!rp) {
        return FALSE;
    }
    return rp->batching_enabled;
}

/*****************************************************************************/
/* Blitting Operations                                                       */
/*****************************************************************************/

static void OpenGLBlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLBlitRenderPorts: %dx%d\n", width, height));

    /* TODO: Implement using FBOs or texture copying */
}

static void OpenGLBlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: src(%d,%d) -> dst(%d,%d) %dx%d\n",
          src_x, src_y, dest_x, dest_y, width, height));

    if (!source || !screen_rp) {
        return;
    }

    board = source->target_board;

    /*
     * FBO-based blitting
     *
     * If this DrawingBoard has an FBO, we need to:
     * 1. Bind the FBO to read from it
     * 2. Read pixels using glReadPixels
     * 3. Write pixels to the destination RastPort
     *
     * If no FBO, just swap buffers (window rendering path).
     */
    if (board && board->backend_data && g_fbo_available) {
        UBYTE *pixelbuffer;
        UBYTE *flipped_buffer;
        ULONG row, src_row, dst_row;

        fbo = (OpenGLFBOData *)board->backend_data;

        D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: FBO %d -> RastPort %p\n",
              fbo->fbo_id, screen_rp));

        if (!CyberGfxBase) {
            D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: CyberGfxBase not available\n"));
            return;
        }

        /* Clamp dimensions to FBO size */
        if (src_x + width > fbo->width) width = fbo->width - src_x;
        if (src_y + height > fbo->height) height = fbo->height - src_y;
        if (width == 0 || height == 0) return;

        /* Allocate buffers */
        pixelbuffer = AllocVec(width * height * 4, MEMF_ANY);
        if (!pixelbuffer) {
            D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: Failed to allocate pixel buffer\n"));
            return;
        }

        flipped_buffer = AllocVec(width * height * 4, MEMF_ANY);
        if (!flipped_buffer) {
            D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: Failed to allocate flipped buffer\n"));
            FreeVec(pixelbuffer);
            return;
        }

        /* Bind the FBO for reading */
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, fbo->fbo_id);

        /* Ensure GL operations are complete */
        glFlush();
        glFinish();

        /* Read pixels from FBO */
        glReadPixels(src_x, src_y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelbuffer);

        /* Unbind FBO */
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

        /*
         * Flip the image vertically because:
         * - OpenGL has Y=0 at bottom
         * - Screen coordinates have Y=0 at top
         */
        for (row = 0; row < height; row++) {
            src_row = row * width * 4;
            dst_row = (height - 1 - row) * width * 4;
            CopyMem(pixelbuffer + src_row, flipped_buffer + dst_row, width * 4);
        }

        /* Write to destination RastPort */
        WritePixelArray(flipped_buffer, 0, 0, width * 4,
                        screen_rp, dest_x, dest_y,
                        width, height, RECTFMT_RGBA);

        FreeVec(flipped_buffer);
        FreeVec(pixelbuffer);

        D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: FBO blit complete\n"));
    } else {
        /*
         * No FBO: rendering went directly to window's GL buffer.
         * Just swap buffers to make it visible.
         */
        D(bug("[ZuneRenderer:OpenGL] OpenGLBlitToScreen: Swap buffers (no FBO)\n"));
        if (g_opengl_priv && g_opengl_priv->gl_context) {
            glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
        }
    }
}

/*****************************************************************************/
/* DrawingBoard Management                                                   */
/*****************************************************************************/

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLInitDrawingBoard: %p (%dx%d)\n",
          board, board ? board->width : 0, board ? board->height : 0));

    if (!board) {
        return FALSE;
    }

    /*
     * FBO-based DrawingBoard initialization
     *
     * We don't create the FBO here because we need an active GL context first.
     * The FBO will be created lazily in OpenGL_SwitchToDrawingBoard when
     * rendering first targets this DrawingBoard.
     *
     * For now, we just:
     * 1. Mark as hardware surface (OpenGL uses GPU)
     * 2. Initialize backend_data to NULL (FBO will be stored here later)
     */
    board->hardware_surface = TRUE;
    board->backend_data = NULL;

    D(bug("[ZuneRenderer:OpenGL] OpenGLInitDrawingBoard: Initialized (FBO will be created on first use)\n"));

    return TRUE;
}

void OpenGLCleanupDrawingBoard(struct DrawingBoard *board)
{
    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupDrawingBoard: %p\n", board));

    if (!board) {
        return;
    }

    /*
     * Destroy the FBO if one was created for this DrawingBoard
     */
    if (board->backend_data) {
        OpenGLFBOData *fbo = (OpenGLFBOData *)board->backend_data;

        D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupDrawingBoard: Destroying FBO %d\n", fbo->fbo_id));

        /*
         * We need an active GL context to destroy the FBO.
         * If this board's parent context is still valid, make it current.
         */
        if (fbo->parent_context && fbo->parent_context->context_valid) {
            OpenGL_MakeContextCurrent(fbo->parent_context);
        } else if (g_opengl_priv && g_opengl_priv->gl_context) {
            /* Fall back to global context if available */
            glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);
        }

        OpenGL_DestroyFBO(fbo);
        board->backend_data = NULL;
    }

    /* Clear current board reference if this was it */
    if (g_opengl_priv && g_opengl_priv->current_board == board) {
        g_opengl_priv->current_board = NULL;
        g_opengl_priv->current_target_type = OPENGL_TARGET_NONE;
    }

    D(bug("[ZuneRenderer:OpenGL] OpenGLCleanupDrawingBoard: Cleanup complete\n"));
}
