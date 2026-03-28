/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - OpenGL Backend Shader Functions

    This file contains all shader-related code extracted from opengl_backend.c:
    - GLSL shader source strings
    - Shader function pointer loading
    - VBO function pointer loading
    - Shader compilation and linking
    - Shader initialization and cleanup
*/

#include "opengl_intern.h"

/*****************************************************************************/
/* Rounded Rectangle Shader Source                                           */
/*****************************************************************************/

/*
 * Vertex Shader for Rounded Rectangle
 * Simply passes through position and texture coordinates
 */
const GLchar *g_rounded_rect_vs_source =
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
const GLchar *g_rounded_rect_fs_source =
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
const GLchar *g_rounded_rect_textured_fs_source =
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
/* Shader and VBO Function Loading                                           */
/*****************************************************************************/

BOOL OpenGL_LoadShaderFunctions(void)
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
BOOL OpenGL_LoadVBOFunctions(void)
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

/*****************************************************************************/
/* VBO Management                                                            */
/*****************************************************************************/

/*
 * OpenGL_CreateQuadVBO - Create a shared VBO for unit quad rendering
 *
 * Returns TRUE on success.
 */
BOOL OpenGL_CreateQuadVBO(void)
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
    glBufferData_ptr(GL_ARRAY_BUFFER, G_QUAD_VERTICES_SIZE, g_quad_vertices, GL_STATIC_DRAW);
    glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);

    return TRUE;
}

/*
 * OpenGL_DestroyQuadVBO - Destroy the shared quad VBO
 */
void OpenGL_DestroyQuadVBO(void)
{
    if (g_quad_vbo != 0 && glDeleteBuffers_ptr) {
        glDeleteBuffers_ptr(1, &g_quad_vbo);
        g_quad_vbo = 0;
    }
}

/*****************************************************************************/
/* Shader Compilation                                                        */
/*****************************************************************************/

/*
 * OpenGL_CompileShader - Compile a shader from source
 *
 * Returns shader ID on success, 0 on failure.
 */
GLuint OpenGL_CompileShader(GLenum type, const GLchar *source)
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

/*****************************************************************************/
/* Shader Program Management                                                 */
/*****************************************************************************/

/*
 * OpenGL_CreateRoundedRectShader - Create the rounded rectangle shader program
 *
 * Returns TRUE on success.
 */
BOOL OpenGL_CreateRoundedRectShader(void)
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
void OpenGL_DestroyShaders(void)
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

/*****************************************************************************/
/* Shader Initialization                                                     */
/*****************************************************************************/

/*
 * OpenGL_InitShadersInternal - Actually compile and link shaders
 *
 * This does the actual shader compilation work. Called either from
 * OpenGL_InitShaders() if stack is large enough, or from
 * OpenGL_PreInitializeShaders() during library init.
 *
 * REQUIRES: GL context must be current, stack must be >= ZUNEGFX_SHADER_SAFESTACK
 */
BOOL OpenGL_InitShadersInternal(void)
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
BOOL OpenGL_InitShaders(void)
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
