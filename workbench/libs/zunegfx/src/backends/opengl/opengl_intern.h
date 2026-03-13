/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Internal Header

    Shared declarations for all OpenGL backend source files.
    This header provides access to GL extension function pointers,
    global state, and internal helper functions.
*/

#ifndef OPENGL_INTERN_H
#define OPENGL_INTERN_H

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

#define DEBUG 1
#include <aros/debug.h>

#include "../backend_interface.h"
#include "opengl_backend.h"

/*****************************************************************************/
/* External Library Bases                                                    */
/*****************************************************************************/

extern struct Library *CyberGfxBase;
extern struct Library *GLBase;

/*****************************************************************************/
/* Shader Function Pointers                                                  */
/*****************************************************************************/

extern PFNGLCREATESHADERPROC        glCreateShader_ptr;
extern PFNGLSHADERSOURCEPROC        glShaderSource_ptr;
extern PFNGLCOMPILESHADERPROC       glCompileShader_ptr;
extern PFNGLGETSHADERINFOLOGPROC    glGetShaderInfoLog_ptr;
extern PFNGLGETSHADERIVPROC         glGetShaderiv_ptr;
extern PFNGLCREATEPROGRAMPROC       glCreateProgram_ptr;
extern PFNGLATTACHSHADERPROC        glAttachShader_ptr;
extern PFNGLLINKPROGRAMPROC         glLinkProgram_ptr;
extern PFNGLGETPROGRAMINFOLOGPROC   glGetProgramInfoLog_ptr;
extern PFNGLGETPROGRAMIVPROC        glGetProgramiv_ptr;
extern PFNGLUSEPROGRAMPROC          glUseProgram_ptr;
extern PFNGLDETACHSHADERPROC        glDetachShader_ptr;
extern PFNGLDELETESHADERPROC        glDeleteShader_ptr;
extern PFNGLDELETEPROGRAMPROC       glDeleteProgram_ptr;
extern PFNGLGETUNIFORMLOCATIONPROC  glGetUniformLocation_ptr;
extern PFNGLUNIFORM1FPROC           glUniform1f_ptr;
extern PFNGLUNIFORM1IPROC           glUniform1i_ptr;
extern PFNGLUNIFORM2FPROC           glUniform2f_ptr;
extern PFNGLUNIFORM4FPROC           glUniform4f_ptr;

/*****************************************************************************/
/* FBO Function Pointer Types and Pointers                                   */
/*****************************************************************************/

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

extern PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers_ptr;
extern PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers_ptr;
extern PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer_ptr;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus_ptr;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D_ptr;
extern PFNGLGENRENDERBUFFERSPROC        glGenRenderbuffers_ptr;
extern PFNGLDELETERENDERBUFFERSPROC     glDeleteRenderbuffers_ptr;
extern PFNGLBINDRENDERBUFFERPROC        glBindRenderbuffer_ptr;
extern PFNGLRENDERBUFFERSTORAGEPROC     glRenderbufferStorage_ptr;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ptr;

/* FBO constants */
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

/*****************************************************************************/
/* VBO Function Pointer Types and Pointers                                   */
/*****************************************************************************/

typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);

extern PFNGLGENBUFFERSPROC              glGenBuffers_ptr;
extern PFNGLDELETEBUFFERSPROC           glDeleteBuffers_ptr;
extern PFNGLBINDBUFFERPROC              glBindBuffer_ptr;
extern PFNGLBUFFERDATAPROC              glBufferData_ptr;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ptr;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ptr;
extern PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer_ptr;
extern PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation_ptr;

/* VBO constants */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#endif

/*****************************************************************************/
/* Global State                                                              */
/*****************************************************************************/

extern BOOL g_fbo_available;
extern BOOL g_vbo_available;
extern BOOL g_shaders_available;
extern GLuint g_quad_vbo;
/* 4 vertices * 4 floats (x,y,s,t) = 16 floats */
#define G_QUAD_VERTICES_COUNT 16
#define G_QUAD_VERTICES_SIZE  (G_QUAD_VERTICES_COUNT * sizeof(GLfloat))
extern const GLfloat g_quad_vertices[G_QUAD_VERTICES_COUNT];
extern GLint g_attrib_position;
extern GLint g_attrib_texcoord;

/* Shader programs */
extern GLuint g_rounded_rect_program;
extern GLuint g_rounded_rect_vs;
extern GLuint g_rounded_rect_fs;
extern GLuint g_rounded_rect_textured_program;
extern GLuint g_rounded_rect_textured_fs;

/* Shader uniform locations - solid color shader */
extern GLint g_uniform_rect_size;
extern GLint g_uniform_rect_radius;
extern GLint g_uniform_fill_color;
extern GLint g_uniform_border_color;
extern GLint g_uniform_border_width;
extern GLint g_uniform_has_border;
extern GLint g_uniform_has_fill;

/* Shader uniform locations - textured shader */
extern GLint g_uniform_tex_rect_size;
extern GLint g_uniform_tex_rect_radius;
extern GLint g_uniform_tex_fill_texture;
extern GLint g_uniform_tex_border_color;
extern GLint g_uniform_tex_border_width;
extern GLint g_uniform_tex_has_border;
extern GLint g_uniform_tex_has_fill;

/* Shader source strings */
extern const GLchar *g_rounded_rect_vs_source;
extern const GLchar *g_rounded_rect_fs_source;
extern const GLchar *g_rounded_rect_textured_fs_source;

/* Global backend private data */
extern OpenGLPrivateData *g_opengl_priv;

/* Pre-init resources */
extern struct Screen *g_preinit_screen;
extern struct Window *g_preinit_window;
extern BOOL g_using_compositor_context;

/* Availability cache */
extern BOOL g_opengl_available_cached;
extern BOOL g_opengl_available_checked;

/* Minimum stack size for shader compilation */
#define ZUNEGFX_SHADER_SAFESTACK    (1 << 18)  /* 256KB */

/*****************************************************************************/
/* Internal Function Declarations - Shaders (opengl_shaders.c)               */
/*****************************************************************************/

BOOL OpenGL_LoadShaderFunctions(void);
BOOL OpenGL_LoadVBOFunctions(void);
BOOL OpenGL_CreateQuadVBO(void);
void OpenGL_DestroyQuadVBO(void);
GLuint OpenGL_CompileShader(GLenum type, const GLchar *source);
BOOL OpenGL_CreateRoundedRectShader(void);
void OpenGL_DestroyShaders(void);
BOOL OpenGL_InitShadersInternal(void);
BOOL OpenGL_InitShaders(void);

/*****************************************************************************/
/* Internal Function Declarations - FBO (opengl_fbo.c)                       */
/*****************************************************************************/

BOOL OpenGL_LoadFBOFunctions(void);
OpenGLFBOData *OpenGL_CreateFBO(UWORD width, UWORD height);
void OpenGL_DestroyFBO(OpenGLFBOData *fbo);
BOOL OpenGL_BindFBO(OpenGLFBOData *fbo);
void OpenGL_UnbindFBO(void);
GLuint OpenGL_GetFBOAsTexture(struct DrawingBoard *board);
void OpenGL_BlitFBOToFBO(struct DrawingBoard *src, struct DrawingBoard *dst,
                          WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                          UWORD width, UWORD height);

/*****************************************************************************/
/* Internal Function Declarations - Context (opengl_context.c)               */
/*****************************************************************************/

BOOL OpenGL_CreateMasterContext(struct Window *window);
GLAContext TryHeadlessContext(void);
BOOL OpenGL_PreInitializeShaders(void);
void OpenGL_CleanupPreInit(void);
APTR OpenGL_GetMasterContext(void);
APTR OpenGL_EnsureMasterContext(struct Window *window);
OpenGLWindowContext *OpenGL_CreateWindowContext(struct Window *window);
void OpenGL_DestroyWindowContext(OpenGLWindowContext *ctx);
OpenGLWindowContext *OpenGL_FindWindowContext(struct Window *window);
BOOL OpenGL_MakeContextCurrent(OpenGLWindowContext *ctx);
BOOL OpenGL_EnsureGlobalContext(struct Window *window);
void OpenGL_SwapBuffers(void);

/*****************************************************************************/
/* Internal Function Declarations - Sync (opengl_sync.c)                     */
/*****************************************************************************/

void OpenGLCopyFromRastPort(struct RenderContext *rctx, struct RastPort *src_rp,
                            WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                            UWORD width, UWORD height);
void OpenGL_BlitToRastPortDirect(struct RastPort *dst_rp, WORD dst_x, WORD dst_y,
                                 UWORD width, UWORD height);
void OpenGL_BlitFBOToRastPort(struct DrawingBoard *board, struct RastPort *dst_rp,
                              WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                              UWORD width, UWORD height);
BOOL OpenGL_SyncFBOToBitmap(struct RenderContext *rctx);
BOOL OpenGL_SyncRegionFBOToBitmap(struct RenderContext *rctx,
                                  WORD x, WORD y, UWORD width, UWORD height);
void OpenGL_SyncFromRastPort(struct RenderContext *rctx);
void OpenGL_SyncIfNeeded(struct RenderContext *rctx);
void OpenGL_FlushIfNotBatching(struct RenderContext *rctx);

/*****************************************************************************/
/* Internal Function Declarations - Pixel Utils (opengl_pixel_utils.c)       */
/*****************************************************************************/

void OpenGL_FlipPixelBufferY(UBYTE *buffer, UWORD width, UWORD height, UBYTE *temp);
void OpenGL_FlipPixelBufferYCopy(const UBYTE *src, UBYTE *dst, UWORD width, UWORD height);
UBYTE *OpenGL_ReadPixelsToBuffer(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_y);
UBYTE *OpenGL_ReadRastPortToBuffer(struct RastPort *rp, WORD x, WORD y,
                                   UWORD width, UWORD height, BOOL force_opaque);
GLuint OpenGL_UploadTextureFromBuffer(const UBYTE *buffer, UWORD width, UWORD height);
void OpenGL_DrawTexturedQuad(WORD x, WORD y, UWORD width, UWORD height, BOOL flip_texcoord);
GLuint OpenGL_BrushToTexture(struct RenderContext *rctx, struct ZuneBrush *brush,
                             WORD x, WORD y, UWORD width, UWORD height);

/*****************************************************************************/
/* Internal Function Declarations - Drawing (opengl_drawing.c)               */
/*****************************************************************************/

BOOL OpenGLInitRenderContext(struct RenderContext *rctx);
void OpenGLCleanupRenderContext(struct RenderContext *rctx);
BOOL OpenGLPrepareColor(struct RenderContext *rctx, struct InternalColor *color);
void OpenGLReleaseColor(struct RenderContext *rctx, struct InternalColor *color);
void OpenGLDrawPixel(struct RenderContext *rctx, WORD x, WORD y,
                     struct InternalColor *color, BOOL antialias);
void OpenGLDrawLine(struct RenderContext *rctx, WORD startX, WORD startY,
                    WORD endX, WORD endY, UWORD width,
                    struct InternalColor *color, BOOL antialias);
void OpenGLDrawRectangle(struct RenderContext *rctx, WORD x, WORD y,
                         UWORD width, UWORD height, UBYTE border_width,
                         UBYTE corner_radius, struct ZuneBrush *fill_brush,
                         struct InternalColor *border_color, BOOL filled,
                         BOOL antialias);
void OpenGLDrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y,
                      UWORD radius, UBYTE border_width,
                      struct ZuneBrush *fill_brush,
                      struct InternalColor *border_color, BOOL filled,
                      BOOL antialias);
void OpenGLClearRenderContext(struct RenderContext *rctx,
                              struct InternalColor *color);

/*****************************************************************************/
/* Internal Function Declarations - Backend core (opengl_backend.c)          */
/*****************************************************************************/

BOOL OpenGL_CheckLibrary(OpenGLPrivateData *priv);
BOOL OpenGL_CheckCapabilities(OpenGLPrivateData *priv);
void OpenGL_DumpDebugInfo(OpenGLPrivateData *priv);

BOOL OpenGL_SwitchToWindow(struct RenderContext *rctx);
BOOL OpenGL_SwitchToDrawingBoard(struct RenderContext *rctx);
BOOL OpenGL_SwitchToTarget(struct RenderContext *rctx);
void OpenGL_SetupOrthoProjection(UWORD width, UWORD height);
void OpenGL_SetColor(struct InternalColor *color);

/*****************************************************************************/
/* Fallback Functions (from zunegfx_fallback.c)                              */
/*****************************************************************************/

void ZuneFallback_DrawPixel(struct RenderContext *rctx, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias);
void ZuneFallback_DrawLine(struct RenderContext *rctx, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias);
void ZuneFallback_DrawRectangle(struct RenderContext *rctx, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius, struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias);
void ZuneFallback_DrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y,
                             UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias);
void ZuneFallback_ClearRenderContext(struct RenderContext *rctx,
                                    struct InternalColor *color);

#endif /* OPENGL_INTERN_H */
