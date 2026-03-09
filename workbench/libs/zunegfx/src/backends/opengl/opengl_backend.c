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
    - Each RenderContext/DrawingBoard gets its own GL context
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
#include <stdio.h>
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
static PFNGLUNIFORM1IPROC           glUniform1i_ptr         = NULL;
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

/* Textured rounded rect shader (for non-solid brushes) */
static GLuint g_rounded_rect_textured_program = 0;
static GLuint g_rounded_rect_textured_fs = 0;

/* Shader uniform locations - solid color shader */
static GLint g_uniform_rect_size = -1;
static GLint g_uniform_rect_radius = -1;
static GLint g_uniform_fill_color = -1;
static GLint g_uniform_border_color = -1;
static GLint g_uniform_border_width = -1;
static GLint g_uniform_has_border = -1;
static GLint g_uniform_has_fill = -1;

/* Shader uniform locations - textured shader */
static GLint g_uniform_tex_rect_size = -1;
static GLint g_uniform_tex_rect_radius = -1;
static GLint g_uniform_tex_fill_texture = -1;
static GLint g_uniform_tex_border_color = -1;
static GLint g_uniform_tex_border_width = -1;
static GLint g_uniform_tex_has_border = -1;
static GLint g_uniform_tex_has_fill = -1;

#define DEBUG 1
#include <aros/debug.h>

#include "../backend_interface.h"
#include "opengl_backend.h"

/*****************************************************************************/
/* Global GL Library Base                                                    */
/*****************************************************************************/

/*
 * GLBase is defined in zunegfx_init.c and declared in zunegfx_intern.h.
 * It is opened in DetectLibraries() in zunegfx_core.c.
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
static BOOL OpenGLIsCompatible(struct RenderContext *rctx);
static ULONG OpenGLGetCapabilities(void);
static ULONG OpenGLGetPixelFormat(struct BitMap *bitmap);

static BOOL OpenGLInitRenderContext(struct RenderContext *rctx);
static void OpenGLCleanupRenderContext(struct RenderContext *rctx);

static BOOL OpenGLPrepareColor(struct RenderContext *rctx,
                               struct InternalColor *color);
static void OpenGLReleaseColor(struct RenderContext *rctx,
                               struct InternalColor *color);

static void OpenGLDrawPixel(struct RenderContext *rctx, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias);
static void OpenGLDrawLine(struct RenderContext *rctx, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias);
static void OpenGLDrawRectangle(struct RenderContext *rctx, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius, struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias);
static void OpenGLDrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y,
                             UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias);

static void OpenGLClearRenderContext(struct RenderContext *rctx,
                                  struct InternalColor *color);

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out);
static void OpenGLUnlockPixels(struct DrawingBoard *board);
static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y);
static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color);

static void OpenGLBeginBatch(struct RenderContext *rctx);
static void OpenGLEndBatch(struct RenderContext *rctx);
static void OpenGLFlushBatch(struct RenderContext *rctx);
static BOOL OpenGLIsBatching(struct RenderContext *rctx);

static void OpenGLBlitRenderContexts(struct RenderContext *source,
                                  struct RenderContext *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height);
static void OpenGLBlitToScreen(struct RenderContext *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height);

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board);
void OpenGLCleanupDrawingBoard(struct DrawingBoard *board);

/* Helper functions */
static BOOL OpenGL_EnsureGlobalContext(struct Window *window);
static BOOL OpenGL_SwitchToWindow(struct RenderContext *rctx);
static BOOL OpenGL_SwitchToDrawingBoard(struct RenderContext *rctx);
static BOOL OpenGL_SwitchToTarget(struct RenderContext *rctx);
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
static BOOL OpenGL_CreateMasterContext(struct Window *window);
static OpenGLWindowContext *OpenGL_CreateWindowContext(struct Window *window);
static void OpenGL_DestroyWindowContext(OpenGLWindowContext *ctx);
static OpenGLWindowContext *OpenGL_FindWindowContext(struct Window *window);
static BOOL OpenGL_MakeContextCurrent(OpenGLWindowContext *ctx);

/* Zero-copy FBO compositing functions */
static GLuint OpenGL_GetFBOAsTexture(struct DrawingBoard *board);
static void OpenGL_BlitFBOToFBO(struct DrawingBoard *src, struct DrawingBoard *dst,
                                WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                                UWORD width, UWORD height);
static BOOL OpenGL_SyncFBOToBitmap(struct RenderContext *rctx);
static BOOL OpenGL_SyncRegionFBOToBitmap(struct RenderContext *rctx,
                                         WORD x, WORD y, UWORD width, UWORD height);

/* Pixel buffer helper functions */
static void OpenGL_FlipPixelBufferY(UBYTE *buffer, UWORD width, UWORD height, UBYTE *temp);
static void OpenGL_FlipPixelBufferYCopy(const UBYTE *src, UBYTE *dst, UWORD width, UWORD height);
static UBYTE *OpenGL_ReadPixelsToBuffer(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_y);
static UBYTE *OpenGL_ReadRastPortToBuffer(struct RastPort *rp, WORD x, WORD y,
                                          UWORD width, UWORD height, BOOL force_opaque);
static GLuint OpenGL_UploadTextureFromBuffer(const UBYTE *buffer, UWORD width, UWORD height);
static void OpenGL_DrawTexturedQuad(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_texcoord);

/* Brush to texture conversion */
static GLuint OpenGL_BrushToTexture(struct RenderContext *rctx, struct ZuneBrush *brush,
                                    WORD x, WORD y, UWORD width, UWORD height);

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
    "    /* Antialiasing: smooth transition over ~1.0 pixels for sharper edges */\n"
    "    float aa = 1.0;\n"
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

/*
 * Fragment Shader for Textured Rounded Rectangle using SDF
 *
 * Same as the solid color shader, but samples fill color from a texture
 * instead of using a uniform color. Used for gradient, pattern, and
 * texture brush fills with rounded corners.
 */
static const GLchar *g_rounded_rect_textured_fs_source =
    "varying vec2 v_texcoord;\n"
    "uniform vec2 u_size;\n"           /* Rectangle size in pixels */
    "uniform float u_radius;\n"        /* Corner radius in pixels */
    "uniform sampler2D u_fill_texture;\n" /* Fill texture */
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
    "    /* Antialiasing: smooth transition over ~0.75 pixels for sharper edges */\n"
    "    float aa = 0.75;\n"
    "    \n"
    "    /* Start with transparent */\n"
    "    vec4 color = vec4(0.0);\n"
    "    \n"
    "    /* Fill: inside the shape (dist < 0) - sample from texture */\n"
    "    if (u_has_fill > 0.5) {\n"
    "        vec4 fillColor = texture2D(u_fill_texture, v_texcoord);\n"
    "        float fillAlpha = 1.0 - smoothstep(-aa, 0.0, dist);\n"
    "        color = fillColor * fillAlpha;\n"
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
static void OpenGLCopyFromRastPort(struct RenderContext *rctx, struct RastPort *src_rp,
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
    
    /* Debug: sample some pixels from the RastPort data */
    D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: RastPort pixel[0] RGBA = %02x %02x %02x %02x\n",
          pixelbuffer[0], pixelbuffer[1], pixelbuffer[2], pixelbuffer[3]));
    /* Sample a pixel that should be in the yellow rect area (around y=150, x=50) */
    {
        ULONG offset = (150 * width + 50) * 4;
        if (offset + 3 < (ULONG)width * height * 4) {
            D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: RastPort pixel at (50,150) RGBA = %02x %02x %02x %02x\n",
                  pixelbuffer[offset], pixelbuffer[offset+1], pixelbuffer[offset+2], pixelbuffer[offset+3]));
        }
    }

    /*
     * Upload pixel buffer directly to texture WITHOUT flipping.
     * 
     * The projection matrix uses glOrtho(0, width, height, 0, -1, 1) which
     * already flips Y to match screen coordinates. If we also flip the
     * pixel data, we get double-flipping which puts content at wrong Y.
     */
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
    
    D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: GL state: TEXTURE_2D=%d, bound tex=%u\n",
          glIsEnabled(GL_TEXTURE_2D), texture));

    /*
     * Ensure correct viewport and projection for the target.
     * This is critical when drawing the full-size texture to the FBO,
     * as the projection matrix must match the FBO dimensions.
     */
    if (rctx->target_board) {
        struct DrawingBoard *board = rctx->target_board;
        OpenGLFBOData *fbo = (OpenGLFBOData *)board->backend_data;
        
        D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: board=%dx%d, texture=%dx%d, dst=%d,%d\n",
              board->width, board->height, width, height, dst_x, dst_y));
        
        /*
         * Explicitly re-bind the FBO before drawing.
         * glAMakeCurrent may have reset the framebuffer binding.
         */
        if (fbo && fbo->valid && glBindFramebuffer_ptr) {
            D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: Re-binding FBO %u before draw\n", fbo->fbo_id));
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
    D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: Drawing textured quad at %d,%d size %dx%d\n",
          dst_x, dst_y, width, height));
    OpenGL_DrawTexturedQuad(dst_x, dst_y, width, height, FALSE);
    
    /* Ensure the draw is flushed to the FBO */
    glFlush();
    glFinish();
    
    /* Debug: verify FBO content immediately after draw */
    {
        GLint current_fbo = 0;
        UBYTE test_pixel[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
        D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: After draw, current FBO binding = %d\n", current_fbo));
        
        /* Read back a pixel that should be yellow (150, 50 in screen coords) */
        /* Note: OpenGL Y is flipped, so we need to flip the Y coordinate */
        if (rctx->target_board) {
            WORD read_y = rctx->target_board->height - 150 - 1;
            glReadPixels(50, read_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, test_pixel);
            D(bug("[ZuneGfx:OpenGL] CopyFromRastPort: FBO readback at (50,%d) RGBA = %02x %02x %02x %02x\n",
                  read_y, test_pixel[0], test_pixel[1], test_pixel[2], test_pixel[3]));
        }
    }

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glDeleteTextures(1, &texture);
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
    .GetPixelFormat = OpenGLGetPixelFormat,

    .InitRenderContext = OpenGLInitRenderContext,
    .CleanupRenderContext = OpenGLCleanupRenderContext,

    .PrepareColor = OpenGLPrepareColor,
    .ReleaseColor = OpenGLReleaseColor,

    .DrawPixel = OpenGLDrawPixel,
    .DrawLine = OpenGLDrawLine,
    .DrawRectangle = OpenGLDrawRectangle,
    .DrawCircle = OpenGLDrawCircle,

    .ClearRenderContext = OpenGLClearRenderContext,

    .LockPixels = OpenGLLockPixels,
    .UnlockPixels = OpenGLUnlockPixels,
    .GetPixel = OpenGLGetPixel,
    .SetPixel = OpenGLSetPixel,

    .BeginBatch = OpenGLBeginBatch,
    .EndBatch = OpenGLEndBatch,
    .FlushBatch = OpenGLFlushBatch,
    .IsBatching = OpenGLIsBatching,

    .BlitRenderContexts = OpenGLBlitRenderContexts,
    .BlitToScreen = OpenGLBlitToScreen,

    .InitDrawingBoard = OpenGLInitDrawingBoard,
    .CleanupDrawingBoard = OpenGLCleanupDrawingBoard,
    .CopyFromDrawingBoard = OpenGL_SyncFBOToBitmap,
    .CopyRegionFromDrawingBoard = OpenGL_SyncRegionFBOToBitmap,

    .CopyFromRastPort = OpenGLCopyFromRastPort,
};

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
 * This function flips an RGBA pixel buffer vertically.
 *
 * Parameters:
 *   buffer - RGBA pixel buffer to flip (modified in place)
 *   width  - Width in pixels
 *   height - Height in pixels
 *   temp   - Temporary row buffer (must be at least width*4 bytes)
 */
static void OpenGL_FlipPixelBufferY(UBYTE *buffer, UWORD width, UWORD height, UBYTE *temp)
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
 *
 * Parameters:
 *   src    - Source RGBA pixel buffer
 *   dst    - Destination RGBA pixel buffer
 *   width  - Width in pixels
 *   height - Height in pixels
 */
static void OpenGL_FlipPixelBufferYCopy(const UBYTE *src, UBYTE *dst, UWORD width, UWORD height)
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
 * Parameters:
 *   x, y          - Source position in framebuffer
 *   width, height - Size of region to read
 *   flip_y        - If TRUE, flip the buffer vertically
 *
 * Returns allocated buffer (caller must FreeVec), or NULL on failure.
 */
static UBYTE *OpenGL_ReadPixelsToBuffer(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_y)
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
 * Parameters:
 *   rctx            - Source RastPort
 *   x, y          - Source position
 *   width, height - Size of region to read
 *   force_opaque  - If TRUE, set alpha to 0xFF for all pixels
 *
 * Returns allocated buffer (caller must FreeVec), or NULL on failure.
 */
static UBYTE *OpenGL_ReadRastPortToBuffer(struct RastPort *rp, WORD x, WORD y,
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
 * Creates a GL texture and uploads pixel data to it.
 *
 * Parameters:
 *   buffer        - RGBA pixel buffer
 *   width, height - Texture dimensions
 *
 * Returns texture ID, or 0 on failure.
 */
static GLuint OpenGL_UploadTextureFromBuffer(const UBYTE *buffer, UWORD width, UWORD height)
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
 *
 * Parameters:
 *   x, y          - Destination position
 *   width, height - Quad size
 *   flip_texcoord - If TRUE, flip texture V coordinates
 */
static void OpenGL_DrawTexturedQuad(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_texcoord)
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
 * Parameters:
 *   rctx           - RenderContext (for colormap/pen resolution)
 *   brush        - The brush to convert
 *   x, y         - Rectangle position (for gradient calculations)
 *   width,height - Rectangle dimensions
 *
 * Returns texture ID, or 0 on failure. Caller must delete the texture.
 */
static GLuint OpenGL_BrushToTexture(struct RenderContext *rctx, struct ZuneBrush *brush,
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
        ULONG fg_color, bg_color;
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

/*****************************************************************************/
/* Library Management                                                        */
/*****************************************************************************/

/*
 * OpenGL_CheckLibrary - Check if GL library is available
 *
 * The gl.library is opened centrally in DetectLibraries() (zunegfx_core.c).
 * This function just checks if it's available and stores a reference.
 *
 * Returns TRUE if library is available.
 */
BOOL OpenGL_CheckLibrary(OpenGLPrivateData *priv)
{
    if (!priv) {
        return FALSE;
    }

    /* GLBase is opened in DetectLibraries() */
    if (GLBase) {
        priv->GLBase = GLBase;
        priv->gl_available = TRUE;
        return TRUE;
    }

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
    priv->max_texture_size = 1024;
    priv->has_npot_textures = FALSE;
    priv->has_framebuffers = FALSE;
    priv->has_shaders = FALSE;

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
    glUniform1i_ptr = (PFNGLUNIFORM1IPROC)glAGetProcAddress("glUniform1i");
    glUniform2f_ptr = (PFNGLUNIFORM2FPROC)glAGetProcAddress("glUniform2f");
    glUniform4f_ptr = (PFNGLUNIFORM4FPROC)glAGetProcAddress("glUniform4f");

    /* Check if all required functions were loaded */
    if (!glCreateShader_ptr || !glShaderSource_ptr || !glCompileShader_ptr ||
        !glCreateProgram_ptr || !glAttachShader_ptr || !glLinkProgram_ptr ||
        !glUseProgram_ptr || !glGetUniformLocation_ptr ||
        !glUniform1f_ptr || !glUniform2f_ptr || !glUniform4f_ptr) {
        return FALSE;
    }

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
        return FALSE;
    }

    g_vbo_available = TRUE;
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
        return FALSE;
    }

    glBindBuffer_ptr(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData_ptr(GL_ARRAY_BUFFER, sizeof(g_quad_vertices), g_quad_vertices, GL_STATIC_DRAW);
    glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);

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
    TEXT log[512];

    D(bug("[ZuneGfx:OpenGL] CompileShader: type=%s\n", 
          type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"));

    if (!glCreateShader_ptr || !glShaderSource_ptr || !glCompileShader_ptr) {
        D(bug("[ZuneGfx:OpenGL] CompileShader: missing function pointers\n"));
        return 0;
    }

    shader = glCreateShader_ptr(type);
    if (shader == 0) {
        D(bug("[ZuneGfx:OpenGL] CompileShader: glCreateShader returned 0\n"));
        return 0;
    }
    D(bug("[ZuneGfx:OpenGL] CompileShader: shader id=%u\n", shader));

    glShaderSource_ptr(shader, 1, &source, NULL);
    glCompileShader_ptr(shader);

    /* Check compilation status */
    compiled = GL_FALSE;
    if (glGetShaderiv_ptr) {
        glGetShaderiv_ptr(shader, GL_COMPILE_STATUS, &compiled);
        D(bug("[ZuneGfx:OpenGL] CompileShader: compile status=%d\n", compiled));
        if (!compiled) {
            if (glGetShaderInfoLog_ptr) {
                log[0] = 0;
                glGetShaderInfoLog_ptr(shader, sizeof(log), NULL, (char *)log);
                D(bug("[ZuneGfx:OpenGL] CompileShader: compile error: %s\n", log));
            }
            if (glDeleteShader_ptr) {
                glDeleteShader_ptr(shader);
            }
            return 0;
        }
    } else {
        D(bug("[ZuneGfx:OpenGL] CompileShader: WARNING - cannot check compile status\n"));
    }

    D(bug("[ZuneGfx:OpenGL] CompileShader: success\n"));
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
    TEXT log[512];

    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: starting\n"));

    if (!glCreateProgram_ptr || !glAttachShader_ptr || !glLinkProgram_ptr) {
        D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: missing function pointers\n"));
        return FALSE;
    }

    /* Compile vertex shader */
    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: compiling vertex shader\n"));
    g_rounded_rect_vs = OpenGL_CompileShader(GL_VERTEX_SHADER, g_rounded_rect_vs_source);
    if (g_rounded_rect_vs == 0) {
        D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: vertex shader compilation FAILED\n"));
        return FALSE;
    }
    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: vertex shader id=%u\n", g_rounded_rect_vs));

    /* Compile fragment shader */
    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: compiling fragment shader\n"));
    g_rounded_rect_fs = OpenGL_CompileShader(GL_FRAGMENT_SHADER, g_rounded_rect_fs_source);
    if (g_rounded_rect_fs == 0) {
        D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: fragment shader compilation FAILED\n"));
        if (glDeleteShader_ptr) glDeleteShader_ptr(g_rounded_rect_vs);
        g_rounded_rect_vs = 0;
        return FALSE;
    }
    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: fragment shader id=%u\n", g_rounded_rect_fs));

    /* Create and link program */
    g_rounded_rect_program = glCreateProgram_ptr();
    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: program id=%u\n", g_rounded_rect_program));
    if (g_rounded_rect_program == 0) {
        D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: glCreateProgram FAILED\n"));
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
    D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: linking program\n"));
    glLinkProgram_ptr(g_rounded_rect_program);

    /* Check link status */
    linked = GL_FALSE;
    if (glGetProgramiv_ptr) {
        glGetProgramiv_ptr(g_rounded_rect_program, GL_LINK_STATUS, &linked);
        D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: link status=%d\n", linked));
        if (!linked) {
            if (glGetProgramInfoLog_ptr) {
                log[0] = 0;
                glGetProgramInfoLog_ptr(g_rounded_rect_program, sizeof(log), NULL, (char *)log);
                D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: link error: %s\n", log));
            }
            OpenGL_DestroyShaders();
            return FALSE;
        }
    } else {
        D(bug("[ZuneGfx:OpenGL] CreateRoundedRectShader: WARNING - cannot check link status\n"));
    }

    /* Get uniform locations */
    g_uniform_rect_size = glGetUniformLocation_ptr(g_rounded_rect_program, "u_size");
    g_uniform_rect_radius = glGetUniformLocation_ptr(g_rounded_rect_program, "u_radius");
    g_uniform_fill_color = glGetUniformLocation_ptr(g_rounded_rect_program, "u_fill_color");
    g_uniform_border_color = glGetUniformLocation_ptr(g_rounded_rect_program, "u_border_color");
    g_uniform_border_width = glGetUniformLocation_ptr(g_rounded_rect_program, "u_border_width");
    g_uniform_has_fill = glGetUniformLocation_ptr(g_rounded_rect_program, "u_has_fill");
    g_uniform_has_border = glGetUniformLocation_ptr(g_rounded_rect_program, "u_has_border");

    /*
     * Create textured rounded rectangle shader program
     * This shader samples fill color from a texture instead of using a uniform.
     */

    /* Compile textured fragment shader */
    g_rounded_rect_textured_fs = OpenGL_CompileShader(GL_FRAGMENT_SHADER, g_rounded_rect_textured_fs_source);
    if (g_rounded_rect_textured_fs == 0) {
        return TRUE; /* Continue with solid shader only */
    }

    /* Create and link textured program (reuses same vertex shader) */
    g_rounded_rect_textured_program = glCreateProgram_ptr();
    if (g_rounded_rect_textured_program == 0) {
        glDeleteShader_ptr(g_rounded_rect_textured_fs);
        g_rounded_rect_textured_fs = 0;
        return TRUE;
    }

    glAttachShader_ptr(g_rounded_rect_textured_program, g_rounded_rect_vs);
    glAttachShader_ptr(g_rounded_rect_textured_program, g_rounded_rect_textured_fs);
    glLinkProgram_ptr(g_rounded_rect_textured_program);

    if (glGetProgramiv_ptr) {
        glGetProgramiv_ptr(g_rounded_rect_textured_program, GL_LINK_STATUS, &linked);
        if (!linked) {
            glDeleteProgram_ptr(g_rounded_rect_textured_program);
            g_rounded_rect_textured_program = 0;
            glDeleteShader_ptr(g_rounded_rect_textured_fs);
            g_rounded_rect_textured_fs = 0;
            return TRUE;
        }
    }

    /* Get uniform locations for textured shader */
    g_uniform_tex_rect_size = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_size");
    g_uniform_tex_rect_radius = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_radius");
    g_uniform_tex_fill_texture = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_fill_texture");
    g_uniform_tex_border_color = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_border_color");
    g_uniform_tex_border_width = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_border_width");
    g_uniform_tex_has_fill = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_has_fill");
    g_uniform_tex_has_border = glGetUniformLocation_ptr(g_rounded_rect_textured_program, "u_has_border");

    return TRUE;
}

/*
 * OpenGL_DestroyShaders - Clean up shader resources
 */
static void OpenGL_DestroyShaders(void)
{

    if ((g_rounded_rect_program || g_rounded_rect_textured_program) && glUseProgram_ptr) {
        glUseProgram_ptr(0);
    }

    /* Clean up textured shader */
    if (g_rounded_rect_textured_program && glDetachShader_ptr) {
        if (g_rounded_rect_vs) glDetachShader_ptr(g_rounded_rect_textured_program, g_rounded_rect_vs);
        if (g_rounded_rect_textured_fs) glDetachShader_ptr(g_rounded_rect_textured_program, g_rounded_rect_textured_fs);
    }

    if (g_rounded_rect_textured_fs && glDeleteShader_ptr) {
        glDeleteShader_ptr(g_rounded_rect_textured_fs);
        g_rounded_rect_textured_fs = 0;
    }

    if (g_rounded_rect_textured_program && glDeleteProgram_ptr) {
        glDeleteProgram_ptr(g_rounded_rect_textured_program);
        g_rounded_rect_textured_program = 0;
    }

    /* Clean up solid color shader */
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

    /* Reset solid shader uniforms */
    g_uniform_rect_size = -1;
    g_uniform_rect_radius = -1;
    g_uniform_fill_color = -1;
    g_uniform_border_color = -1;
    g_uniform_border_width = -1;
    g_uniform_has_fill = -1;
    g_uniform_has_border = -1;

    /* Reset textured shader uniforms */
    g_uniform_tex_rect_size = -1;
    g_uniform_tex_rect_radius = -1;
    g_uniform_tex_fill_texture = -1;
    g_uniform_tex_border_color = -1;
    g_uniform_tex_border_width = -1;
    g_uniform_tex_has_fill = -1;
    g_uniform_tex_has_border = -1;
}

/*
 * Minimum stack size required for shader compilation.
 * Mesa/LLVM shader compilation requires significant stack space.
 */
#define ZUNEGFX_SHADER_SAFESTACK    (1 << 18)  /* 256KB */

/*
 * OpenGL_InitShadersInternal - Actually compile and link shaders
 *
 * This does the actual shader compilation work. Called either from
 * OpenGL_InitShaders() if stack is large enough, or from
 * OpenGL_PreInitializeShaders() during library init.
 *
 * REQUIRES: GL context must be current, stack must be >= ZUNEGFX_SHADER_SAFESTACK
 */
static BOOL OpenGL_InitShadersInternal(void)
{
    const GLubyte *version_str;
    LONG major = 0, minor = 0;

    if (g_shaders_available) {
        return TRUE;
    }

    /*
     * Check GL version before attempting to use shaders.
     * GLSL shaders require OpenGL 2.0 or higher.
     */
    version_str = glGetString(GL_VERSION);
    if (version_str) {
        D(bug("[ZuneGfx:OpenGL] InitShadersInternal: GL_VERSION=%s\n", version_str));
        /* Parse version string - format is "major.minor" or "major.minor.release" */
        sscanf((const char *)version_str, "%d.%d", &major, &minor);
    } else {
        D(bug("[ZuneGfx:OpenGL] InitShadersInternal: glGetString(GL_VERSION) failed\n"));
        return FALSE;
    }

    /* Require at least OpenGL 2.0 for GLSL shaders */
    if (major < 2) {
        D(bug("[ZuneGfx:OpenGL] InitShadersInternal: GL %d.%d < 2.0, no shaders\n", major, minor));
        return FALSE;
    }

    /* Load shader function pointers */
    if (!OpenGL_LoadShaderFunctions()) {
        D(bug("[ZuneGfx:OpenGL] InitShadersInternal: LoadShaderFunctions failed\n"));
        return FALSE;
    }

    /* Create the rounded rectangle shader program */
    if (!OpenGL_CreateRoundedRectShader()) {
        D(bug("[ZuneGfx:OpenGL] InitShadersInternal: CreateRoundedRectShader failed\n"));
        return FALSE;
    }

    g_shaders_available = TRUE;
    D(bug("[ZuneGfx:OpenGL] InitShadersInternal: Shaders OK!\n"));
    return TRUE;
}

/*
 * OpenGL_InitShaders - Initialize shaders after context creation
 *
 * Call this after the first GL context is created and made current.
 * Checks stack size and only proceeds if large enough.
 *
 * NOTE: We cannot use NewStackSwap/StackSwap to work around small stacks
 * because Mesa's shader compilation uses posixc.library functions (like fprintf
 * in error paths) which have thread-local state tied to the original stack.
 * Swapping stacks corrupts this state and causes crashes.
 */
static BOOL OpenGL_InitShaders(void)
{
    struct Task *this_task;
    IPTR stack_size;

    if (g_shaders_available) {
        return TRUE;
    }

    /*
     * Check stack size before attempting shader compilation.
     * Mesa shader compilation (especially with LLVM) requires significant stack.
     * If we don't have enough stack, shader compilation will crash.
     *
     * If stack is too small, shaders should have been pre-initialized during
     * library init via OpenGL_PreInitializeShaders(). If not, we can't compile
     * shaders safely and must fall back to non-shader rendering.
     */
    this_task = FindTask(NULL);
    if (this_task) {
        stack_size = (IPTR)this_task->tc_SPUpper - (IPTR)this_task->tc_SPLower;
        D(bug("[ZuneGfx:OpenGL] InitShaders: stack=%ld, required=%ld\n",
              (LONG)stack_size, (LONG)ZUNEGFX_SHADER_SAFESTACK));
        if (stack_size < ZUNEGFX_SHADER_SAFESTACK) {
            D(bug("[ZuneGfx:OpenGL] InitShaders: Stack too small, shaders unavailable\n"));
            return FALSE;
        }
    }

    /* Stack is large enough, compile shaders */
    return OpenGL_InitShadersInternal();
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
static OpenGLFBOData *OpenGL_CreateFBO(UWORD width, UWORD height)
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
static void OpenGL_DestroyFBO(OpenGLFBOData *fbo)
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
static BOOL OpenGL_BindFBO(OpenGLFBOData *fbo)
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
static void OpenGL_UnbindFBO(void)
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
static GLuint OpenGL_GetFBOAsTexture(struct DrawingBoard *board)
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
static void OpenGL_BlitFBOToFBO(struct DrawingBoard *src, struct DrawingBoard *dst,
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

/*****************************************************************************/
/* Window Context Functions                                                  */
/*****************************************************************************/

/*
 * OpenGL_CreateMasterContext - Create the master GL context for resource sharing
 *
 * The master context is created once and all other window contexts share
 * resources (textures, buffers, shaders) with it via GLA_ShareContext.
 * This enables zero-copy compositing between DrawingBoards.
 *
 * Returns TRUE if master context was created successfully.
 */
static BOOL OpenGL_CreateMasterContext(struct Window *window)
{
    struct TagItem tags[10];
    WORD tag_idx = 0;
    GLAContext master_ctx;
    APTR master_pipe_screen;

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

    /* Set up tags for master context creation */
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

    /* Create master GL context */
    master_ctx = glACreateContext(tags);
    if (!master_ctx) {
        D(bug("[ZuneRenderer:OpenGL] OpenGL_CreateMasterContext: glACreateContext FAILED\n"));
        return FALSE;
    }

    /* Store master context */
    g_opengl_priv->master_context = (APTR)master_ctx;
    g_opengl_priv->master_context_created = TRUE;

    /* Make it current to initialize GL state */
    glAMakeCurrent(master_ctx);

    /* Get pipe_screen to verify sharing will work */
    master_pipe_screen = glAGetPipeScreen(master_ctx);
    g_opengl_priv->shared_contexts_supported = (master_pipe_screen != NULL);

    D(bug("[ZuneGfx:OpenGL] CreateMasterContext: master=%p, pipe_screen=%p, shared_supported=%d\n",
          master_ctx, master_pipe_screen, g_opengl_priv->shared_contexts_supported));

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

/* Pre-init window/screen for shader compilation (fallback path) */
static struct Screen *g_preinit_screen = NULL;
static struct Window *g_preinit_window = NULL;
static BOOL g_using_compositor_context = FALSE;

/* GLCompositor semaphore structure — must match glcompositor_intern.h */
struct GLCompositorSemaphore
{
    struct SignalSemaphore   sem;
    APTR                    master_context;
};
#define GLCOMPOSITOR_SEMAPHORE_NAME "GLCompositorMasterContext"

/*
 * TryHeadlessContext - Create a headless GL context without a window
 *
 * Uses GLA_Headless to create a GL context without needing a window.
 * CreatePipeV handles the Gallium driver lookup via LockPubScreen fallback,
 * so no friendBM is needed.
 *
 * If the compositor has already published its master context, we share
 * with it via GLA_ShareContext. Otherwise we create a standalone context.
 *
 * Returns the new context, or NULL on failure.
 */
static GLAContext TryHeadlessContext(void)
{
    struct GLCompositorSemaphore *comp_sem;
    GLAContext ctx;
    APTR compositor_ctx = NULL;

    /* Check if compositor's master context is available for sharing */
    Forbid();
    comp_sem = (struct GLCompositorSemaphore *)FindSemaphore(GLCOMPOSITOR_SEMAPHORE_NAME);
    Permit();

    if (comp_sem)
    {
        ObtainSemaphoreShared(&comp_sem->sem);
        compositor_ctx = comp_sem->master_context;
        ReleaseSemaphore(&comp_sem->sem);
    }

    if (compositor_ctx)
    {
        struct TagItem ctx_tags[] = {
            { GLA_Headless,      GL_TRUE },
            { GLA_BitsPerPixel,  32 },
            { GLA_Width,         1 },
            { GLA_Height,        1 },
            { GLA_NoDepth,       GL_TRUE },
            { GLA_NoStencil,     GL_TRUE },
            { GLA_NoAccum,       GL_TRUE },
            { GLA_ShareContext,  (IPTR)compositor_ctx },
            { TAG_DONE,          0 }
        };

        D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: Sharing with compositor master context @ %p\n", compositor_ctx));
        ctx = glACreateContext(ctx_tags);
    }
    else
    {
        struct TagItem ctx_tags[] = {
            { GLA_Headless,      GL_TRUE },
            { GLA_BitsPerPixel,  32 },
            { GLA_Width,         1 },
            { GLA_Height,        1 },
            { GLA_NoDepth,       GL_TRUE },
            { GLA_NoStencil,     GL_TRUE },
            { GLA_NoAccum,       GL_TRUE },
            { TAG_DONE,          0 }
        };

        D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: Creating standalone headless context (no compositor yet)\n"));
        ctx = glACreateContext(ctx_tags);
    }

    if (!ctx)
    {
        D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: glACreateContext failed\n"));
        return NULL;
    }

    D(bug("[ZuneGfx:OpenGL] TryHeadlessContext: Created headless context @ %p (shared=%d)\n",
          ctx, compositor_ctx ? 1 : 0));
    return ctx;
}

/*
 * OpenGL_PreInitializeShaders - Initialize shaders during library init
 *
 * First tries to discover GLCompositor's master GL context via semaphore
 * and create a shared headless context (no window needed).
 *
 * Falls back to opening a small backdrop window if GLCompositor is not
 * available (e.g. running without compositor, or compositor not yet
 * initialized).
 *
 * The context is kept alive for the library lifetime since GL shader
 * objects are bound to the context that created them.
 */
BOOL OpenGL_PreInitializeShaders(void)
{
    GLAContext preinit_ctx;
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

    /* Strategy 1: Use a headless GL context (no window needed) */
    preinit_ctx = TryHeadlessContext();
    if (preinit_ctx)
    {
        g_using_compositor_context = TRUE;
        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Using headless context (no window needed)\n"));
    }
    else
    {
        /* Strategy 2: Fall back to creating a pre-init window */
        struct TagItem ctx_tags[4];

        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Compositor not available, creating pre-init window\n"));

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
        APTR pipe_screen;

        g_opengl_priv->master_context = preinit_ctx;
        g_opengl_priv->master_context_created = TRUE;
        g_opengl_priv->has_shaders = g_shaders_available;

        /* Check if context sharing is supported by getting pipe_screen */
        pipe_screen = glAGetPipeScreen(preinit_ctx);
        g_opengl_priv->shared_contexts_supported = (pipe_screen != NULL);

        D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Stored as master context, pipe_screen=%p, sharing=%d\n",
              pipe_screen, g_opengl_priv->shared_contexts_supported));
    }

    /* Unbind our context so we don't interfere with other GL users
     * (e.g. GLCompositor which may already have a context current) */
    glAMakeCurrent(NULL);

    D(bug("[ZuneGfx:OpenGL] PreInitializeShaders: Done (compositor_shared=%d)\n",
          g_using_compositor_context));

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
    D(bug("[ZuneGfx:OpenGL] CleanupPreInit: Cleaning up pre-init resources (compositor_shared=%d)\n",
          g_using_compositor_context));

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

    g_using_compositor_context = FALSE;
}

/*
 * OpenGL_GetMasterContext - Get the master GL context for sharing
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
 * OpenGL_CreateWindowContext - Create a GL context for a window
 *
 * If a master context exists, the new context will share resources with it
 * via GLA_ShareContext. This enables efficient switching between windows
 * using glAMakeCurrent() instead of the crash-prone glASetRast().
 */
static OpenGLWindowContext *OpenGL_CreateWindowContext(struct Window *window)
{
    OpenGLWindowContext *ctx;
    struct TagItem tags[12];  /* Extra space for GLA_ShareContext */
    WORD tag_idx = 0;
    BOOL use_shared_context = FALSE;

    if (!window || !GLBase) {
        return NULL;
    }

    /* Allocate context structure */
    ctx = AllocVec(sizeof(OpenGLWindowContext), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ctx) {
        return NULL;
    }

    /*
     * Check if we should use shared context mode.
     * If master context exists and sharing is supported, create this context
     * with GLA_ShareContext to enable resource sharing.
     */
    if (g_opengl_priv && g_opengl_priv->master_context_created &&
        g_opengl_priv->master_context && g_opengl_priv->shared_contexts_supported) {
        use_shared_context = TRUE;
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

    /* Add GLA_ShareContext if master context is available */
    if (use_shared_context) {
        tags[tag_idx].ti_Tag = GLA_ShareContext;
        tags[tag_idx].ti_Data = (IPTR)g_opengl_priv->master_context;
        tag_idx++;
    }

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create GL context */
    ctx->gl_context = glACreateContext(tags);
    if (!ctx->gl_context) {
        /* If shared context failed, try again without sharing */
        if (use_shared_context) {
            use_shared_context = FALSE;

            /* Rebuild tags without GLA_ShareContext */
            tag_idx = 0;
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

            ctx->gl_context = glACreateContext(tags);
            if (!ctx->gl_context) {
                FreeVec(ctx);
                return NULL;
            }
        } else {
            FreeVec(ctx);
            return NULL;
        }
    }

    /* Verify sharing worked by comparing pipe_screen */
    if (use_shared_context) {
        APTR ctx_pipe_screen = glAGetPipeScreen((GLAContext)ctx->gl_context);
        APTR master_pipe_screen = glAGetPipeScreen((GLAContext)g_opengl_priv->master_context);

        ctx->uses_shared_context = (ctx_pipe_screen && master_pipe_screen &&
                                    ctx_pipe_screen == master_pipe_screen);
    } else {
        ctx->uses_shared_context = FALSE;
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

    return ctx;
}

/*
 * OpenGL_DestroyWindowContext - Destroy a window's GL context
 *
 * When using shared contexts, this handles the reference counting properly.
 * The master context is only destroyed when all window contexts are gone.
 */
static void OpenGL_DestroyWindowContext(OpenGLWindowContext *ctx)
{
    OpenGLWindowContext **prev;
    BOOL was_shared;

    if (!ctx) return;

    was_shared = ctx->uses_shared_context;

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

    /*
     * Check if we should destroy the master context.
     * Only destroy it when there are no more window contexts using it.
     */
    if (was_shared && g_opengl_priv && g_opengl_priv->master_context_created) {
        /* Check if any remaining contexts are using shared resources */
        BOOL has_shared_contexts = FALSE;
        OpenGLWindowContext *remaining = g_opengl_priv->window_contexts;

        while (remaining) {
            if (remaining->uses_shared_context) {
                has_shared_contexts = TRUE;
                break;
            }
            remaining = remaining->next;
        }

        if (!has_shared_contexts) {
            if (g_opengl_priv->master_context) {
                glADestroyContext((GLAContext)g_opengl_priv->master_context);
                g_opengl_priv->master_context = NULL;
            }
            g_opengl_priv->master_context_created = FALSE;
            g_opengl_priv->shared_contexts_supported = FALSE;
        }
    }
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
    WORD tag_idx = 0;

    if (!g_opengl_priv || !g_opengl_priv->gl_context || !dst_rp) {
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

    /* Change the GL context's visible_rp to the destination RastPort */
    glASetRast((GLAContext)g_opengl_priv->gl_context, setrast_tags);

    /* Flush and swap - this will use BltPipeResourceRastPort internally */
    glFlush();
    glASwapBuffers((GLAContext)g_opengl_priv->gl_context);

    g_opengl_priv->setrast_calls++;

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
     *
     * FBOs are NOT shared between GL contexts in Mesa - each context has its
     * own FBO namespace. The FBO content was rendered in the global context
     * (g_opengl_priv->gl_context), so we MUST use that same context to read it.
     *
     * First, flush any pending operations to ensure FBO content is complete.
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

    /* Verify the write by reading back a sample pixel */
    if (CyberGfxBase && dst_rp->BitMap) {
        UBYTE verify[4];
        ReadPixelArray(verify, 0, 0, 4, dst_rp, dst_x, dst_y, 1, 1, RECTFMT_RGBA);
        D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: verify readback RGBA = %02x %02x %02x %02x\n",
              verify[0], verify[1], verify[2], verify[3]));
    }

    FreeVec(pixelbuffer);
    
    D(bug("[ZuneGfx:OpenGL] BlitFBOToRastPort: done\n"));
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
BOOL OpenGL_SyncFBOToBitmap(struct RenderContext *rctx)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    /* Validate RenderContext */
    if (!rctx) {
        return FALSE;
    }

    board = rctx->target_board;

    /* Validate DrawingBoard */
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

    /* Validate FBO */
    if (!fbo->valid) {
        return FALSE;
    }

    /*
     * Only sync if the FBO has been modified since last sync.
     * This avoids expensive glReadPixels + WritePixelArray operations
     * when the FBO content hasn't changed (e.g., during window resize
     * when blitting unchanged content).
     */
    if (!fbo->dirty) {
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

/*
 * OpenGL_SyncRegionFBOToBitmap - Sync a region of FBO contents to DrawingBoard's bitmap
 *
 * This is more efficient than OpenGL_SyncFBOToBitmap when only a portion
 * of the FBO needs to be synced (e.g., when flushing a specific dirty region).
 *
 * Parameters:
 *   rctx - RenderContext with target DrawingBoard
 *   x, y - Top-left corner of region to sync
 *   width, height - Size of region to sync
 */
static BOOL OpenGL_SyncRegionFBOToBitmap(struct RenderContext *rctx,
                                         WORD x, WORD y, UWORD width, UWORD height)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: rctx=%p, region=%d,%d %dx%d\n",
          rctx, x, y, width, height));

    /* Validate RenderContext */
    if (!rctx) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: rctx is NULL\n"));
        return FALSE;
    }

    board = rctx->target_board;

    /* Validate DrawingBoard */
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

    /* Validate FBO */
    if (!fbo->valid) {
        D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: FBO not valid\n"));
        return FALSE;
    }

    D(bug("[ZuneGfx:OpenGL] SyncRegionFBOToBitmap: fbo=%p, fbo_id=%u, dirty=%d\n",
          fbo, fbo->fbo_id, fbo->dirty));

    /*
     * Only sync if the FBO has been modified since last sync.
     */
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

    /* Sync only the specified region */
    OpenGL_BlitFBOToRastPort(board, board->rastport,
                              x, y, x, y,
                              width, height);

    /*
     * Note: We do NOT clear the dirty flag here because we only synced
     * a region. The FBO may still have other dirty regions. The dirty
     * flag is only cleared when the entire FBO is synced.
     *
     * For a more sophisticated implementation, we could track dirty
     * regions and only clear when all are synced.
     */

    return TRUE;
}

/*****************************************************************************/
/* Backend Implementation - Lifecycle                                        */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx)
{
    OpenGLPrivateData *priv;

    if (!ctx) {
        return FALSE;
    }

    /* Allocate private data */
    priv = AllocVec(sizeof(OpenGLPrivateData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!priv) {
        return FALSE;
    }

    /* Check if GL library is available (opened in DetectLibraries) */
    if (!OpenGL_CheckLibrary(priv)) {
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

    return TRUE;
}

static void OpenGLCleanupBackend(ZuneBackendContext *ctx)
{

    if (!ctx || !ctx->private_data) {
        return;
    }

    OpenGLPrivateData *priv = (OpenGLPrivateData *)ctx->private_data;

    /* Destroy VBO before destroying the GL context */
    OpenGL_DestroyQuadVBO();

    /* Destroy shaders before destroying the GL context */
    OpenGL_DestroyShaders();

    /* Destroy all window contexts first */
    while (priv->window_contexts) {
        OpenGLWindowContext *ctx_to_destroy = priv->window_contexts;
        priv->window_contexts = ctx_to_destroy->next;

        if (ctx_to_destroy->gl_context) {
            glADestroyContext((GLAContext)ctx_to_destroy->gl_context);
        }
        FreeVec(ctx_to_destroy);
    }
    priv->current_context = NULL;

    /* Destroy the master context if it exists */
    if (priv->master_context) {
        glADestroyContext((GLAContext)priv->master_context);
        priv->master_context = NULL;
        priv->master_context_created = FALSE;
        priv->shared_contexts_supported = FALSE;
    }

    /* Destroy the global GL context if it exists (fallback mode) */
    if (priv->gl_context) {
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
}

/*
 * Cached availability check - result doesn't change after library init
 */
static BOOL g_opengl_available_cached = FALSE;
static BOOL g_opengl_available_checked = FALSE;

static BOOL OpenGLIsAvailable(void)
{
    /* 
     * Always re-check if GLBase became available.
     * This handles the case where GL library is opened after
     * initial backend registration.
     */
    if (GLBase != NULL) {
        g_opengl_available_cached = TRUE;
        g_opengl_available_checked = TRUE;
        return TRUE;
    }
    
    /* Return cached FALSE result if already checked and still not available */
    if (g_opengl_available_checked) {
        return g_opengl_available_cached;
    }

    /*
     * Check if GLBase was opened in DetectLibraries().
     */
    g_opengl_available_cached = (GLBase != NULL);
    g_opengl_available_checked = TRUE;
    return g_opengl_available_cached;
}

static BOOL OpenGLIsCompatible(struct RenderContext *rctx)
{
    if (!rctx) {
        /* NULL rctx means checking general compatibility */
        return OpenGLIsAvailable();
    }

    D(bug("[ZuneGfx:OpenGL] IsCompatible: rctx=%p window=%p target_board=%p\n",
          rctx, rctx->window, rctx->target_board));
    if (rctx->target_board) {
        D(bug("[ZuneGfx:OpenGL] IsCompatible: board->parent_window=%p\n",
              rctx->target_board->parent_window));
    }

    /*
     * NEW ARCHITECTURE: OpenGL compatibility is based on having a Window.
     *
     * OpenGL requires a Window to create a GL context. The RenderContext should
     * have rctx->window set (via ZuneCreateRenderContextForWindow) for OpenGL to work.
     *
     * With the new architecture:
     * - RenderContext is bound to a Window (required for GL context)
     * - DrawingBoards always have BitMap (for legacy compatibility)
     * - OpenGL adds FBO to DrawingBoard for accelerated rendering
     * - Switching targets uses glBindFramebuffer() (fast)
     */

    /* Check if RenderContext has a window - required for GL context */
    if (rctx->window) {
        return TRUE;
    }

    /* Legacy path: Check for DrawingBoard with parent_window */
    if (rctx->target_board && rctx->target_board->parent_window) {
        return TRUE;
    }

    /* No Window means we can't create a GL context - fall back to CyberGraphics */
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

static ULONG OpenGLGetPixelFormat(struct BitMap *bitmap)
{
    /* OpenGL backend uses CyberGfx bitmaps as backing store */
    if (CyberGfxBase && bitmap) {
        return GetCyberMapAttr(bitmap, CYBRMATTR_PIXFMT);
    }
    return 0;
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
    struct TagItem tags[12];  /* Extra space for GLA_ShareContext */
    WORD tag_idx = 0;
    GLAContext gl_ctx;
    BOOL use_shared_context = FALSE;

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

    /*
     * NEW: Create master context first if it doesn't exist.
     * This enables shared context mode for all subsequent contexts.
     * The master context is used for resource sharing (textures, FBOs, etc.)
     */
    if (!g_opengl_priv->master_context_created) {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Creating master context first\n"));
        if (OpenGL_CreateMasterContext(window)) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Master context created, shared_contexts_supported=%d\n",
                  g_opengl_priv->shared_contexts_supported));
            use_shared_context = g_opengl_priv->shared_contexts_supported;
        } else {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Master context creation failed, continuing without sharing\n"));
        }
    } else if (g_opengl_priv->shared_contexts_supported && g_opengl_priv->master_context) {
        /* Master context exists and supports sharing */
        use_shared_context = TRUE;
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
    tags[tag_idx].ti_Data = GL_TRUE;
    tag_idx++;

    tags[tag_idx].ti_Tag = GLA_NoStencil;
    tags[tag_idx].ti_Data = GL_TRUE;
    tag_idx++;

    /* Add GLA_ShareContext if master context is available */
    if (use_shared_context) {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Using GLA_ShareContext with master %p\n",
              g_opengl_priv->master_context));
        tags[tag_idx].ti_Tag = GLA_ShareContext;
        tags[tag_idx].ti_Data = (IPTR)g_opengl_priv->master_context;
        tag_idx++;
    }

    tags[tag_idx].ti_Tag = TAG_DONE;
    tags[tag_idx].ti_Data = 0;

    /* Create the global context (with sharing if master context exists) */
    gl_ctx = glACreateContext(tags);
    if (!gl_ctx) {
        D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: glACreateContext FAILED\n"));
        
        /* If shared context failed, try again without sharing */
        if (use_shared_context) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Retrying without sharing\n"));
            use_shared_context = FALSE;
            
            /* Rebuild tags without GLA_ShareContext */
            tag_idx = 0;
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
            
            gl_ctx = glACreateContext(tags);
            if (!gl_ctx) {
                D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: glACreateContext FAILED (without sharing)\n"));
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    /* Verify sharing worked by comparing pipe_screens */
    if (use_shared_context) {
        APTR global_pipe_screen = glAGetPipeScreen(gl_ctx);
        APTR master_pipe_screen = glAGetPipeScreen((GLAContext)g_opengl_priv->master_context);
        
        if (global_pipe_screen && master_pipe_screen && global_pipe_screen == master_pipe_screen) {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Sharing verified - same pipe_screen %p\n",
                  global_pipe_screen));
        } else {
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: WARNING - pipe_screens differ (global=%p, master=%p)\n",
                  global_pipe_screen, master_pipe_screen));
            /* Sharing didn't work as expected, but context was created successfully */
        }
    }

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
    
    D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: Global context created %p, shared=%d\n",
          gl_ctx, use_shared_context));

    /* Set up initial orthographic projection */
    OpenGL_SetupOrthoProjection(g_opengl_priv->current_width, g_opengl_priv->current_height);

    /* Initialize shaders now that we have a context */
    if (OpenGL_InitShaders()) {
        if (g_opengl_priv) {
            g_opengl_priv->has_shaders = TRUE;
        }
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

        if (fbo_works && g_opengl_priv) {
            g_opengl_priv->has_framebuffers = TRUE;
            D(bug("[ZuneGfx:OpenGL] EnsureGlobalContext: FBO test PASSED\n"));
        } else {
            g_fbo_available = FALSE;
            if (g_opengl_priv) {
                g_opengl_priv->has_framebuffers = FALSE;
            }
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
 * OpenGL_SwitchToWindow - Switch the GL context to render to a different window
 *
 * NEW ARCHITECTURE (with shared contexts):
 * - Each window has its own GL context that shares resources with the master context
 * - Switching uses glAMakeCurrent() which is safe and efficient
 * - No glASetRast() needed - avoids crashes with different dimensions
 *
 * FALLBACK (without shared contexts):
 * - Uses the old glASetRast() method with the single global context
 *
 * Returns TRUE if switch was successful.
 */
static BOOL OpenGL_SwitchToWindow(struct RenderContext *rctx)
{
    struct Window *window = NULL;
    struct RastPort *rastport;
    OpenGLWindowContext *win_ctx;
    UWORD width, height;

    if (!rctx || !g_opengl_priv) {
        return FALSE;
    }

    /* Get the target window */
    if (rctx->target_rastport) {
        rastport = rctx->target_rastport;
        if (rastport->Layer && rastport->Layer->Window) {
            window = (struct Window *)rastport->Layer->Window;
        }
    }

    if (!window) {
        return FALSE;
    }

    /* Calculate window dimensions */
    width = window->Width - window->BorderLeft - window->BorderRight;
    height = window->Height - window->BorderTop - window->BorderBottom;

    /*
     * NEW: Try shared context approach first.
     * Each window gets its own GL context that shares resources with master.
     * This allows safe switching via glAMakeCurrent().
     *
     * We try this path if:
     * 1. Shared contexts are already known to be supported, OR
     * 2. Master context hasn't been created yet (we'll create it and check)
     */
    if (g_opengl_priv->shared_contexts_supported || !g_opengl_priv->master_context_created) {
        /* Ensure master context exists */
        if (!g_opengl_priv->master_context_created) {
            D(bug("[ZuneGfx:OpenGL] SwitchToWindow: Creating master context for shared context support\n"));
            if (!OpenGL_CreateMasterContext(window)) {
                D(bug("[ZuneGfx:OpenGL] SwitchToWindow: Master context creation failed\n"));
                goto fallback_setrast;
            }
        }

        /* Check if sharing is actually supported after master context creation */
        if (!g_opengl_priv->shared_contexts_supported) {
            D(bug("[ZuneGfx:OpenGL] SwitchToWindow: Shared contexts not supported, using fallback\n"));
            goto fallback_setrast;
        }

        /* Find or create window context */
        win_ctx = OpenGL_FindWindowContext(window);
        if (!win_ctx) {
            win_ctx = OpenGL_CreateWindowContext(window);
            if (!win_ctx) {
                goto fallback_setrast;
            }
        }

        /* Check if this window context uses shared resources */
        if (win_ctx->uses_shared_context) {
            /* Check if we need to switch */
            if (g_opengl_priv->current_context != win_ctx ||
                g_opengl_priv->current_target_type != OPENGL_TARGET_WINDOW) {

                /* Simply make this window's context current - fast! */
                glAMakeCurrent((GLAContext)win_ctx->gl_context);

                /* Update state */
                g_opengl_priv->current_context = win_ctx;
                g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
                g_opengl_priv->current_window = window;
                g_opengl_priv->current_board = NULL;
                g_opengl_priv->current_width = width;
                g_opengl_priv->current_height = height;
                g_opengl_priv->context_switches++;

                /* Setup projection for new dimensions */
                OpenGL_SetupOrthoProjection(width, height);

                g_opengl_priv->needs_sync = TRUE;
            }
            return TRUE;
        }
        /* Fall through to glASetRast if this context doesn't share */
    }

fallback_setrast:
    /*
     * FALLBACK: Use the old glASetRast() method.
     * This is used when:
     * - Shared contexts are not supported
     * - Window context creation failed
     * - The window context doesn't share resources
     */

    /* Ensure global context exists (creates it if this is the first window) */
    if (!OpenGL_EnsureGlobalContext(window)) {
        return FALSE;
    }

    /* Check if we need to switch targets (including from DrawingBoard to Window) */
    if (g_opengl_priv->current_target_type != OPENGL_TARGET_WINDOW ||
        g_opengl_priv->current_window != window ||
        g_opengl_priv->current_width != width ||
        g_opengl_priv->current_height != height) {

        struct TagItem setrast_tags[6];
        WORD tag_idx = 0;

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
static BOOL OpenGL_SwitchToDrawingBoard(struct RenderContext *rctx)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;

    if (!rctx || !rctx->target_board || !g_opengl_priv) {
        return FALSE;
    }

    board = rctx->target_board;
    
    /*
     * CRITICAL: Ensure the global context is current before any GL operations.
     * Another application/compositor may have made a different context current.
     * FBOs are per-context, so we MUST use the same context that created the FBO.
     */
    if (g_opengl_priv->context_created && g_opengl_priv->gl_context) {
        GLAContext current_ctx = glAGetCurrentContext();
        if (current_ctx != (GLAContext)g_opengl_priv->gl_context) {
            D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: Wrong context active (%p), switching to global (%p)\n",
                  current_ctx, g_opengl_priv->gl_context));
            glAMakeCurrent((GLAContext)g_opengl_priv->gl_context);
        }
    }

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
        return FALSE;
    }

    /*
     * We need a GL context before we can create/use FBOs.
     * If no context exists yet, we need to create one first.
     * This requires a window - check multiple sources:
     * 1. DrawingBoard's parent_window (set by user)
     * 2. RenderContext's target_rastport Layer->Window
     */
    if (!g_opengl_priv->context_created) {
        struct Window *window = NULL;

        /* First, check if DrawingBoard has a parent_window set */
        if (board->parent_window) {
            window = board->parent_window;
        }
        /* Otherwise, try to get window from RenderContext's target_rastport */
        else if (rctx->target_rastport && rctx->target_rastport->Layer && rctx->target_rastport->Layer->Window) {
            window = (struct Window *)rctx->target_rastport->Layer->Window;
        }

        if (window) {
            /* Create context for the window */
            if (!OpenGL_EnsureGlobalContext(window)) {
                return FALSE;
            }
        } else {
            /* No window available - DrawingBoards require a window for GL context */
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
        D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: FBO not available, fallback to CyberGfx\n"));
        return FALSE;
    }

    /* Get or create FBO for this DrawingBoard */
    fbo = (OpenGLFBOData *)board->backend_data;

    if (!fbo) {
        /* Create FBO for this DrawingBoard */
        fbo = OpenGL_CreateFBO(board->width, board->height);
        if (!fbo) {
            return FALSE;
        }

        /* Store FBO in DrawingBoard */
        board->backend_data = fbo;

        /*
         * Track parent context for cleanup and for making the correct context
         * current when blitting. Note: current_context may be NULL if we're
         * using the global context path (which is common for single-context
         * AROS/Mesa). In that case, OpenGL_BlitFBOToRastPort will fall back
         * to g_opengl_priv->gl_context.
         */
        fbo->parent_context = g_opengl_priv->current_context;
    }

    /* Check if we need to switch to this FBO */
    if (g_opengl_priv->current_target_type != OPENGL_TARGET_DRAWINGBOARD ||
        g_opengl_priv->current_board != board) {

        D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: Binding FBO %u (was target_type=%d)\n",
              fbo->fbo_id, g_opengl_priv->current_target_type));

        /* Bind the FBO - this is much faster than glASetRast! */
        if (!OpenGL_BindFBO(fbo)) {
            D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: BindFBO FAILED\n"));
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
    } else {
        D(bug("[ZuneGfx:OpenGL] SwitchToDrawingBoard: Already on same board, just reset projection\n"));
        /*
         * Already on the same DrawingBoard - FBO is already bound.
         * However, we must ensure viewport and projection are correct,
         * as they may have been modified by other GL operations
         * (e.g., ZuneReload's texture drawing or window resize).
         *
         * This is a lightweight operation compared to full FBO binding,
         * but ensures rendering uses correct coordinate system.
         */
        glViewport(0, 0, fbo->width, fbo->height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, fbo->width, fbo->height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    return TRUE;
}

/*
 * OpenGL_SwitchToTarget - Unified function to switch GL context to any target
 *
 * Determines if the RenderContext targets a Window or DrawingBoard and calls
 * the appropriate switching function.
 *
 * Returns TRUE if switch was successful.
 */
static BOOL OpenGL_SwitchToTarget(struct RenderContext *rctx)
{
    if (!rctx || !g_opengl_priv) {
        return FALSE;
    }

    /* Check if this is a DrawingBoard target */
    if (rctx->target_board) {
        return OpenGL_SwitchToDrawingBoard(rctx);
    }

    /* Otherwise it's a Window-based RastPort */
    return OpenGL_SwitchToWindow(rctx);
}

/*
 * Set up 2D orthographic projection for pixel-perfect rendering
 */
static void OpenGL_SetupOrthoProjection(UWORD width, UWORD height)
{

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
static void OpenGL_SyncFromRastPort(struct RenderContext *rctx)
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

/*
 * Check if we need to sync from RastPort and do it if necessary.
 * This should be called before any OpenGL drawing operation.
 */
static void OpenGL_SyncIfNeeded(struct RenderContext *rctx)
{
    if (!g_opengl_priv || !g_opengl_priv->needs_sync) {
        return;
    }

//    OpenGL_SyncFromRastPort(rctx);
    g_opengl_priv->needs_sync = FALSE;
}

/*
 * Flush OpenGL rendering to make it visible
 *
 * In non-batched mode, we need to flush and swap after each operation
 * to make the rendering visible immediately.
 */
static void OpenGL_FlushIfNotBatching(struct RenderContext *rctx)
{
    if (!rctx || rctx->batching_enabled) {
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
/* RenderContext Management                                                     */
/*****************************************************************************/

static BOOL OpenGLInitRenderContext(struct RenderContext *rctx)
{
    if (!rctx) {
        return FALSE;
    }

    /*
     * With the global context approach, we don't need per-RenderContext context.
     * The global context is created on first use and shared across all windows.
     * glASetRast() is used to switch render targets as needed.
     */
    rctx->backend_context = NULL;

    return TRUE;
}

static void OpenGLCleanupRenderContext(struct RenderContext *rctx)
{
    if (!rctx) {
        return;
    }

    /*
     * With global context, nothing to clean up per-RenderContext.
     * The global context is destroyed in OpenGLCleanupBackend().
     */
}

/*****************************************************************************/
/* Color Management                                                          */
/*****************************************************************************/

static BOOL OpenGLPrepareColor(struct RenderContext *rctx,
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

static void OpenGLReleaseColor(struct RenderContext *rctx,
                               struct InternalColor *color)
{
    /* Nothing to release for OpenGL colors */
}

/*****************************************************************************/
/* Drawing Operations                                                        */
/*****************************************************************************/

static void OpenGLDrawPixel(struct RenderContext *rctx, WORD x, WORD y,
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

static void OpenGLDrawLine(struct RenderContext *rctx, WORD startX, WORD startY,
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

static void OpenGLDrawRectangle(struct RenderContext *rctx, WORD x, WORD y,
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
         *
         * For non-solid brushes, we use the textured shader variant which
         * samples fill color from a texture instead of a uniform.
         */
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
                    has_fill = FALSE; /* Failed to create texture, skip fill */
                    use_textured_shader = FALSE;
                }
            }

            if (use_textured_shader) {
                /* Use textured shader for non-solid brushes */
                glUseProgram_ptr(g_rounded_rect_textured_program);

                /* Bind the brush texture */
                glEnable(GL_TEXTURE_2D);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, brush_texture);

                /* Set uniforms */
                if (g_uniform_tex_rect_size >= 0 && glUniform2f_ptr)
                    glUniform2f_ptr(g_uniform_tex_rect_size, (GLfloat)width, (GLfloat)height);
                if (g_uniform_tex_rect_radius >= 0 && glUniform1f_ptr)
                    glUniform1f_ptr(g_uniform_tex_rect_radius, (GLfloat)corner_radius);
                if (g_uniform_tex_fill_texture >= 0 && glUniform1i_ptr)
                    glUniform1i_ptr(g_uniform_tex_fill_texture, 0); /* Texture unit 0 */
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

                /* Extract fill color for solid brush */
                if (has_fill && fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
                    fill_color_val = fill_brush->data.solid.color;
                    fill_r = ((fill_color_val >> 16) & 0xFF) / 255.0f;
                    fill_g = ((fill_color_val >> 8) & 0xFF) / 255.0f;
                    fill_b = (fill_color_val & 0xFF) / 255.0f;
                    fill_a = ((fill_color_val >> 24) & 0xFF) / 255.0f;
                } else {
                    has_fill = FALSE; /* Non-solid without textured shader = no fill */
                }

                glUseProgram_ptr(g_rounded_rect_program);

                /* Set uniforms */
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
            } else {
                /* Fallback to immediate mode */
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

            /* Deactivate shader */
            glUseProgram_ptr(0);
        } else {
            /*
             * Fallback: Draw rounded rectangle using geometry (no shaders)
             * This is the old implementation for systems without shader support.
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

static void OpenGLDrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y,
                             UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias)
{
    /* Use more segments for smoother circles, especially for AA */
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
        /* For now, only support solid color fills */
        if (fill_brush->type == ZUNE_BRUSH_TYPE_SOLID) {
            ULONG color = fill_brush->data.solid.color;
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

    OpenGL_FlushIfNotBatching(rctx);
    #undef CIRCLE_SEGMENTS
}

static void OpenGLClearRenderContext(struct RenderContext *rctx,
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

    /* Set clear color */
    glClearColor(
        color->r / 255.0f,
        color->g / 255.0f,
        color->b / 255.0f,
        color->a / 255.0f
    );

    glClear(GL_COLOR_BUFFER_BIT);

    OpenGL_FlushIfNotBatching(rctx);
}

/*****************************************************************************/
/* Direct Pixel Access                                                       */
/*****************************************************************************/

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out)
{
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
    return 0x00000000;
}

static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color)
{
    /* TODO: Would need to draw a point via the RenderContext */
}

/*****************************************************************************/
/* Batching Operations                                                       */
/*****************************************************************************/

static void OpenGLBeginBatch(struct RenderContext *rctx)
{
    /*
     * OpenGL naturally batches commands. We could use this to
     * defer glASwapBuffers until ZuneEndBatch.
     */
    if (rctx) {
        rctx->batching_enabled = TRUE;
    }
}

static void OpenGLEndBatch(struct RenderContext *rctx)
{
    if (!rctx) {
        return;
    }

    rctx->batching_enabled = FALSE;

    /* Flush and swap buffers using global context */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();
        glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
    }
}

static void OpenGLFlushBatch(struct RenderContext *rctx)
{
    if (!rctx) {
        return;
    }

    /* Flush and swap buffers using global context */
    if (g_opengl_priv && g_opengl_priv->gl_context) {
        glFlush();
        glASwapBuffers((GLAContext)g_opengl_priv->gl_context);
    }
}

static BOOL OpenGLIsBatching(struct RenderContext *rctx)
{
    if (!rctx) {
        return FALSE;
    }
    return rctx->batching_enabled;
}

/*****************************************************************************/
/* Blitting Operations                                                       */
/*****************************************************************************/

static void OpenGLBlitRenderContexts(struct RenderContext *source,
                                  struct RenderContext *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height)
{
    struct DrawingBoard *src_board, *dst_board;

    if (!source || !dest) {
        return;
    }

    src_board = source->target_board;
    dst_board = dest->target_board;

    /*
     * ZERO-COPY PATH: Both source and destination are DrawingBoards with FBOs
     *
     * This is the optimal case - we can blit directly from one FBO texture
     * to another FBO without involving the CPU at all. All data stays on GPU.
     */
    if (src_board && src_board->backend_data &&
        dst_board && dst_board->backend_data &&
        g_fbo_available) {

        OpenGLFBOData *src_fbo = (OpenGLFBOData *)src_board->backend_data;
        OpenGLFBOData *dst_fbo = (OpenGLFBOData *)dst_board->backend_data;

        if (src_fbo->valid && dst_fbo->valid) {
            OpenGL_BlitFBOToFBO(src_board, dst_board,
                                src_x, src_y, dest_x, dest_y, width, height);
            return;
        }
    }

    /*
     * FALLBACK PATH: Use software blitting via CyberGfx
     *
     * This handles cases where:
     * - Source or destination is a Window (not DrawingBoard)
     * - FBOs are not available
     * - One or both boards don't have valid FBO data
     */

    /* If source is a DrawingBoard with FBO, sync it to bitmap first */
    if (src_board && src_board->backend_data && g_fbo_available) {
        OpenGLFBOData *src_fbo = (OpenGLFBOData *)src_board->backend_data;
        if (src_fbo->valid && src_fbo->dirty) {
            OpenGL_SyncFBOToBitmap(source);
        }
    }

    /* Use BltBitMapRastPort for the actual blit if both have rastports */
    if (src_board && src_board->rastport && src_board->rastport->BitMap &&
        dst_board && dst_board->rastport && dst_board->rastport->BitMap) {

        BltBitMapRastPort(src_board->rastport->BitMap,
                          src_x, src_y,
                          dst_board->rastport,
                          dest_x, dest_y,
                          width, height,
                          0xC0);  /* Copy */
    }
}

static void OpenGLBlitToScreen(struct RenderContext *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height)
{
    struct DrawingBoard *board;
    OpenGLFBOData *fbo;
    struct Window *target_window = NULL;
    OpenGLWindowContext *win_ctx = NULL;

    if (!source || !screen_rp) {
        return;
    }

    board = source->target_board;

    /* Try to get target window from RastPort */
    if (screen_rp->Layer && screen_rp->Layer->Window) {
        target_window = (struct Window *)screen_rp->Layer->Window;
    }

    /*
     * GPU-ACCELERATED PATH: Blit FBO directly to window via shared context
     *
     * If the target RastPort belongs to a window that has a shared GL context,
     * we can render the FBO texture directly to that window's framebuffer.
     * This avoids the expensive glReadPixels + WritePixelArray roundtrip.
     */
    if (board && board->backend_data && g_fbo_available && target_window &&
        g_opengl_priv && g_opengl_priv->shared_contexts_supported) {

        fbo = (OpenGLFBOData *)board->backend_data;

        /* Find or create window context */
        win_ctx = OpenGL_FindWindowContext(target_window);
        if (!win_ctx) {
            /* Try to create shared context for this window */
            if (g_opengl_priv->master_context_created) {
                win_ctx = OpenGL_CreateWindowContext(target_window);
            }
        }

        /* If we have a shared window context, use GPU path */
        if (win_ctx && win_ctx->uses_shared_context && fbo->valid) {
            GLuint src_texture = fbo->texture_id;
            GLfloat tex_x1, tex_y1, tex_x2, tex_y2;
            UWORD win_width, win_height;

            /* Make window context current */
            glAMakeCurrent((GLAContext)win_ctx->gl_context);

            /* Update state */
            g_opengl_priv->current_context = win_ctx;
            g_opengl_priv->current_target_type = OPENGL_TARGET_WINDOW;
            g_opengl_priv->current_window = target_window;
            g_opengl_priv->current_board = NULL;
            g_opengl_priv->context_switches++;

            /* Setup projection for window */
            win_width = target_window->Width - target_window->BorderLeft - target_window->BorderRight;
            win_height = target_window->Height - target_window->BorderTop - target_window->BorderBottom;
            OpenGL_SetupOrthoProjection(win_width, win_height);

            /* Unbind any FBO - render to window's default framebuffer */
            glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

            /* Ensure source FBO rendering is complete */
            glFlush();

            /* Calculate texture coordinates */
            tex_x1 = (GLfloat)src_x / (GLfloat)fbo->width;
            tex_y1 = (GLfloat)src_y / (GLfloat)fbo->height;
            tex_x2 = (GLfloat)(src_x + width) / (GLfloat)fbo->width;
            tex_y2 = (GLfloat)(src_y + height) / (GLfloat)fbo->height;

            /* Flip Y for FBO texture */
            tex_y1 = 1.0f - tex_y1;
            tex_y2 = 1.0f - tex_y2;

            /* Setup for textured rendering */
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, src_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            /* Disable shader */
            if (glUseProgram_ptr) {
                glUseProgram_ptr(0);
            }

            /* Enable blending for alpha */
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            /* Draw textured quad to window */
            glBegin(GL_QUADS);
            glTexCoord2f(tex_x1, tex_y1); glVertex2i(dest_x, dest_y);
            glTexCoord2f(tex_x2, tex_y1); glVertex2i(dest_x + width, dest_y);
            glTexCoord2f(tex_x2, tex_y2); glVertex2i(dest_x + width, dest_y + height);
            glTexCoord2f(tex_x1, tex_y2); glVertex2i(dest_x, dest_y + height);
            glEnd();

            glDisable(GL_TEXTURE_2D);

            /* Swap to make visible */
            glASwapBuffers((GLAContext)win_ctx->gl_context);
            return;
        }
    }

    /*
     * FALLBACK: FBO-based software blitting
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

        if (!CyberGfxBase) {
            return;
        }

        /* Clamp dimensions to FBO size */
        if (src_x + width > fbo->width) width = fbo->width - src_x;
        if (src_y + height > fbo->height) height = fbo->height - src_y;
        if (width == 0 || height == 0) return;

        /* Allocate buffers */
        pixelbuffer = AllocVec(width * height * 4, MEMF_ANY);
        if (!pixelbuffer) {
            return;
        }

        flipped_buffer = AllocVec(width * height * 4, MEMF_ANY);
        if (!flipped_buffer) {
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
    } else {
        /*
         * No FBO: rendering went directly to window's GL buffer.
         * Just swap buffers to make it visible.
         */
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

    return TRUE;
}

void OpenGLCleanupDrawingBoard(struct DrawingBoard *board)
{
    if (!board) {
        return;
    }

    /*
     * Destroy the FBO if one was created for this DrawingBoard
     */
    if (board->backend_data) {
        OpenGLFBOData *fbo = (OpenGLFBOData *)board->backend_data;

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
}
