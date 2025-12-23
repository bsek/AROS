/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Header

    This header defines the OpenGL rendering backend for ZuneRenderer.
    The backend uses AROS's gl.library (mesa3dgl or hostgl) to provide
    hardware-accelerated 2D rendering using OpenGL.
*/

#ifndef OPENGL_BACKEND_H
#define OPENGL_BACKEND_H

#include <exec/types.h>
#include "../backend_interface.h"

/*****************************************************************************/
/* OpenGL Backend Constants                                                  */
/*****************************************************************************/

#define OPENGL_MIN_VERSION      20  /* Minimum gl.library version required */
#define OPENGL_BACKEND_NAME     "OpenGL"

/*****************************************************************************/
/* OpenGL Backend Export                                                     */
/*****************************************************************************/

/* Backend operations table - defined in opengl_backend.c */
extern ZuneBackendOps opengl_backend_ops;

/*****************************************************************************/
/* OpenGL Backend Private Data                                               */
/*****************************************************************************/

/*
 * OpenGL context state for a window/RenderPort
 * Each RenderPort that uses OpenGL gets its own GL context
 */
typedef struct OpenGLContextData {
    APTR        gl_context;         /* GLAContext from gl.library */
    BOOL        context_active;     /* Is this context current? */
    UWORD       viewport_width;     /* Current viewport width */
    UWORD       viewport_height;    /* Current viewport height */
} OpenGLContextData;

/*
 * Global OpenGL backend state
 */
typedef struct OpenGLPrivateData {
    struct Library *GLBase;         /* gl.library base */
    BOOL        initialized;        /* Backend initialized successfully */
    BOOL        gl_available;       /* GL library available and working */
    
    /* GL capabilities detected at init */
    ULONG       gl_version_major;   /* OpenGL major version */
    ULONG       gl_version_minor;   /* OpenGL minor version */
    ULONG       max_texture_size;   /* Maximum texture dimension */
    BOOL        has_npot_textures;  /* Non-power-of-two texture support */
    BOOL        has_framebuffers;   /* Framebuffer object support */
    BOOL        has_shaders;        /* Shader support (GLSL) */
    
    /* Statistics */
    ULONG       contexts_created;   /* Number of GL contexts created */
    ULONG       draw_calls;         /* Total draw calls (for debugging) */
    
} OpenGLPrivateData;

/*****************************************************************************/
/* OpenGL Backend Functions                                                  */
/*****************************************************************************/

/* Library management */
BOOL OpenGL_CheckLibrary(OpenGLPrivateData *priv);
BOOL OpenGL_CheckCapabilities(OpenGLPrivateData *priv);

/* Context management */
APTR OpenGL_CreateContext(struct Window *window);
void OpenGL_DestroyContext(APTR context);
BOOL OpenGL_MakeCurrent(APTR context);
void OpenGL_SwapBuffers(APTR context);

/* Debug/Info */
void OpenGL_DumpDebugInfo(OpenGLPrivateData *priv);
const char *OpenGL_GetErrorString(ULONG error);

#endif /* OPENGL_BACKEND_H */
