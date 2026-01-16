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

/* Forward declarations */
struct DrawingBoard;

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
 * Target type for the current GL context binding
 */
typedef enum {
    OPENGL_TARGET_NONE = 0,
    OPENGL_TARGET_WINDOW,
    OPENGL_TARGET_DRAWINGBOARD
} OpenGLTargetType;

/*****************************************************************************/
/* Per-Window GL Context                                                     */
/*****************************************************************************/

/*
 * OpenGLWindowContext - GL context data for a specific window
 *
 * Each window that uses OpenGL rendering gets its own GL context.
 * This allows multiple windows to render independently without
 * the overhead of glASetRast() calls between them.
 */
typedef struct OpenGLWindowContext {
    struct Window *window;          /* The window this context belongs to */
    APTR        gl_context;         /* GLAContext for this window */
    BOOL        context_valid;      /* Context is valid and usable */
    BOOL        shaders_initialized; /* Shaders compiled for this context */
    BOOL        uses_shared_context; /* TRUE if created with GLA_ShareContext */
    UWORD       width;              /* Current framebuffer width */
    UWORD       height;             /* Current framebuffer height */

    /* Linked list for tracking all window contexts */
    struct OpenGLWindowContext *next;
} OpenGLWindowContext;

/*****************************************************************************/
/* FBO Data for DrawingBoards                                                */
/*****************************************************************************/

/*
 * OpenGLFBOData - FBO (Framebuffer Object) data for a DrawingBoard
 *
 * Each DrawingBoard gets its own FBO for off-screen rendering.
 * The FBO is attached to the parent window's GL context.
 * Switching between DrawingBoards is done via glBindFramebuffer (fast).
 */
typedef struct OpenGLFBOData {
    ULONG       fbo_id;             /* OpenGL framebuffer object ID */
    ULONG       texture_id;         /* Color attachment texture ID */
    ULONG       depth_rb_id;        /* Depth renderbuffer ID (optional) */
    UWORD       width;              /* FBO width */
    UWORD       height;             /* FBO height */
    BOOL        valid;              /* FBO is valid and complete */
    BOOL        dirty;              /* FBO content modified, needs sync to bitmap */

    /* Zero-copy compositing support */
    APTR        pipe_resource;      /* Gallium pipe_resource for zero-copy access */
    BOOL        resource_valid;     /* pipe_resource is valid for texture use */

    /* Parent context - the window context this FBO belongs to */
    OpenGLWindowContext *parent_context;
} OpenGLFBOData;

/*****************************************************************************/
/* Global OpenGL Backend State                                               */
/*****************************************************************************/

/*
 * Global OpenGL backend state
 * Stored in ZuneBackendContext->private_data
 *
 * ARCHITECTURE:
 * - Each Window gets its own GL context (via glACreateContext with GLA_Window)
 * - Each DrawingBoard gets its own FBO within the active window's context
 * - Switching between windows requires glAMakeCurrent()
 * - Switching between DrawingBoards only requires glBindFramebuffer() (fast)
 *
 * This avoids the glASetRast() crashes when switching between targets
 * with different dimensions.
 */
typedef struct OpenGLPrivateData {
    struct Library *GLBase;         /* gl.library base */
    BOOL        initialized;        /* Backend initialized successfully */
    BOOL        gl_available;       /* GL library available and working */

    /* Master GL context for shared resource management */
    APTR        master_context;     /* Master GL context - all others share with this */
    BOOL        master_context_created; /* Master context has been created */
    BOOL        shared_contexts_supported; /* GLA_ShareContext is working */

    /* Global GL context (for single-context fallback mode) */
    APTR        gl_context;         /* GLAContext - the single global context */
    BOOL        context_created;    /* Global context has been created */

    /* Window context management (for multi-context FBO mode) */
    OpenGLWindowContext *window_contexts;   /* Linked list of window contexts */
    OpenGLWindowContext *current_context;   /* Currently active window context */

    /* Current target tracking */
    OpenGLTargetType current_target_type;   /* Type of current target */
    struct Window *current_window;          /* Currently bound window (if WINDOW) */
    struct DrawingBoard *current_board;     /* Currently bound DrawingBoard (if DRAWINGBOARD) */
    UWORD       current_width;              /* Current framebuffer width */
    UWORD       current_height;             /* Current framebuffer height */
    BOOL        needs_sync;                 /* Need to sync from RastPort before drawing */

    /* GL capabilities detected at init */
    ULONG       gl_version_major;   /* OpenGL major version */
    ULONG       gl_version_minor;   /* OpenGL minor version */
    ULONG       max_texture_size;   /* Maximum texture dimension */
    BOOL        has_npot_textures;  /* Non-power-of-two texture support */
    BOOL        has_framebuffers;   /* Framebuffer object support (FBO) */
    BOOL        has_shaders;        /* Shader support (GLSL) */

    /* Statistics */
    ULONG       contexts_created;   /* Number of GL contexts created */
    ULONG       fbos_created;       /* Number of FBOs created */
    ULONG       draw_calls;         /* Total draw calls (for debugging) */
    ULONG       context_switches;   /* Number of context switches */
    ULONG       fbo_switches;       /* Number of FBO switches */
    ULONG       setrast_calls;      /* Number of glASetRast calls */

} OpenGLPrivateData;

/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/*
 * OpenGL_GetMasterContext - Get the master GL context for sharing
 *
 * Returns the master GL context that can be passed to other components
 * (like LayerCompositor) via GLA_ShareContext to enable resource sharing
 * between contexts.
 *
 * Returns NULL if no master context has been created yet.
 */
APTR OpenGL_GetMasterContext(void);

/*
 * OpenGL_EnsureMasterContext - Ensure master context exists
 *
 * Creates the master GL context if it doesn't exist, using the given window.
 * Returns the master context or NULL on failure.
 */
APTR OpenGL_EnsureMasterContext(struct Window *window);

#endif /* OPENGL_BACKEND_H */
