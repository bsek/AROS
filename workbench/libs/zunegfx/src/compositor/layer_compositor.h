/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: Layer Compositor - Hardware-accelerated window compositing
    
    HYBRID COMPOSITING ARCHITECTURE
    ===============================
    
    This compositor uses a hybrid approach:
    
    1. Standard Intuition windows render normally through the layer system
       - They draw directly to the screen bitmap as usual
       - No compositor involvement for opaque windows
    
    2. ZuneGfx windows with alpha are composited by this module
       - They render to their own FBO (off-screen)
       - Compositor blends them over the screen content
       - Enables true transparency with windows behind
    
    The compositor does NOT hook into _ShowPartsOfLayer for standard windows.
    Instead, it's triggered after normal layer updates to overlay alpha windows.
    
    Data flow:
    
    [Standard Window] --> layer system --> Screen bitmap
                                                |
                                                v
    [ZuneGfx Window] --> FBO -----> Compositor --> OpenGL blend --> Screen
*/

#ifndef ZUNEGFX_LAYER_COMPOSITOR_H
#define ZUNEGFX_LAYER_COMPOSITOR_H

#include <exec/types.h>
#include <exec/lists.h>
#include <graphics/layers.h>
#include <graphics/regions.h>
#include <utility/hooks.h>
#include <oop/oop.h>

/* Forward declarations */
struct Layer;
struct Layer_Info;
struct Screen;
struct RastPort;
struct BitMap;
struct Window;
struct DrawingBoard;

/*
 * CompositorWindow - Tracks a zunegfx window for compositing
 *
 * Only zunegfx windows with alpha support are registered here.
 * Standard windows are NOT tracked - they render via normal layer system.
 */
struct CompositorWindow
{
    struct MinNode  cw_Node;
    struct Window  *cw_Window;          /* The Intuition window */
    struct Layer   *cw_Layer;           /* The window's layer */
    APTR            cw_GLContext;       /* GL context for this window (from zunegfx) */
    struct DrawingBoard *cw_DrawingBoard; /* The zunegfx DrawingBoard */
    ULONG           cw_TextureID;       /* Cached GL texture ID */
    APTR            cw_PipeResource;    /* Gallium pipe_resource (for zero-copy) */
    UBYTE           cw_Alpha;           /* Global alpha 0-255 */
    BOOL            cw_Valid;           /* Registration is valid */
    BOOL            cw_Dirty;           /* Content changed, needs re-read */
    BOOL            cw_Visible;         /* Window is currently visible */
    UWORD           cw_Width;           /* Window content width */
    UWORD           cw_Height;          /* Window content height */
};

/*
 * LayerCompositor - Main compositor state for a screen
 *
 * HYBRID MODE:
 * - Does NOT install hook in Layer_Info (standard windows work normally)
 * - Only composites registered zunegfx windows
 * - Called after screen updates to blend alpha windows
 */
struct LayerCompositor
{
    struct Screen      *lc_Screen;          /* Screen we're attached to */
    struct Layer_Info  *lc_LayerInfo;       /* Layer_Info for reference */
    
    /* Registered zunegfx windows with alpha */
    struct MinList      lc_Windows;         /* List of CompositorWindow */
    ULONG               lc_WindowCount;     /* Number of registered windows */
    
    /* OpenGL context - shared with zunegfx backend */
    APTR                lc_GLContext;       /* GL context for compositing */
    APTR                lc_MasterContext;   /* Master context we share with */
    BOOL                lc_OwnsGLContext;   /* TRUE if we created the context */
    BOOL                lc_ContextValid;    /* GL context is valid and usable */
    
    /* Screen dimensions */
    UWORD               lc_Width;
    UWORD               lc_Height;
    
    /* Compositor hook - installed in LayerInfo for alpha layers */
    struct Hook         lc_Hook;
    
    /* Compositor state */
    BOOL                lc_Active;          /* Compositor is active */
    BOOL                lc_NeedsComposite;  /* Compositing pass needed */
    
    /* Damage tracking for incremental updates */
    struct Region      *lc_DamageRegion;    /* Region that needs recomposite */
    
    /* Shaders */
    ULONG               lc_CompositeShader; /* Basic texture compositing shader */
    BOOL                lc_ShadersValid;    /* Shaders compiled successfully */
    BOOL                lc_ShadersSupported;/* GLSL shaders available */
    BOOL                lc_FBOSupported;    /* FBO extension available */
    
    /* Shader uniform locations */
    LONG                lc_UniTexture;      /* u_texture sampler */
    LONG                lc_UniAlpha;        /* u_alpha global alpha */
    
    /* Shadow parameters */
    WORD                lc_ShadowOffsetX;
    WORD                lc_ShadowOffsetY;
    UWORD               lc_ShadowBlur;
    UBYTE               lc_ShadowAlpha;
    BOOL                lc_ShadowEnabled;
    
    /* GL extension function pointers */
    APTR                lc_glBindFramebuffer;
    APTR                lc_glCreateShader;
    APTR                lc_glShaderSource;
    APTR                lc_glCompileShader;
    APTR                lc_glCreateProgram;
    APTR                lc_glAttachShader;
    APTR                lc_glLinkProgram;
    APTR                lc_glUseProgram;
    APTR                lc_glGetUniformLocation;
    APTR                lc_glUniform1i;
    APTR                lc_glUniform1f;
    APTR                lc_glDeleteShader;
    APTR                lc_glDeleteProgram;
    APTR                lc_glGetShaderiv;
    APTR                lc_glGetProgramiv;
};

/*
 * INTERNAL API - These are the actual implementations called by library wrappers
 */

/* Create a compositor for a screen (hybrid mode - no hook installation) */
struct LayerCompositor *CreateLayerCompositorInternal(struct Screen *screen);

/* Create a compositor with shared GL context */
struct LayerCompositor *CreateLayerCompositorSharedInternal(struct Screen *screen,
                                                             APTR masterGLContext);

/* Destroy a compositor */
void DestroyLayerCompositorInternal(struct LayerCompositor *comp);

/* Activate/deactivate the compositor */
BOOL ActivateLayerCompositorInternal(struct LayerCompositor *comp);
void DeactivateLayerCompositorInternal(struct LayerCompositor *comp);

/*
 * WINDOW REGISTRATION
 * Only zunegfx windows with alpha should be registered.
 */

/* Register a zunegfx window for alpha compositing */
struct CompositorWindow *CompositorRegisterWindowInternal(
    struct LayerCompositor *comp,
    struct Window *window,
    APTR glContext,
    struct DrawingBoard *board,
    UBYTE alpha);

/* Unregister a window */
void CompositorUnregisterWindowInternal(struct LayerCompositor *comp,
                                         struct Window *window);

/* Update window alpha value */
void CompositorSetWindowAlphaInternal(struct LayerCompositor *comp,
                                       struct Window *window,
                                       UBYTE alpha);

/* Mark a window as dirty (content changed) */
void CompositorMarkWindowDirtyInternal(struct LayerCompositor *comp,
                                        struct Window *window);

/* Find registered window */
struct CompositorWindow *CompositorFindWindowInternal(struct LayerCompositor *comp,
                                                       struct Window *window);

/*
 * COMPOSITING
 */

/* Composite all registered alpha windows over the screen */
void CompositorUpdateInternal(struct LayerCompositor *comp);

/* Force full recomposite */
void CompositorRefreshInternal(struct LayerCompositor *comp);

/* Configure shadow parameters */
void CompositorSetShadowInternal(struct LayerCompositor *comp,
                                  WORD offsetX, WORD offsetY,
                                  UWORD blur, UBYTE alpha);

/*
 * INTERNAL FUNCTIONS
 */

/* Initialize/cleanup GL resources */
BOOL CompositorInitGL(struct LayerCompositor *comp);
void CompositorCleanupGL(struct LayerCompositor *comp);

/* Load GL extensions */
BOOL CompositorLoadGLExtensions(struct LayerCompositor *comp);

/* Create/destroy shaders */
BOOL CompositorCreateShaders(struct LayerCompositor *comp);
void CompositorDestroyShaders(struct LayerCompositor *comp);

/* Get texture from zunegfx DrawingBoard */
ULONG CompositorGetWindowTexture(struct LayerCompositor *comp,
                                  struct CompositorWindow *cw);

/* Draw a single composited window */
void CompositorDrawWindow(struct LayerCompositor *comp,
                           struct CompositorWindow *cw);

/* Draw window shadow */
void CompositorDrawShadow(struct LayerCompositor *comp,
                           struct CompositorWindow *cw);

#endif /* ZUNEGFX_LAYER_COMPOSITOR_H */
