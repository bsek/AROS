#ifndef _GLCOMPOSITOR_INTERN_H
#define _GLCOMPOSITOR_INTERN_H

/*
    Copyright (C) 2010-2025, The AROS Development Team. All rights reserved.

    Desc: OpenGL GPU Compositor - Internal structures
*/

#include "compositor.h"

#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/semaphores.h>
#include <graphics/gfx.h>

#include <GL/gl.h>
#include <GL/gla.h>
#include <hidd/gallium.h>

/* ────────────────────────────────────────────────────────────────────────── */
/* GL Extension Function Pointer Types                                       */
/* ────────────────────────────────────────────────────────────────────────── */

typedef GLuint (*PFNGLCREATESHADERPROC_)(GLenum type);
typedef void   (*PFNGLSHADERSOURCEPROC_)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void   (*PFNGLCOMPILESHADERPROC_)(GLuint shader);
typedef GLuint (*PFNGLCREATEPROGRAMPROC_)(void);
typedef void   (*PFNGLATTACHSHADERPROC_)(GLuint program, GLuint shader);
typedef void   (*PFNGLLINKPROGRAMPROC_)(GLuint program);
typedef void   (*PFNGLUSEPROGRAMPROC_)(GLuint program);
typedef GLint  (*PFNGLGETUNIFORMLOCATIONPROC_)(GLuint program, const GLchar *name);
typedef void   (*PFNGLUNIFORM1IPROC_)(GLint location, GLint v0);
typedef void   (*PFNGLUNIFORM1FPROC_)(GLint location, GLfloat v0);
typedef void   (*PFNGLUNIFORM2FPROC_)(GLint location, GLfloat v0, GLfloat v1);
typedef void   (*PFNGLUNIFORM4FPROC_)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void   (*PFNGLDELETESHADERPROC_)(GLuint shader);
typedef void   (*PFNGLDELETEPROGRAMPROC_)(GLuint program);
typedef void   (*PFNGLGETSHADERIVPROC_)(GLuint shader, GLenum pname, GLint *params);
typedef void   (*PFNGLGETPROGRAMIVPROC_)(GLuint program, GLenum pname, GLint *params);
typedef void   (*PFNGLBINDFRAMEBUFFERPROC_)(GLenum target, GLuint framebuffer);
typedef void   (*PFNGLGENBUFFERSPROC_)(GLsizei n, GLuint *buffers);
typedef void   (*PFNGLBINDBUFFERPROC_)(GLenum target, GLuint buffer);
typedef void   (*PFNGLBUFFERDATAPROC_)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void   (*PFNGLDELETEBUFFERSPROC_)(GLsizei n, const GLuint *buffers);
typedef void   (*PFNGLENABLEVERTEXATTRIBARRAYPROC_)(GLuint index);
typedef void   (*PFNGLVERTEXATTRIBPOINTERPROC_)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLint  (*PFNGLGETATTRIBLOCATIONPROC_)(GLuint program, const GLchar *name);

/* ────────────────────────────────────────────────────────────────────────── */
/* Compositor Shared Context Semaphore                                       */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Published via AddSemaphore() so zunegfx can discover the master GL context.
 * Zunegfx calls FindSemaphore("GLCompositorMasterContext") and if found,
 * uses GLA_ShareContext with the master_context to share textures/resources.
 */
#define GLCOMPOSITOR_SEMAPHORE_NAME "GLCompositorMasterContext"

struct GLCompositorSemaphore
{
    struct SignalSemaphore   sem;
    APTR                    master_context;     /* The master GLAContext */
};

/* ────────────────────────────────────────────────────────────────────────── */
/* Per-Bitmap GPU Data                                                       */
/* ────────────────────────────────────────────────────────────────────────── */

struct StackBitMapNode
{
    struct MinNode  n;
    OOP_Object      *bm;
    struct Region   *screenregion;
    SIPTR           leftedge;
    SIPTR           topedge;
    IPTR            sbmflags;
    struct Hook     *prealphacomphook;

    /* GPU rendering data */
    struct {
        ULONG           texture_id;     /* GL texture for this bitmap */
        BOOL            is_zunegfx;     /* TRUE = shared FBO texture, no upload needed */
        APTR            pipe_resource;  /* Gallium pipe_resource for zero-copy */
        BOOL            needs_upload;   /* TRUE if texture needs re-upload from bitmap */
        UWORD           tex_width;      /* Allocated texture width */
        UWORD           tex_height;     /* Allocated texture height */
    } gpu;
};

/* sbmflags bits 0 to 3 are reserved for the normal compositing flags. */
#define STACKNODEB_VISIBLE       16
#define STACKNODEF_VISIBLE       (1 << STACKNODEB_VISIBLE)
#define STACKNODEB_DISPLAYABLE   17
#define STACKNODEF_DISPLAYABLE   (1 << STACKNODEB_DISPLAYABLE)

/* ────────────────────────────────────────────────────────────────────────── */
/* Main Compositor Data                                                      */
/* ────────────────────────────────────────────────────────────────────────── */

struct HIDDCompositorData
{
    /* ── Libraries ── */
    struct GfxBase              *GraphicsBase;
    struct IntuitionBase        *IntuitionBase;
    struct Library              *GLBase;

    /* ── Capabilities & Flags ── */
    ULONG                       capabilities;
    ULONG                       flags;

    /* ── Bitmap Management (same as software compositor) ── */
    OOP_Object                  *displaybitmap;
    OOP_Object                  *intermedbitmap;
    OOP_Object                  *screenbitmap;
    OOP_Object                  *topbitmap;

    struct Rectangle            displayrect;
    struct Region               *alpharegion;

    struct MinList              bitmapstack;
    struct SignalSemaphore      semaphore;

    struct Hook                 *backfillhook;

    OOP_Object                  *gfx;
    OOP_Object                  *fb;
    OOP_Object                  *gc;

    ULONG                       displayid;
    HIDDT_ModeID                displaymode;
    UBYTE                       displaydepth;

    struct Hook                 defaultbackfill;
    BOOL                        modeschanged;

    /* ── GPU Rendering State ── */
    struct {
        APTR        gl_context;         /* Compositor's own GL context (the master) */
        BOOL        available;          /* TRUE if GPU pipeline is usable */
        BOOL        context_valid;      /* GL context is created and current */

        OOP_MethodID    mid_DisplayResource;  /* Hidd_Gallium_DisplayResource */
        OOP_Object      *gallium_driver;      /* Gallium HIDD driver for DisplayResource */
        struct BitMap   *displayBM;           /* struct BitMap * for DisplayResource blit target */

        struct BitMap   *friendBM;            /* BitMap for Gallium driver lookup (set before RequestGPUInit) */

        /* Helper Process for deferred GPU init (Mesa needs Process context) */
        struct Process  *init_proc;     /* Helper process waiting for init signal */
        BYTE            sig_init;       /* Signal bit: "please init GPU" */
        BYTE            sig_done;       /* Signal bit: "init complete" */
        struct Task     *requester;     /* Task that requested init (to signal back) */
        BYTE            req_sig_done;   /* Signal bit in requester to signal on completion */
        BOOL            init_result;    /* TRUE if GPU init succeeded */

        /* Shared context semaphore - published for zunegfx to find */
        struct GLCompositorSemaphore shared_sem;

        /* Shaders */
        ULONG       composite_shader;  /* Texture + alpha compositing */
        ULONG       shadow_shader;     /* SDF window shadow */
        BOOL        shaders_valid;

        /* Composite shader uniforms */
        LONG        u_texture;          /* sampler2D u_texture */
        LONG        u_alpha;            /* float u_alpha */
        LONG        u_screen_size;      /* vec2 u_screen_size */

        /* Shadow shader uniforms */
        LONG        u_shadow_texture;
        LONG        u_shadow_window_pos;
        LONG        u_shadow_window_size;
        LONG        u_shadow_color;
        LONG        u_shadow_offset;
        LONG        u_shadow_blur;
        LONG        u_shadow_screen_size;

        /* Quad geometry */
        ULONG       quad_vbo;

        /* GL extension function pointers */
        PFNGLCREATESHADERPROC_          glCreateShader;
        PFNGLSHADERSOURCEPROC_          glShaderSource;
        PFNGLCOMPILESHADERPROC_         glCompileShader;
        PFNGLCREATEPROGRAMPROC_         glCreateProgram;
        PFNGLATTACHSHADERPROC_          glAttachShader;
        PFNGLLINKPROGRAMPROC_           glLinkProgram;
        PFNGLUSEPROGRAMPROC_            glUseProgram;
        PFNGLGETUNIFORMLOCATIONPROC_    glGetUniformLocation;
        PFNGLUNIFORM1IPROC_            glUniform1i;
        PFNGLUNIFORM1FPROC_            glUniform1f;
        PFNGLUNIFORM2FPROC_            glUniform2f;
        PFNGLUNIFORM4FPROC_            glUniform4f;
        PFNGLDELETESHADERPROC_          glDeleteShader;
        PFNGLDELETEPROGRAMPROC_         glDeleteProgram;
        PFNGLGETSHADERIVPROC_           glGetShaderiv;
        PFNGLGETPROGRAMIVPROC_          glGetProgramiv;
        PFNGLBINDFRAMEBUFFERPROC_       glBindFramebuffer;
        PFNGLGENBUFFERSPROC_            glGenBuffers;
        PFNGLBINDBUFFERPROC_            glBindBuffer;
        PFNGLBUFFERDATAPROC_            glBufferData;
        PFNGLDELETEBUFFERSPROC_         glDeleteBuffers;
        PFNGLENABLEVERTEXATTRIBARRAYPROC_ glEnableVertexAttribArray;
        PFNGLVERTEXATTRIBPOINTERPROC_  glVertexAttribPointer;
        PFNGLGETATTRIBLOCATIONPROC_    glGetAttribLocation;
    } gpu;
};

/* Compositor state flags */
#define COMPSTATEB_HASALPHA     0
#define COMPSTATEF_HASALPHA     (1 << COMPSTATEB_HASALPHA)
#define COMPSTATEB_DEEPLUT      1
#define COMPSTATEF_DEEPLUT      (1 << COMPSTATEB_DEEPLUT)
#define COMPSTATEB_GPUACCEL     2
#define COMPSTATEF_GPUACCEL     (1 << COMPSTATEB_GPUACCEL)

/* Method helper macro */
#define METHOD(base, id, name) \
  base ## __ ## id ## __ ## name (OOP_Class *cl, OOP_Object *o, struct p ## id ## _ ## name *msg)

/* Locking macros */
#define LOCK_COMPOSITOR_READ       { ObtainSemaphoreShared(&compdata->semaphore); }
#define LOCK_COMPOSITOR_WRITE      { ObtainSemaphore(&compdata->semaphore); }
#define UNLOCK_COMPOSITOR          { ReleaseSemaphore(&compdata->semaphore); }

/* ────────────────────────────────────────────────────────────────────────── */
/* External declarations                                                     */
/* ────────────────────────────────────────────────────────────────────────── */

extern OOP_AttrBase HiddCompositorAttrBase;
extern const struct OOP_InterfaceDescr Compositor_ifdescr[];

/* displaymode.c */
void UpdateDisplayMode(struct HIDDCompositorData *compdata);

/* glcompositor_shaders.c */
BOOL GLCompositor_LoadExtensions(struct HIDDCompositorData *compdata);
BOOL GLCompositor_CompileCompositeShader(struct HIDDCompositorData *compdata);
BOOL GLCompositor_CompileShadowShader(struct HIDDCompositorData *compdata);
void GLCompositor_DestroyShaders(struct HIDDCompositorData *compdata);
void GLCompositor_CreateQuadVBO(struct HIDDCompositorData *compdata);
void GLCompositor_DestroyQuadVBO(struct HIDDCompositorData *compdata);

#endif /* _GLCOMPOSITOR_INTERN_H */
