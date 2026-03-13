/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Global Variable Definitions

    All global state variables used across multiple OpenGL backend files.
    Declared as extern in opengl_intern.h.
*/

#include "opengl_intern.h"

/*****************************************************************************/
/* Shader Function Pointers                                                  */
/*****************************************************************************/

PFNGLCREATESHADERPROC        glCreateShader_ptr      = NULL;
PFNGLSHADERSOURCEPROC        glShaderSource_ptr      = NULL;
PFNGLCOMPILESHADERPROC       glCompileShader_ptr     = NULL;
PFNGLGETSHADERINFOLOGPROC    glGetShaderInfoLog_ptr  = NULL;
PFNGLGETSHADERIVPROC         glGetShaderiv_ptr       = NULL;
PFNGLCREATEPROGRAMPROC       glCreateProgram_ptr     = NULL;
PFNGLATTACHSHADERPROC        glAttachShader_ptr      = NULL;
PFNGLLINKPROGRAMPROC         glLinkProgram_ptr       = NULL;
PFNGLGETPROGRAMINFOLOGPROC   glGetProgramInfoLog_ptr = NULL;
PFNGLGETPROGRAMIVPROC        glGetProgramiv_ptr      = NULL;
PFNGLUSEPROGRAMPROC          glUseProgram_ptr        = NULL;
PFNGLDETACHSHADERPROC        glDetachShader_ptr      = NULL;
PFNGLDELETESHADERPROC        glDeleteShader_ptr      = NULL;
PFNGLDELETEPROGRAMPROC       glDeleteProgram_ptr     = NULL;
PFNGLGETUNIFORMLOCATIONPROC  glGetUniformLocation_ptr = NULL;
PFNGLUNIFORM1FPROC           glUniform1f_ptr         = NULL;
PFNGLUNIFORM1IPROC           glUniform1i_ptr         = NULL;
PFNGLUNIFORM2FPROC           glUniform2f_ptr         = NULL;
PFNGLUNIFORM4FPROC           glUniform4f_ptr         = NULL;

/*****************************************************************************/
/* FBO Function Pointers                                                     */
/*****************************************************************************/

PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers_ptr = NULL;
PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers_ptr = NULL;
PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer_ptr = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus_ptr = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D_ptr = NULL;
PFNGLGENRENDERBUFFERSPROC        glGenRenderbuffers_ptr = NULL;
PFNGLDELETERENDERBUFFERSPROC     glDeleteRenderbuffers_ptr = NULL;
PFNGLBINDRENDERBUFFERPROC        glBindRenderbuffer_ptr = NULL;
PFNGLRENDERBUFFERSTORAGEPROC     glRenderbufferStorage_ptr = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ptr = NULL;

BOOL g_fbo_available = FALSE;

/*****************************************************************************/
/* VBO Function Pointers and State                                           */
/*****************************************************************************/

PFNGLGENBUFFERSPROC              glGenBuffers_ptr = NULL;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers_ptr = NULL;
PFNGLBINDBUFFERPROC              glBindBuffer_ptr = NULL;
PFNGLBUFFERDATAPROC              glBufferData_ptr = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ptr = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ptr = NULL;
PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer_ptr = NULL;
PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation_ptr = NULL;

BOOL g_vbo_available = FALSE;
GLuint g_quad_vbo = 0;

/* Quad vertex data: position (x,y) + texcoord (s,t) */
const GLfloat g_quad_vertices[G_QUAD_VERTICES_COUNT] = {
    /* x,    y,    s,    t */
    0.0f, 0.0f, 0.0f, 0.0f,  /* Bottom-left */
    1.0f, 0.0f, 1.0f, 0.0f,  /* Bottom-right */
    1.0f, 1.0f, 1.0f, 1.0f,  /* Top-right */
    0.0f, 1.0f, 0.0f, 1.0f,  /* Top-left */
};

GLint g_attrib_position = -1;
GLint g_attrib_texcoord = -1;

/*****************************************************************************/
/* Shader State                                                              */
/*****************************************************************************/

BOOL g_shaders_available = FALSE;
GLuint g_rounded_rect_program = 0;
GLuint g_rounded_rect_vs = 0;
GLuint g_rounded_rect_fs = 0;

GLuint g_rounded_rect_textured_program = 0;
GLuint g_rounded_rect_textured_fs = 0;

/* Shader uniform locations - solid color shader */
GLint g_uniform_rect_size = -1;
GLint g_uniform_rect_radius = -1;
GLint g_uniform_fill_color = -1;
GLint g_uniform_border_color = -1;
GLint g_uniform_border_width = -1;
GLint g_uniform_has_border = -1;
GLint g_uniform_has_fill = -1;

/* Shader uniform locations - textured shader */
GLint g_uniform_tex_rect_size = -1;
GLint g_uniform_tex_rect_radius = -1;
GLint g_uniform_tex_fill_texture = -1;
GLint g_uniform_tex_border_color = -1;
GLint g_uniform_tex_border_width = -1;
GLint g_uniform_tex_has_border = -1;
GLint g_uniform_tex_has_fill = -1;

/*****************************************************************************/
/* Global Backend State                                                      */
/*****************************************************************************/

OpenGLPrivateData *g_opengl_priv = NULL;

/* Pre-init resources */
struct Screen *g_preinit_screen = NULL;
struct Window *g_preinit_window = NULL;
BOOL g_using_compositor_context = FALSE;

/* Availability cache */
BOOL g_opengl_available_cached = FALSE;
BOOL g_opengl_available_checked = FALSE;
