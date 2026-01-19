/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: Layer Compositor - Hardware-accelerated window compositing
    
    HYBRID COMPOSITING MODE
    =======================
    
    This compositor operates in hybrid mode:
    
    1. Standard windows (no alpha) render via normal layer system
       - Direct blitting to screen bitmap
       - No compositor involvement
       - Maximum performance and compatibility
    
    2. Alpha windows (ILAF_ALPHA flag set) are handled by compositor
       - hyperlayers calls our hook instead of normal blit
       - We read from the window's DrawingBoard FBO
       - Alpha-blend over whatever is already on screen
    
    The key insight: when _ShowPartsOfLayer is called for an alpha layer,
    the standard windows BEHIND it have already been drawn to the screen.
    We just need to blend our alpha window on top.
*/

#define DEBUG 1
#include <aros/debug.h>
#include <exec/memory.h>
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
#include <GL/glext.h>

#include "layer_compositor.h"
/* Note: We use weak linking for OpenGL_GetMasterContext instead of including
 * the header, because compositor may be statically linked into test programs
 * where the internal OpenGL backend functions are not available.
 */

/* Library bases */
extern struct Library *CyberGfxBase;
extern struct Library *GLBase;

/* Access internal layer structure - must match rom/hyperlayers/layers_intern.h */
struct IntLayer
{
    struct Layer    lay;
    struct Hook    *shapehook;
    IPTR            window;
    ULONG           intflags;
    struct RastPort rp;
    
    /* Compositor support */
    UBYTE           il_Alpha;
    UBYTE           il_AlphaFlags;
    UBYTE           il_Pad[2];
    APTR            il_CompositorData;
};

#define IL(x) ((struct IntLayer *)(x))

/* Alpha flags */
#define ILAF_ALPHA           (1 << 0)
#define ILAF_NOSHADOW        (1 << 1)
#define ILAF_COMPOSITEDIRTY  (1 << 2)

/* Access LayerInfo_extra - must match rom/hyperlayers/layers_intern.h exactly */
#include <setjmp.h>

struct LayerInfo_extra
{
    jmp_buf         lie_JumpBuf;
    struct MinList  lie_ResourceList;
    UBYTE           lie_pad[4];
    struct Hook    *lie_CompositorHook;
    APTR            lie_CompositorData;
};

#define LIE(li) ((struct LayerInfo_extra *)((li)->LayerInfo_extra))

/* GL constants */
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER                    0x8D40
#endif

/*
 * Compositor hook message - must match layers_intern.h
 */
struct CompositorMsg
{
    ULONG           cm_Method;
    struct Layer   *cm_Layer;
    struct Region  *cm_Region;
    APTR            cm_Reserved[4];
};

#define COMP_SHOWLAYER      1
#define COMP_HIDELAYER      2
#define COMP_MOVELAYER      3
#define COMP_DIRTYLAYER     4
#define COMP_CREATELAYER    5
#define COMP_DELETELAYER    6

/* Forward declarations */
void CompositorDrawLayerBorder(struct LayerCompositor *comp, struct Layer *layer);
void CompositorBlendLayerAlpha(struct LayerCompositor *comp, struct Layer *layer);

/*
 * Shader sources
 */
static const GLchar *g_compositor_vs_source =
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = gl_MultiTexCoord0.xy;\n"
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "}\n";

static const GLchar *g_compositor_fs_source =
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
    "}\n";

/*
 * CompositorHookFunc - Called by hyperlayers for alpha layers only
 *
 * When this is called, standard windows behind this layer have already
 * been drawn to the screen. We blend our alpha content on top.
 */
AROS_UFH3(void, CompositorHookFunc,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(struct Layer_Info *, li, A2),
    AROS_UFHA(struct CompositorMsg *, msg, A1))
{
    AROS_USERFUNC_INIT
    
    struct LayerCompositor *comp = (struct LayerCompositor *)hook->h_Data;
    struct CompositorWindow *cw;
    
    if (!comp || !comp->lc_Active)
        return;
    
    D(bug("[LayerCompositor] Hook: method=%ld layer=%p\n", msg->cm_Method, msg->cm_Layer));
    
    switch (msg->cm_Method)
    {
        case COMP_SHOWLAYER:
            /*
             * An alpha layer is becoming visible.
             * Find the registered window and composite it.
             */
            if (msg->cm_Layer)
            {
                struct Window *win = (struct Window *)IL(msg->cm_Layer)->window;
                if (win)
                {
                    cw = CompositorFindWindowInternal(comp, win);
                    if (cw)
                    {
                        cw->cw_Visible = TRUE;
                        CompositorDrawWindow(comp, cw);
                    }
                    else
                    {
                        D(bug("[LayerCompositor] Alpha layer %p not registered!\n", msg->cm_Layer));
                    }
                }
            }
            break;
            
        case COMP_HIDELAYER:
            /* Alpha layer hidden - just mark it */
            if (msg->cm_Layer)
            {
                struct Window *win = (struct Window *)IL(msg->cm_Layer)->window;
                if (win)
                {
                    cw = CompositorFindWindowInternal(comp, win);
                    if (cw)
                        cw->cw_Visible = FALSE;
                }
            }
            break;
            
        case COMP_MOVELAYER:
            /*
             * Layer was moved or resized. For alpha layers, we need to 
             * re-blend the content at the new position.
             */
            if (msg->cm_Layer)
            {
                bug("[LayerCompositor] COMP_MOVELAYER: Alpha-blending layer %p at new position\n", msg->cm_Layer);
                CompositorBlendLayerAlpha(comp, msg->cm_Layer);
            }
            break;
            
        case COMP_DIRTYLAYER:
            /*
             * Content changed - this is called AFTER window content has been
             * rendered to the layer. Now we alpha-blend it to the screen.
             */
            if (msg->cm_Layer)
            {
                bug("[LayerCompositor] COMP_DIRTYLAYER: Alpha-blending layer %p\n", msg->cm_Layer);
                CompositorBlendLayerAlpha(comp, msg->cm_Layer);
                
                struct Window *win = (struct Window *)IL(msg->cm_Layer)->window;
                if (win)
                {
                    cw = CompositorFindWindowInternal(comp, win);
                    if (cw)
                        cw->cw_Dirty = TRUE;
                }
            }
            break;
            
        case COMP_DELETELAYER:
            /* Layer being destroyed - unregister */
            if (msg->cm_Layer)
            {
                struct Window *win = (struct Window *)IL(msg->cm_Layer)->window;
                if (win)
                    CompositorUnregisterWindowInternal(comp, win);
            }
            break;
            
        case COMP_CREATELAYER:
            /*
             * New alpha layer created - alpha-blend it.
             * This is called from _ShowLayer when a new alpha layer is created.
             */
            if (msg->cm_Layer)
            {
                bug("[LayerCompositor] COMP_CREATELAYER: Alpha-blending new layer %p\n", msg->cm_Layer);
                CompositorBlendLayerAlpha(comp, msg->cm_Layer);
            }
            break;
    }
    
    AROS_USERFUNC_EXIT
}

/*
 * CompositorLoadGLExtensions - Load GL function pointers
 */
BOOL CompositorLoadGLExtensions(struct LayerCompositor *comp)
{
    if (!comp || !GLBase)
        return FALSE;
    
    comp->lc_glBindFramebuffer = glAGetProcAddress((const GLubyte *)"glBindFramebuffer");
    if (!comp->lc_glBindFramebuffer)
        comp->lc_glBindFramebuffer = glAGetProcAddress((const GLubyte *)"glBindFramebufferEXT");
    
    comp->lc_FBOSupported = (comp->lc_glBindFramebuffer != NULL);
    
    comp->lc_glCreateShader = glAGetProcAddress((const GLubyte *)"glCreateShader");
    comp->lc_glShaderSource = glAGetProcAddress((const GLubyte *)"glShaderSource");
    comp->lc_glCompileShader = glAGetProcAddress((const GLubyte *)"glCompileShader");
    comp->lc_glCreateProgram = glAGetProcAddress((const GLubyte *)"glCreateProgram");
    comp->lc_glAttachShader = glAGetProcAddress((const GLubyte *)"glAttachShader");
    comp->lc_glLinkProgram = glAGetProcAddress((const GLubyte *)"glLinkProgram");
    comp->lc_glUseProgram = glAGetProcAddress((const GLubyte *)"glUseProgram");
    comp->lc_glGetUniformLocation = glAGetProcAddress((const GLubyte *)"glGetUniformLocation");
    comp->lc_glUniform1i = glAGetProcAddress((const GLubyte *)"glUniform1i");
    comp->lc_glUniform1f = glAGetProcAddress((const GLubyte *)"glUniform1f");
    comp->lc_glDeleteShader = glAGetProcAddress((const GLubyte *)"glDeleteShader");
    comp->lc_glDeleteProgram = glAGetProcAddress((const GLubyte *)"glDeleteProgram");
    comp->lc_glGetShaderiv = glAGetProcAddress((const GLubyte *)"glGetShaderiv");
    comp->lc_glGetProgramiv = glAGetProcAddress((const GLubyte *)"glGetProgramiv");
    
    comp->lc_ShadersSupported = (comp->lc_glCreateShader && comp->lc_glShaderSource &&
                                  comp->lc_glCompileShader && comp->lc_glCreateProgram &&
                                  comp->lc_glAttachShader && comp->lc_glLinkProgram &&
                                  comp->lc_glUseProgram && comp->lc_glGetUniformLocation);
    
    D(bug("[LayerCompositor] FBO: %s, Shaders: %s\n",
          comp->lc_FBOSupported ? "Yes" : "No",
          comp->lc_ShadersSupported ? "Yes" : "No"));
    
    return TRUE;
}

/*
 * CompileShader - Compile a single shader
 */
static GLuint CompileShader(struct LayerCompositor *comp, GLenum type, const GLchar *source)
{
    PFNGLCREATESHADERPROC glCreateShader = (PFNGLCREATESHADERPROC)comp->lc_glCreateShader;
    PFNGLSHADERSOURCEPROC glShaderSource = (PFNGLSHADERSOURCEPROC)comp->lc_glShaderSource;
    PFNGLCOMPILESHADERPROC glCompileShader = (PFNGLCOMPILESHADERPROC)comp->lc_glCompileShader;
    PFNGLGETSHADERIVPROC glGetShaderiv = (PFNGLGETSHADERIVPROC)comp->lc_glGetShaderiv;
    
    GLuint shader;
    GLint status;
    
    shader = glCreateShader(type);
    if (shader == 0)
        return 0;
    
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    if (glGetShaderiv)
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status)
        {
            D(bug("[LayerCompositor] Shader compile failed\n"));
            ((PFNGLDELETESHADERPROC)comp->lc_glDeleteShader)(shader);
            return 0;
        }
    }
    
    return shader;
}

/*
 * CompositorCreateShaders - Create the compositing shader program
 */
BOOL CompositorCreateShaders(struct LayerCompositor *comp)
{
    PFNGLCREATEPROGRAMPROC glCreateProgram = (PFNGLCREATEPROGRAMPROC)comp->lc_glCreateProgram;
    PFNGLATTACHSHADERPROC glAttachShader = (PFNGLATTACHSHADERPROC)comp->lc_glAttachShader;
    PFNGLLINKPROGRAMPROC glLinkProgram = (PFNGLLINKPROGRAMPROC)comp->lc_glLinkProgram;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)comp->lc_glGetUniformLocation;
    
    GLuint vs, fs;
    
    if (!comp->lc_ShadersSupported)
        return FALSE;
    
    vs = CompileShader(comp, GL_VERTEX_SHADER, g_compositor_vs_source);
    if (!vs)
        return FALSE;
    
    fs = CompileShader(comp, GL_FRAGMENT_SHADER, g_compositor_fs_source);
    if (!fs)
    {
        ((PFNGLDELETESHADERPROC)comp->lc_glDeleteShader)(vs);
        return FALSE;
    }
    
    comp->lc_CompositeShader = glCreateProgram();
    glAttachShader(comp->lc_CompositeShader, vs);
    glAttachShader(comp->lc_CompositeShader, fs);
    glLinkProgram(comp->lc_CompositeShader);
    
    comp->lc_UniTexture = glGetUniformLocation(comp->lc_CompositeShader, "u_texture");
    comp->lc_UniAlpha = glGetUniformLocation(comp->lc_CompositeShader, "u_alpha");
    
    ((PFNGLDELETESHADERPROC)comp->lc_glDeleteShader)(vs);
    ((PFNGLDELETESHADERPROC)comp->lc_glDeleteShader)(fs);
    
    comp->lc_ShadersValid = TRUE;
    
    D(bug("[LayerCompositor] Shaders created\n"));
    
    return TRUE;
}

/*
 * CompositorDestroyShaders
 */
void CompositorDestroyShaders(struct LayerCompositor *comp)
{
    if (!comp)
        return;
    
    if (comp->lc_glUseProgram)
        ((PFNGLUSEPROGRAMPROC)comp->lc_glUseProgram)(0);
    
    if (comp->lc_CompositeShader && comp->lc_glDeleteProgram)
    {
        ((PFNGLDELETEPROGRAMPROC)comp->lc_glDeleteProgram)(comp->lc_CompositeShader);
        comp->lc_CompositeShader = 0;
    }
    
    comp->lc_ShadersValid = FALSE;
}

/*
 * CompositorInitGL - Initialize OpenGL context
 */
BOOL CompositorInitGL(struct LayerCompositor *comp)
{
    struct TagItem tags[10];
    int idx = 0;
    
    if (!comp || !comp->lc_Screen || !GLBase)
        return FALSE;
    
    tags[idx].ti_Tag = GLA_Screen;
    tags[idx++].ti_Data = (IPTR)comp->lc_Screen;
    
    tags[idx].ti_Tag = GLA_Left;
    tags[idx++].ti_Data = 0;
    
    tags[idx].ti_Tag = GLA_Top;
    tags[idx++].ti_Data = 0;
    
    tags[idx].ti_Tag = GLA_Width;
    tags[idx++].ti_Data = comp->lc_Width;
    
    tags[idx].ti_Tag = GLA_Height;
    tags[idx++].ti_Data = comp->lc_Height;
    
    tags[idx].ti_Tag = GLA_NoDepth;
    tags[idx++].ti_Data = TRUE;
    
    tags[idx].ti_Tag = GLA_NoStencil;
    tags[idx++].ti_Data = TRUE;
    
    if (comp->lc_MasterContext)
    {
        tags[idx].ti_Tag = GLA_ShareContext;
        tags[idx++].ti_Data = (IPTR)comp->lc_MasterContext;
    }
    
    tags[idx].ti_Tag = TAG_DONE;
    
    comp->lc_GLContext = glACreateContext(tags);
    if (!comp->lc_GLContext)
    {
        D(bug("[LayerCompositor] Failed to create GL context\n"));
        return FALSE;
    }
    
    comp->lc_OwnsGLContext = TRUE;
    
    glAMakeCurrent((GLAContext)comp->lc_GLContext);
    
    CompositorLoadGLExtensions(comp);
    
    if (comp->lc_ShadersSupported)
        CompositorCreateShaders(comp);
    
    /* Setup GL state for 2D compositing */
    glViewport(0, 0, comp->lc_Width, comp->lc_Height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, comp->lc_Width, comp->lc_Height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    
    comp->lc_ContextValid = TRUE;
    
    D(bug("[LayerCompositor] GL initialized\n"));
    
    return TRUE;
}

/*
 * CompositorCleanupGL
 */
void CompositorCleanupGL(struct LayerCompositor *comp)
{
    if (!comp)
        return;
    
    if (comp->lc_GLContext)
    {
        glAMakeCurrent((GLAContext)comp->lc_GLContext);
        CompositorDestroyShaders(comp);
        
        if (comp->lc_OwnsGLContext)
            glADestroyContext((GLAContext)comp->lc_GLContext);
        
        comp->lc_GLContext = NULL;
    }
    
    comp->lc_ContextValid = FALSE;
}

/*
 * CreateLayerCompositorInternal
 *
 * Automatically gets the zunegfx master GL context for shared context support.
 * This ensures the compositor shares OpenGL resources (textures, FBOs) with
 * zunegfx windows, enabling proper compositing without pipe_screen conflicts.
 */
struct LayerCompositor *CreateLayerCompositorInternal(struct Screen *screen)
{
    APTR masterContext = NULL;
    
    /*
     * Try to get zunegfx's master GL context for sharing.
     * This is critical for first-run scenarios where Mesa hasn't cached
     * the pipe_screen yet - without sharing, the compositor would create
     * a separate pipe_screen that can't access zunegfx FBO content.
     *
     * Note: When the compositor is statically linked into test programs,
     * OpenGL_GetMasterContext may not be available. In that case, we fall
     * back to creating an unshared context (which works on second run due
     * to Mesa caching the pipe_screen).
     *
     * For proper integration, applications should use CreateLayerCompositorShared()
     * and pass a master context obtained from zunegfx library's public API.
     */
    bug("[LayerCompositor] CreateLayerCompositorInternal: masterContext = NULL (use CreateLayerCompositorShared for sharing)\n");
    
    return CreateLayerCompositorSharedInternal(screen, masterContext);
}

/*
 * CreateLayerCompositorSharedInternal
 */
struct LayerCompositor *CreateLayerCompositorSharedInternal(struct Screen *screen,
                                                             APTR masterGLContext)
{
    struct LayerCompositor *comp;
    
    if (!screen)
        return NULL;
    
    comp = AllocVec(sizeof(struct LayerCompositor), MEMF_CLEAR | MEMF_PUBLIC);
    if (!comp)
        return NULL;
    
    comp->lc_Screen = screen;
    comp->lc_LayerInfo = &screen->LayerInfo;
    comp->lc_MasterContext = masterGLContext;
    comp->lc_Width = screen->Width;
    comp->lc_Height = screen->Height;
    
    NEWLIST(&comp->lc_Windows);
    
    comp->lc_DamageRegion = NewRegion();
    if (!comp->lc_DamageRegion)
    {
        FreeVec(comp);
        return NULL;
    }
    
    /* Setup hook */
    comp->lc_Hook.h_Entry = (HOOKFUNC)CompositorHookFunc;
    comp->lc_Hook.h_Data = comp;
    
    /* Default shadow */
    comp->lc_ShadowOffsetX = 6;
    comp->lc_ShadowOffsetY = 6;
    comp->lc_ShadowBlur = 8;
    comp->lc_ShadowAlpha = 128;
    comp->lc_ShadowEnabled = TRUE;
    
    /* Initialize GL */
    if (GLBase)
        CompositorInitGL(comp);
    
    D(bug("[LayerCompositor] Created for screen %p (%dx%d)\n",
          screen, screen->Width, screen->Height));
    
    return comp;
}

/*
 * DestroyLayerCompositorInternal
 */
void DestroyLayerCompositorInternal(struct LayerCompositor *comp)
{
    struct CompositorWindow *cw, *next;
    
    if (!comp)
        return;
    
    DeactivateLayerCompositorInternal(comp);
    
    /* Free all registered windows */
    ForeachNodeSafe(&comp->lc_Windows, cw, next)
    {
        Remove((struct Node *)cw);
        
        if (cw->cw_TextureID && comp->lc_ContextValid)
        {
            glAMakeCurrent((GLAContext)comp->lc_GLContext);
            glDeleteTextures(1, &cw->cw_TextureID);
        }
        
        FreeVec(cw);
    }
    
    if (comp->lc_DamageRegion)
        DisposeRegion(comp->lc_DamageRegion);
    
    CompositorCleanupGL(comp);
    
    FreeVec(comp);
    
    D(bug("[LayerCompositor] Destroyed\n"));
}

/*
 * ActivateLayerCompositorInternal - Install hook in LayerInfo
 */
BOOL ActivateLayerCompositorInternal(struct LayerCompositor *comp)
{
    if (!comp || !comp->lc_LayerInfo)
        return FALSE;
    
    if (comp->lc_Active)
        return TRUE;
    
    bug("[LayerCompositor] Activating: screen=%p LayerInfo=%p\n", comp->lc_Screen, comp->lc_LayerInfo);
    bug("[LayerCompositor] LayerInfo_extra=%p\n", comp->lc_LayerInfo->LayerInfo_extra);
    
    if (!comp->lc_LayerInfo->LayerInfo_extra)
    {
        D(bug("[LayerCompositor] No LayerInfo_extra!\n"));
        return FALSE;
    }
    
    LockLayerInfo(comp->lc_LayerInfo);
    LIE(comp->lc_LayerInfo)->lie_CompositorHook = &comp->lc_Hook;
    LIE(comp->lc_LayerInfo)->lie_CompositorData = comp;
    UnlockLayerInfo(comp->lc_LayerInfo);
    
    bug("[LayerCompositor] Hook installed at %p, LayerInfo_extra=%p\n", 
        &comp->lc_Hook, comp->lc_LayerInfo->LayerInfo_extra);
    
    comp->lc_Active = TRUE;
    
    D(bug("[LayerCompositor] Activated\n"));
    
    return TRUE;
}

/*
 * DeactivateLayerCompositorInternal
 */
void DeactivateLayerCompositorInternal(struct LayerCompositor *comp)
{
    if (!comp || !comp->lc_Active)
        return;
    
    if (comp->lc_LayerInfo && comp->lc_LayerInfo->LayerInfo_extra)
    {
        LockLayerInfo(comp->lc_LayerInfo);
        LIE(comp->lc_LayerInfo)->lie_CompositorHook = NULL;
        LIE(comp->lc_LayerInfo)->lie_CompositorData = NULL;
        UnlockLayerInfo(comp->lc_LayerInfo);
    }
    
    comp->lc_Active = FALSE;
    
    D(bug("[LayerCompositor] Deactivated\n"));
}

/*
 * CompositorFindWindowInternal
 */
struct CompositorWindow *CompositorFindWindowInternal(struct LayerCompositor *comp,
                                                       struct Window *window)
{
    struct CompositorWindow *cw;
    
    if (!comp || !window)
        return NULL;
    
    ForeachNode(&comp->lc_Windows, cw)
    {
        if (cw->cw_Window == window)
            return cw;
    }
    
    return NULL;
}

/*
 * CompositorRegisterWindowInternal - Register a zunegfx window for compositing
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
    cw = CompositorFindWindowInternal(comp, window);
    if (cw)
    {
        /* Update existing registration */
        cw->cw_GLContext = glContext;
        cw->cw_DrawingBoard = board;
        cw->cw_Alpha = alpha;
        cw->cw_Dirty = TRUE;
        return cw;
    }
    
    cw = AllocVec(sizeof(struct CompositorWindow), MEMF_CLEAR | MEMF_PUBLIC);
    if (!cw)
        return NULL;
    
    cw->cw_Window = window;
    cw->cw_Layer = window->WLayer;
    cw->cw_GLContext = glContext;
    cw->cw_DrawingBoard = board;
    cw->cw_Alpha = alpha;
    cw->cw_Valid = TRUE;
    cw->cw_Dirty = TRUE;
    cw->cw_Visible = TRUE;
    cw->cw_Width = window->Width - window->BorderLeft - window->BorderRight;
    cw->cw_Height = window->Height - window->BorderTop - window->BorderBottom;
    
    AddTail((struct List *)&comp->lc_Windows, (struct Node *)cw);
    comp->lc_WindowCount++;
    
    D(bug("[LayerCompositor] Registered window %p (alpha=%d)\n", window, alpha));
    
    return cw;
}

/*
 * CompositorUnregisterWindowInternal
 */
void CompositorUnregisterWindowInternal(struct LayerCompositor *comp,
                                         struct Window *window)
{
    struct CompositorWindow *cw;
    
    if (!comp || !window)
        return;
    
    cw = CompositorFindWindowInternal(comp, window);
    if (!cw)
        return;
    
    Remove((struct Node *)cw);
    comp->lc_WindowCount--;
    
    if (cw->cw_TextureID && comp->lc_ContextValid)
    {
        glAMakeCurrent((GLAContext)comp->lc_GLContext);
        glDeleteTextures(1, &cw->cw_TextureID);
    }
    
    FreeVec(cw);
    
    D(bug("[LayerCompositor] Unregistered window %p\n", window));
}

/*
 * CompositorSetWindowAlphaInternal
 */
void CompositorSetWindowAlphaInternal(struct LayerCompositor *comp,
                                       struct Window *window,
                                       UBYTE alpha)
{
    struct CompositorWindow *cw = CompositorFindWindowInternal(comp, window);
    if (cw)
        cw->cw_Alpha = alpha;
}

/*
 * CompositorMarkWindowDirtyInternal
 */
void CompositorMarkWindowDirtyInternal(struct LayerCompositor *comp,
                                        struct Window *window)
{
    struct CompositorWindow *cw = CompositorFindWindowInternal(comp, window);
    if (cw)
        cw->cw_Dirty = TRUE;
}

/*
 * CompositorGetWindowTexture - Get/create texture for window content
 *
 * For zunegfx windows with FBO, we try zero-copy first (use FBO texture directly).
 * Fallback: read pixels from DrawingBoard bitmap and upload to texture.
 */
ULONG CompositorGetWindowTexture(struct LayerCompositor *comp,
                                  struct CompositorWindow *cw)
{
    if (!comp || !cw || !comp->lc_ContextValid)
        return 0;
    
    /* If texture exists and not dirty, return it */
    if (cw->cw_TextureID && !cw->cw_Dirty)
        return cw->cw_TextureID;
    
    glAMakeCurrent((GLAContext)comp->lc_GLContext);
    
    /*
     * Try zero-copy path: get pipe_resource from zunegfx context
     * TODO: This requires glACreateTextureFromResource() which doesn't exist yet
     */
    if (cw->cw_GLContext && GLBase)
    {
        APTR resource = glAGetRenderResource((GLAContext)cw->cw_GLContext);
        if (resource)
        {
            cw->cw_PipeResource = resource;
            /* For now, fall through to bitmap upload until we have texture wrapping */
        }
    }
    
    /*
     * Fallback: Read from DrawingBoard bitmap and upload to texture
     */
    if (cw->cw_DrawingBoard)
    {
        struct DrawingBoard *board = cw->cw_DrawingBoard;
        UBYTE *pixels;
        UWORD width = cw->cw_Width;
        UWORD height = cw->cw_Height;
        
        /* The DrawingBoard should have a bitmap with the rendered content */
        /* We need to access board->bitmap - this requires knowing the structure */
        /* For now, use ReadPixelArray from the window's RastPort */
        
        if (cw->cw_Window && cw->cw_Window->RPort && CyberGfxBase)
        {
            pixels = AllocVec(width * height * 4, MEMF_ANY);
            if (pixels)
            {
                ReadPixelArray(pixels, 0, 0, width * 4,
                               cw->cw_Window->RPort,
                               cw->cw_Window->BorderLeft,
                               cw->cw_Window->BorderTop,
                               width, height, RECTFMT_RGBA);
                
                if (cw->cw_TextureID == 0)
                    glGenTextures(1, &cw->cw_TextureID);
                
                glBindTexture(GL_TEXTURE_2D, cw->cw_TextureID);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                
                FreeVec(pixels);
                cw->cw_Dirty = FALSE;
            }
        }
    }
    
    return cw->cw_TextureID;
}

/*
 * CompositorDrawShadow - Draw drop shadow behind window
 */
void CompositorDrawShadow(struct LayerCompositor *comp,
                           struct CompositorWindow *cw)
{
    WORD x, y, w, h;
    GLfloat alpha;
    
    if (!comp || !cw || !comp->lc_ShadowEnabled)
        return;
    
    if (!cw->cw_Window || !cw->cw_Layer)
        return;
    
    x = cw->cw_Layer->bounds.MinX + comp->lc_ShadowOffsetX;
    y = cw->cw_Layer->bounds.MinY + comp->lc_ShadowOffsetY;
    w = cw->cw_Layer->bounds.MaxX - cw->cw_Layer->bounds.MinX + 1;
    h = cw->cw_Layer->bounds.MaxY - cw->cw_Layer->bounds.MinY + 1;
    
    alpha = comp->lc_ShadowAlpha / 255.0f;
    
    /* Simple shadow - just a dark rectangle for now */
    if (comp->lc_glUseProgram)
        ((PFNGLUSEPROGRAMPROC)comp->lc_glUseProgram)(0);
    
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glColor4f(0.0f, 0.0f, 0.0f, alpha * 0.5f);
    
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

/*
 * CompositorBlendLayerAlpha - Alpha-blend a layer's content to the screen using OpenGL
 *
 * This uses OpenGL for hardware-accelerated alpha blending:
 * 1. Read window content from RastPort into a texture
 * 2. Draw a textured quad with alpha blending enabled
 * 3. The result is composited directly to the screen via GL context
 *
 * Only blends within the layer's VisibleRegion to respect z-order.
 * The alpha value comes from the layer's il_Alpha field (0-255).
 */
void CompositorBlendLayerAlpha(struct LayerCompositor *comp, struct Layer *layer)
{
    struct Window *win;
    struct RegionRectangle *rr;
    UBYTE *pixel_buffer = NULL;
    UBYTE alpha;
    GLuint texture = 0;
    
    if (!comp || !layer || !CyberGfxBase)
        return;
    
    /* Check if layer has any visible area */
    if (!layer->VisibleRegion || !layer->VisibleRegion->RegionRectangle)
    {
        bug("[LayerCompositor] BlendAlpha: Layer %p has no visible region\n", layer);
        return;
    }
    
    /* Get the window from the layer */
    win = (struct Window *)IL(layer)->window;
    if (!win || !win->RPort || !win->RPort->BitMap)
    {
        bug("[LayerCompositor] BlendAlpha: Layer %p has no valid window\n", layer);
        return;
    }
    
    /* Get alpha value from layer (default to semi-transparent if not set) */
    alpha = IL(layer)->il_Alpha;
    if (alpha == 0)
        alpha = 200;  /* Default semi-transparent */
    
    bug("[LayerCompositor] BlendAlpha: Layer %p, Window %p, Alpha=%d\n", 
        layer, win, alpha);
    bug("[LayerCompositor] BlendAlpha: Layer bounds (%d,%d)-(%d,%d)\n",
        layer->bounds.MinX, layer->bounds.MinY,
        layer->bounds.MaxX, layer->bounds.MaxY);
    
    /* Check if we have a valid GL context */
    if (!comp->lc_ContextValid || !comp->lc_GLContext)
    {
        bug("[LayerCompositor] BlendAlpha: No valid GL context, falling back to software\n");
        /* Fallback to software blending */
        goto software_fallback;
    }
    
    /* Make compositor's GL context current */
    glAMakeCurrent((GLAContext)comp->lc_GLContext);
    
    /* Setup GL state for alpha blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    /* Iterate through all visible rectangles and blend each one */
    rr = layer->VisibleRegion->RegionRectangle;
    while (rr)
    {
        /* RegionRectangle bounds are relative to Region bounds */
        WORD vx1 = layer->VisibleRegion->bounds.MinX + rr->bounds.MinX;
        WORD vy1 = layer->VisibleRegion->bounds.MinY + rr->bounds.MinY;
        WORD vx2 = layer->VisibleRegion->bounds.MinX + rr->bounds.MaxX;
        WORD vy2 = layer->VisibleRegion->bounds.MinY + rr->bounds.MaxY;
        WORD width = vx2 - vx1 + 1;
        WORD height = vy2 - vy1 + 1;
        
        /* Source coordinates in window's content area */
        WORD src_x = vx1 - layer->bounds.MinX;
        WORD src_y = vy1 - layer->bounds.MinY;
        GLfloat alpha_f = alpha / 255.0f;
        
        bug("[LayerCompositor] BlendAlpha: Visible rect (%d,%d)-(%d,%d), src=(%d,%d)\n",
            vx1, vy1, vx2, vy2, src_x, src_y);
        
        /* Allocate buffer for this rectangle */
        pixel_buffer = AllocVec(width * height * 4, MEMF_ANY);
        if (pixel_buffer)
        {
            /* Read pixels from window's RastPort (RGBA format for OpenGL) */
            ReadPixelArray(pixel_buffer, 0, 0, width * 4,
                          win->RPort, src_x, src_y,
                          width, height, RECTFMT_RGBA);
            
            /* Create texture from pixel data */
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer);
            
            /* Draw textured quad with alpha */
            glColor4f(1.0f, 1.0f, 1.0f, alpha_f);
            
            glBegin(GL_QUADS);
                /* Bottom-left */
                glTexCoord2f(0.0f, 1.0f);
                glVertex2i(vx1, vy2 + 1);
                /* Bottom-right */
                glTexCoord2f(1.0f, 1.0f);
                glVertex2i(vx2 + 1, vy2 + 1);
                /* Top-right */
                glTexCoord2f(1.0f, 0.0f);
                glVertex2i(vx2 + 1, vy1);
                /* Top-left */
                glTexCoord2f(0.0f, 0.0f);
                glVertex2i(vx1, vy1);
            glEnd();
            
            /* Cleanup texture */
            glDeleteTextures(1, &texture);
            texture = 0;
            
            FreeVec(pixel_buffer);
            pixel_buffer = NULL;
        }
        
        rr = rr->Next;
    }
    
    /* Flush GL commands */
    glFlush();
    
    /* Swap buffers to make visible (if double-buffered) */
    glASwapBuffers((GLAContext)comp->lc_GLContext);
    
    return;

software_fallback:
    /* Software fallback using WritePixelArrayAlpha */
    rr = layer->VisibleRegion->RegionRectangle;
    while (rr)
    {
        WORD vx1 = layer->VisibleRegion->bounds.MinX + rr->bounds.MinX;
        WORD vy1 = layer->VisibleRegion->bounds.MinY + rr->bounds.MinY;
        WORD vx2 = layer->VisibleRegion->bounds.MinX + rr->bounds.MaxX;
        WORD vy2 = layer->VisibleRegion->bounds.MinY + rr->bounds.MaxY;
        WORD width = vx2 - vx1 + 1;
        WORD height = vy2 - vy1 + 1;
        WORD src_x = vx1 - layer->bounds.MinX;
        WORD src_y = vy1 - layer->bounds.MinY;
        
        pixel_buffer = AllocVec(width * height * 4, MEMF_ANY);
        if (pixel_buffer)
        {
            LONG i;
            
            ReadPixelArray(pixel_buffer, 0, 0, width * 4,
                          win->RPort, src_x, src_y,
                          width, height, RECTFMT_ARGB);
            
            for (i = 0; i < width * height; i++)
            {
                ULONG pixel_alpha = pixel_buffer[i * 4];
                pixel_buffer[i * 4] = (pixel_alpha * alpha) / 255;
            }
            
            WritePixelArrayAlpha(pixel_buffer, 0, 0, width * 4,
                                &comp->lc_Screen->RastPort,
                                vx1, vy1, width, height, 0xFFFFFFFF);
            
            FreeVec(pixel_buffer);
        }
        
        rr = rr->Next;
    }
}

/*
 * CompositorDrawLayerBorder - Draw a red border inside a layer
 *
 * This is used to visually verify that the compositor hook is being called.
 * Takes a Layer pointer directly (doesn't need a registered CompositorWindow).
 * Border is drawn INSIDE the layer bounds so it moves with the window.
 * 
 * IMPORTANT: Only draws within the layer's VisibleRegion to respect z-order.
 * If another window is in front, those areas won't be drawn over.
 */
void CompositorDrawLayerBorder(struct LayerCompositor *comp, struct Layer *layer)
{
    WORD x1, y1, x2, y2;
    WORD bw = 4;  /* Border width */
    struct RegionRectangle *rr;
    
    if (!comp || !layer)
        return;
    
    /* Check if layer has any visible area */
    if (!layer->VisibleRegion || !layer->VisibleRegion->RegionRectangle)
    {
        bug("[LayerCompositor] Layer %p has no visible region - skipping border\n", layer);
        return;
    }
    
    x1 = layer->bounds.MinX;
    y1 = layer->bounds.MinY;
    x2 = layer->bounds.MaxX;
    y2 = layer->bounds.MaxY;
    
    bug("[LayerCompositor] Drawing RED BORDER inside layer %p at (%d,%d)-(%d,%d)\n",
          layer, x1, y1, x2, y2);
    bug("[LayerCompositor] VisibleRegion bounds: (%d,%d)-(%d,%d)\n",
          layer->VisibleRegion->bounds.MinX, layer->VisibleRegion->bounds.MinY,
          layer->VisibleRegion->bounds.MaxX, layer->VisibleRegion->bounds.MaxY);
    
    /*
     * Draw a bright red border INSIDE the layer bounds using CyberGraphics.
     * Only draw within each visible rectangle to respect z-order.
     */
    if (CyberGfxBase && comp->lc_Screen)
    {
        struct RastPort rp;
        ULONG red = 0x00FF0000;  /* ARGB red */
        
        InitRastPort(&rp);
        rp.BitMap = comp->lc_Screen->RastPort.BitMap;
        
        /* Iterate through all visible rectangles */
        rr = layer->VisibleRegion->RegionRectangle;
        while (rr)
        {
            /* RegionRectangle bounds are relative to Region bounds */
            WORD vx1 = layer->VisibleRegion->bounds.MinX + rr->bounds.MinX;
            WORD vy1 = layer->VisibleRegion->bounds.MinY + rr->bounds.MinY;
            WORD vx2 = layer->VisibleRegion->bounds.MinX + rr->bounds.MaxX;
            WORD vy2 = layer->VisibleRegion->bounds.MinY + rr->bounds.MaxY;
            
            bug("[LayerCompositor]   Visible rect: (%d,%d)-(%d,%d)\n", vx1, vy1, vx2, vy2);
            
            /* Draw border parts that intersect with this visible rect */
            
            /* Top border - clip to visible rect */
            if (vy1 <= y1 + bw - 1 && vy2 >= y1)
            {
                WORD ty1 = (y1 > vy1) ? y1 : vy1;
                WORD ty2 = (y1 + bw - 1 < vy2) ? y1 + bw - 1 : vy2;
                WORD tx1 = (x1 > vx1) ? x1 : vx1;
                WORD tx2 = (x2 < vx2) ? x2 : vx2;
                if (tx1 <= tx2 && ty1 <= ty2)
                    FillPixelArray(&rp, tx1, ty1, tx2 - tx1 + 1, ty2 - ty1 + 1, red);
            }
            
            /* Bottom border - clip to visible rect */
            if (vy2 >= y2 - bw + 1 && vy1 <= y2)
            {
                WORD ty1 = (y2 - bw + 1 > vy1) ? y2 - bw + 1 : vy1;
                WORD ty2 = (y2 < vy2) ? y2 : vy2;
                WORD tx1 = (x1 > vx1) ? x1 : vx1;
                WORD tx2 = (x2 < vx2) ? x2 : vx2;
                if (tx1 <= tx2 && ty1 <= ty2)
                    FillPixelArray(&rp, tx1, ty1, tx2 - tx1 + 1, ty2 - ty1 + 1, red);
            }
            
            /* Left border - clip to visible rect (exclude corners already drawn) */
            if (vx1 <= x1 + bw - 1 && vx2 >= x1)
            {
                WORD ty1 = (y1 + bw > vy1) ? y1 + bw : vy1;
                WORD ty2 = (y2 - bw < vy2) ? y2 - bw : vy2;
                WORD tx1 = (x1 > vx1) ? x1 : vx1;
                WORD tx2 = (x1 + bw - 1 < vx2) ? x1 + bw - 1 : vx2;
                if (tx1 <= tx2 && ty1 <= ty2)
                    FillPixelArray(&rp, tx1, ty1, tx2 - tx1 + 1, ty2 - ty1 + 1, red);
            }
            
            /* Right border - clip to visible rect (exclude corners already drawn) */
            if (vx2 >= x2 - bw + 1 && vx1 <= x2)
            {
                WORD ty1 = (y1 + bw > vy1) ? y1 + bw : vy1;
                WORD ty2 = (y2 - bw < vy2) ? y2 - bw : vy2;
                WORD tx1 = (x2 - bw + 1 > vx1) ? x2 - bw + 1 : vx1;
                WORD tx2 = (x2 < vx2) ? x2 : vx2;
                if (tx1 <= tx2 && ty1 <= ty2)
                    FillPixelArray(&rp, tx1, ty1, tx2 - tx1 + 1, ty2 - ty1 + 1, red);
            }
            
            rr = rr->Next;
        }
    }
}

/*
 * CompositorDrawWindow - Draw a composited window with alpha
 *
 * For now, just draws a bright red border around the window to verify
 * that the compositor hook is being called correctly.
 */
void CompositorDrawWindow(struct LayerCompositor *comp,
                           struct CompositorWindow *cw)
{
    WORD x1, y1, x2, y2;
    WORD bw = 4;  /* Border width */
    
    if (!comp || !cw)
        return;
    
    if (!cw->cw_Window || !cw->cw_Layer)
        return;
    
    x1 = cw->cw_Layer->bounds.MinX;
    y1 = cw->cw_Layer->bounds.MinY;
    x2 = cw->cw_Layer->bounds.MaxX;
    y2 = cw->cw_Layer->bounds.MaxY;
    
    D(bug("[LayerCompositor] Drawing RED BORDER around window %p at (%d,%d)-(%d,%d)\n",
          cw->cw_Window, x1, y1, x2, y2));
    
    /*
     * Draw a bright red border using CyberGraphics directly to the screen bitmap.
     * This bypasses OpenGL entirely to verify the hook is being called.
     */
    if (CyberGfxBase && comp->lc_Screen)
    {
        struct RastPort rp;
        ULONG red = 0x00FF0000;  /* ARGB red */
        
        InitRastPort(&rp);
        rp.BitMap = comp->lc_Screen->RastPort.BitMap;
        
        /* Top border */
        FillPixelArray(&rp, x1, y1, x2 - x1 + 1, bw, red);
        
        /* Bottom border */
        FillPixelArray(&rp, x1, y2 - bw + 1, x2 - x1 + 1, bw, red);
        
        /* Left border */
        FillPixelArray(&rp, x1, y1, bw, y2 - y1 + 1, red);
        
        /* Right border */
        FillPixelArray(&rp, x2 - bw + 1, y1, bw, y2 - y1 + 1, red);
    }
}

/*
 * CompositorUpdateInternal - Update all visible alpha windows
 */
void CompositorUpdateInternal(struct LayerCompositor *comp)
{
    struct CompositorWindow *cw;
    
    if (!comp || !comp->lc_Active)
        return;
    
    ForeachNode(&comp->lc_Windows, cw)
    {
        if (cw->cw_Visible && cw->cw_Valid)
            CompositorDrawWindow(comp, cw);
    }
}

/*
 * CompositorRefreshInternal - Force full refresh
 */
void CompositorRefreshInternal(struct LayerCompositor *comp)
{
    struct CompositorWindow *cw;
    
    if (!comp)
        return;
    
    ForeachNode(&comp->lc_Windows, cw)
    {
        cw->cw_Dirty = TRUE;
    }
    
    CompositorUpdateInternal(comp);
}

/*
 * CompositorSetShadowInternal
 */
void CompositorSetShadowInternal(struct LayerCompositor *comp,
                                  WORD offsetX, WORD offsetY,
                                  UWORD blur, UBYTE alpha)
{
    if (!comp)
        return;
    
    comp->lc_ShadowOffsetX = offsetX;
    comp->lc_ShadowOffsetY = offsetY;
    comp->lc_ShadowBlur = blur;
    comp->lc_ShadowAlpha = alpha;
}
