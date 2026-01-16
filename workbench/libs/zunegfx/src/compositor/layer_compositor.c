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

/* Access LayerInfo_extra */
struct LayerInfo_extra
{
    UBYTE           lie_JumpBuf[256];
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
            
        case COMP_DIRTYLAYER:
            /* Content changed - mark for re-read */
            if (msg->cm_Layer)
            {
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
    
    if (!comp->lc_LayerInfo->LayerInfo_extra)
    {
        D(bug("[LayerCompositor] No LayerInfo_extra!\n"));
        return FALSE;
    }
    
    LockLayerInfo(comp->lc_LayerInfo);
    LIE(comp->lc_LayerInfo)->lie_CompositorHook = &comp->lc_Hook;
    LIE(comp->lc_LayerInfo)->lie_CompositorData = comp;
    UnlockLayerInfo(comp->lc_LayerInfo);
    
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
 * CompositorDrawWindow - Draw a composited window with alpha
 */
void CompositorDrawWindow(struct LayerCompositor *comp,
                           struct CompositorWindow *cw)
{
    ULONG texID;
    GLfloat alpha;
    WORD x1, y1, x2, y2;
    
    if (!comp || !cw || !comp->lc_ContextValid)
        return;
    
    if (!cw->cw_Window || !cw->cw_Layer)
        return;
    
    glAMakeCurrent((GLAContext)comp->lc_GLContext);
    
    /* Bind to screen (FBO 0) */
    if (comp->lc_glBindFramebuffer)
        ((PFNGLBINDFRAMEBUFFERPROC)comp->lc_glBindFramebuffer)(GL_FRAMEBUFFER, 0);
    
    /* Setup viewport for screen */
    glViewport(0, 0, comp->lc_Width, comp->lc_Height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, comp->lc_Width, comp->lc_Height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    /* Draw shadow first (behind window) */
    CompositorDrawShadow(comp, cw);
    
    /* Get texture for window content */
    texID = CompositorGetWindowTexture(comp, cw);
    if (texID == 0)
    {
        D(bug("[LayerCompositor] No texture for window %p\n", cw->cw_Window));
        return;
    }
    
    alpha = cw->cw_Alpha / 255.0f;
    
    x1 = cw->cw_Layer->bounds.MinX;
    y1 = cw->cw_Layer->bounds.MinY;
    x2 = cw->cw_Layer->bounds.MaxX + 1;
    y2 = cw->cw_Layer->bounds.MaxY + 1;
    
    D(bug("[LayerCompositor] Drawing window %p at (%d,%d)-(%d,%d) alpha=%.2f\n",
          cw->cw_Window, x1, y1, x2, y2, alpha));
    
    /* Use shader if available */
    if (comp->lc_ShadersValid)
    {
        ((PFNGLUSEPROGRAMPROC)comp->lc_glUseProgram)(comp->lc_CompositeShader);
        
        if (comp->lc_UniTexture >= 0)
            ((PFNGLUNIFORM1IPROC)comp->lc_glUniform1i)(comp->lc_UniTexture, 0);
        if (comp->lc_UniAlpha >= 0)
            ((PFNGLUNIFORM1FPROC)comp->lc_glUniform1f)(comp->lc_UniAlpha, alpha);
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        if (comp->lc_glUseProgram)
            ((PFNGLUSEPROGRAMPROC)comp->lc_glUseProgram)(0);
        
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
    }
    
    /* Draw textured quad */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(x1, y1);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(x2, y1);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(x2, y2);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(x1, y2);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    
    /* Flush to ensure it's visible */
    glFlush();
    glASwapBuffers((GLAContext)comp->lc_GLContext);
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
