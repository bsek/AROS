/*
    Copyright (C) 2010-2025, The AROS Development Team. All rights reserved.

    Desc: OpenGL GPU Compositor - Main class implementation.
          Replaces the CPU-based compositor with GPU-accelerated rendering.
          Region calculation logic is reused from the software compositor.
          Rendering uses OpenGL shaders for texture compositing and SDF shadows.
*/

#define DEBUG 1
#if (DEBUG)
#define DTOGGLE(x) x
#define DMOVE(x) x
#define DRECALC(x) x
#define DREDRAWBM(x) x
#define DREDRAWSCR(x) x
#define DSTACK(x) x
#define DUPDATE(x) x
#define DGPU(x) x
#else
#define DTOGGLE(x)
#define DMOVE(x)
#define DRECALC(x)
#define DREDRAWBM(x)
#define DREDRAWSCR(x)
#define DSTACK(x)
#define DUPDATE(x)
#define DGPU(x)
#endif

#include <aros/debug.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/oop.h>
#include <proto/utility.h>

#include <graphics/view.h>
#include <hidd/gfx.h>

#include <GL/gl.h>
#include <GL/gla.h>

#include "glcompositor_intern.h"

/* GL constants that may not be in gl.h */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER     0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW      0x88E4
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER      0x8D40
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS   0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS      0x8B82
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER    0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER  0x8B30
#endif

#define COMPOSITOR_PREFS "SYS/compositor.prefs"
#define COMPOSITOR_PEFSTEMPLATE  "ABOVE/S,BELOW/S,LEFT/S,RIGHT/S,ALPHA/S"

#define CAPABILITY_FLAGS (COMPF_ABOVE|COMPF_BELOW|COMPF_LEFT|COMPF_RIGHT|COMPF_ALPHA)

enum
{
    ARG_ABOVE = 0,
    ARG_BELOW,
    ARG_LEFT,
    ARG_RIGHT,
    ARG_ALPHA,
    NOOFARGS
};

#ifdef GfxBase
#undef GfxBase
#endif
#define GfxBase compdata->GraphicsBase
#ifdef IntuitionBase
#undef IntuitionBase
#endif
#define IntuitionBase compdata->IntuitionBase

#define _RECT(x) x.MinX, x.MinY, x.MaxX, x.MaxY

#define MAX(a,b) a > b ? a : b
#define MIN(a,b) a < b ? a : b

/* ────────────────────────────────────────────────────────────────────────── */
/* Helper: Check if rectangle intersects region                              */
/* ────────────────────────────────────────────────────────────────────────── */

static BOOL isRectInRegion(struct Region *region, struct Rectangle *rect)
{
    struct RegionRectangle *rrect = region->RegionRectangle;

    while (rrect)
    {
        if (AndRectRect(&rrect->bounds, rect, NULL))
            return TRUE;
        rrect = rrect->Next;
    }
    return FALSE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Stack management helpers (identical to software compositor)               */
/* ────────────────────────────────────────────────────────────────────────── */

static struct StackBitMapNode *HIDDCompositorFindBitMapStackNode(
    struct HIDDCompositorData *compdata, OOP_Object *bm)
{
    struct StackBitMapNode *n = NULL;

    ForeachNode(&compdata->bitmapstack, n)
    {
        if (n->bm == bm)
            return n;
    }
    return NULL;
}

static struct Screen *HIDDCompositorFindBitMapScreen(
    struct HIDDCompositorData *compdata, OOP_Object *bm)
{
    struct Screen *curScreen = NULL;

    for (curScreen = IntuitionBase->FirstScreen; curScreen != NULL;
         curScreen = curScreen->NextScreen)
    {
        if (bm == HIDD_BM_OBJ(curScreen->RastPort.BitMap))
            return curScreen;
    }
    return (struct Screen *)NULL;
}

static VOID HIDDCompositorValidateBitMapPositionChange(
    OOP_Object *bm, SIPTR *newxoffset, SIPTR *newyoffset,
    LONG displayedwidth, LONG displayedheight)
{
    IPTR width, height;
    LONG neglimit, poslimit;

    OOP_GetAttr(bm, aHidd_BitMap_Width, &width);
    OOP_GetAttr(bm, aHidd_BitMap_Height, &height);

    if (width > displayedwidth)
    {
        neglimit = displayedwidth - width;
        poslimit = 0;
    }
    else
    {
        neglimit = 0;
        poslimit = displayedwidth - width;
    }

    if (*(newxoffset) > poslimit)
        *(newxoffset) = poslimit;
    if (*(newxoffset) < neglimit)
        *(newxoffset) = neglimit;

    if (height > displayedheight)
        neglimit = displayedheight - height;
    else
        neglimit = 0;
    poslimit = displayedheight - 15;

    if (*(newyoffset) > poslimit)
        *(newyoffset) = poslimit;
    if (*(newyoffset) < neglimit)
        *(newyoffset) = neglimit;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Region calculation (identical to software compositor)                     */
/* ────────────────────────────────────────────────────────────────────────── */

static VOID HIDDCompositorRecalculateVisibleRegions(struct HIDDCompositorData *compdata)
{
    struct StackBitMapNode *n = NULL, *tmpn;
    struct Region *dispvisregion = NULL;
    ULONG oldflags = compdata->flags;

    DRECALC(bug("[GLCompositor] %s: Display rect [%d, %d - %d, %d]\n", __func__,
                _RECT(compdata->displayrect)));

    if ((dispvisregion = NewRegion()) != NULL)
    {
        OrRectRegion(dispvisregion, &compdata->displayrect);

        compdata->flags &= ~COMPSTATEF_HASALPHA;
        if (compdata->alpharegion)
        {
            DisposeRegion(compdata->alpharegion);
            compdata->alpharegion = NULL;
        }

        ForeachNodeSafe(&compdata->bitmapstack, n, tmpn)
        {
            struct Rectangle tmprect;

            n->sbmflags &= ~STACKNODEF_VISIBLE;

            if (n->screenregion)
                ClearRegion(n->screenregion);
            else
                n->screenregion = NewRegion();

            if (n->screenregion)
            {
                struct RegionRectangle *srrect;

                tmprect.MinX = n->leftedge;
                tmprect.MaxX = n->leftedge + OOP_GET(n->bm, aHidd_BitMap_Width) - 1;
                tmprect.MinY = n->topedge;
                tmprect.MaxY = n->topedge + OOP_GET(n->bm, aHidd_BitMap_Height) - 1;

                OrRectRegion(n->screenregion, &tmprect);
                AndRegionRegion(dispvisregion, n->screenregion);

                if ((srrect = n->screenregion->RegionRectangle) != NULL)
                {
                    while (srrect)
                    {
                        tmprect.MinX = srrect->bounds.MinX + n->screenregion->bounds.MinX;
                        tmprect.MinY = srrect->bounds.MinY + n->screenregion->bounds.MinY;
                        tmprect.MaxX = srrect->bounds.MaxX + n->screenregion->bounds.MinX;
                        tmprect.MaxY = srrect->bounds.MaxY + n->screenregion->bounds.MinY;

                        if (!(n->sbmflags & COMPF_ALPHA))
                            ClearRectRegion(dispvisregion, &tmprect);
                        else
                        {
                            compdata->flags |= COMPSTATEF_HASALPHA;
                            if (!(compdata->alpharegion))
                                compdata->alpharegion = NewRegion();
                            OrRectRegion(compdata->alpharegion, &tmprect);
                        }
                        srrect = srrect->Next;
                    }

                    if (!(compdata->capabilities & COMPF_ABOVE) && !(n->sbmflags & COMPF_ABOVE))
                    {
                        tmprect.MinX = compdata->displayrect.MinX;
                        tmprect.MaxX = compdata->displayrect.MaxX;
                        tmprect.MinY = compdata->displayrect.MinY;
                        tmprect.MaxY = n->topedge - 1;
                        ClearRectRegion(dispvisregion, &tmprect);
                    }
                    if (!(compdata->capabilities & COMPF_BELOW) && !(n->sbmflags & COMPF_BELOW))
                    {
                        tmprect.MinX = compdata->displayrect.MinX;
                        tmprect.MaxX = compdata->displayrect.MaxX;
                        tmprect.MinY = n->topedge + OOP_GET(n->bm, aHidd_BitMap_Height);
                        tmprect.MaxY = compdata->displayrect.MaxY;
                        ClearRectRegion(dispvisregion, &tmprect);
                    }
                    if (!(compdata->capabilities & COMPF_LEFT) && !(n->sbmflags & COMPF_LEFT))
                    {
                        tmprect.MinX = compdata->displayrect.MinX;
                        tmprect.MaxX = n->leftedge - 1;
                        tmprect.MinY = n->topedge;
                        tmprect.MaxY = n->topedge + OOP_GET(n->bm, aHidd_BitMap_Height) - 1;
                        ClearRectRegion(dispvisregion, &tmprect);
                    }
                    if (!(compdata->capabilities & COMPF_RIGHT) && !(n->sbmflags & COMPF_RIGHT))
                    {
                        tmprect.MinX = n->leftedge + OOP_GET(n->bm, aHidd_BitMap_Width);
                        tmprect.MaxX = compdata->displayrect.MaxX;
                        tmprect.MinY = n->topedge;
                        tmprect.MaxY = n->topedge + OOP_GET(n->bm, aHidd_BitMap_Height) - 1;
                        ClearRectRegion(dispvisregion, &tmprect);
                    }
                    n->sbmflags |= STACKNODEF_VISIBLE;
                }
            }
        }
        DisposeRegion(dispvisregion);

        if (compdata->flags != oldflags)
        {
            ULONG newflags = (~oldflags) & compdata->flags;
            if ((!(newflags & COMPSTATEF_HASALPHA)) && (oldflags & COMPSTATEF_HASALPHA))
            {
                if (compdata->alpharegion)
                    DisposeRegion(compdata->alpharegion);
                compdata->alpharegion = NULL;
            }
        }
    }
}

/* ────────────────────────────────────────────────────────────────────────── */
/* GPU Initialization and Teardown                                           */
/* ────────────────────────────────────────────────────────────────────────── */

static BOOL InitGPUCompositor(struct HIDDCompositorData *compdata)
{
    struct TagItem ctxtags[] =
    {
        { GLA_Width,        1 },
        { GLA_Height,       1 },
        { GLA_NoDepth,      GL_TRUE },
        { GLA_NoStencil,    GL_TRUE },
        { GLA_NoAccum,      GL_TRUE },
        { GLA_DoubleBuf,    GL_TRUE },
        { GLA_RGBMode,      GL_TRUE },
        { TAG_DONE,         0 }
    };

    D(bug("[GLCompositor] %s: Initializing GPU compositor\n", __func__));

    compdata->gpu.available = FALSE;
    compdata->gpu.context_valid = FALSE;

    /* Create master GL context (no sharing — this IS the master) */
    compdata->gpu.gl_context = glACreateContext(ctxtags);
    if (!compdata->gpu.gl_context)
    {
        D(bug("[GLCompositor] %s: Failed to create GL context\n", __func__));
        return FALSE;
    }

    D(bug("[GLCompositor] %s: GL context created @ %p\n", __func__, compdata->gpu.gl_context));

    /* Make current so we can load extensions and compile shaders */
    glAMakeCurrent(compdata->gpu.gl_context);
    compdata->gpu.context_valid = TRUE;

    /* Publish master context via named semaphore for zunegfx to discover */
    compdata->gpu.shared_sem.sem.ss_Link.ln_Name = GLCOMPOSITOR_SEMAPHORE_NAME;
    compdata->gpu.shared_sem.sem.ss_Link.ln_Pri = 0;
    compdata->gpu.shared_sem.master_context = compdata->gpu.gl_context;
    InitSemaphore(&compdata->gpu.shared_sem.sem);
    AddSemaphore(&compdata->gpu.shared_sem.sem);

    D(bug("[GLCompositor] %s: Published master context semaphore '%s'\n",
          __func__, GLCOMPOSITOR_SEMAPHORE_NAME));

    /* Load GL extension function pointers */
    if (!GLCompositor_LoadExtensions(compdata))
    {
        D(bug("[GLCompositor] %s: Failed to load GL extensions\n", __func__));
        goto fail;
    }

    /* Compile shaders */
    if (!GLCompositor_CompileCompositeShader(compdata))
    {
        D(bug("[GLCompositor] %s: Failed to compile composite shader\n", __func__));
        goto fail;
    }

    if (!GLCompositor_CompileShadowShader(compdata))
    {
        D(bug("[GLCompositor] %s: Shadow shader failed (non-fatal)\n", __func__));
        /* Shadow shader is optional — compositing still works without it */
    }

    compdata->gpu.shaders_valid = TRUE;

    /* Create unit quad VBO */
    GLCompositor_CreateQuadVBO(compdata);

    compdata->gpu.available = TRUE;
    compdata->flags |= COMPSTATEF_GPUACCEL;

    D(bug("[GLCompositor] %s: GPU compositor initialized successfully\n", __func__));
    return TRUE;

fail:
    RemSemaphore(&compdata->gpu.shared_sem.sem);
    if (compdata->gpu.gl_context)
    {
        glADestroyContext(compdata->gpu.gl_context);
        compdata->gpu.gl_context = NULL;
    }
    compdata->gpu.context_valid = FALSE;
    return FALSE;
}

static void ShutdownGPUCompositor(struct HIDDCompositorData *compdata)
{
    D(bug("[GLCompositor] %s: Shutting down GPU compositor\n", __func__));

    if (compdata->gpu.gl_context)
    {
        glAMakeCurrent(compdata->gpu.gl_context);

        /* Destroy per-bitmap textures */
        struct StackBitMapNode *n;
        ForeachNode(&compdata->bitmapstack, n)
        {
            if (n->gpu.texture_id && !n->gpu.is_zunegfx)
            {
                glDeleteTextures(1, &n->gpu.texture_id);
                n->gpu.texture_id = 0;
            }
        }

        GLCompositor_DestroyQuadVBO(compdata);
        GLCompositor_DestroyShaders(compdata);

        /* Remove published semaphore before destroying context */
        RemSemaphore(&compdata->gpu.shared_sem.sem);

        glADestroyContext(compdata->gpu.gl_context);
        compdata->gpu.gl_context = NULL;
    }

    compdata->gpu.available = FALSE;
    compdata->gpu.context_valid = FALSE;
    compdata->flags &= ~COMPSTATEF_GPUACCEL;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* GPU Texture Management                                                    */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Ensure a GL texture exists for the given bitmap node.
 * For zunegfx bitmaps: uses shared texture_id directly (zero-copy).
 * For standard bitmaps: creates a texture and uploads pixel data.
 */
static BOOL GPUEnsureTexture(struct HIDDCompositorData *compdata, struct StackBitMapNode *n)
{
    IPTR width, height;

    OOP_GetAttr(n->bm, aHidd_BitMap_Width, &width);
    OOP_GetAttr(n->bm, aHidd_BitMap_Height, &height);

    if (n->gpu.is_zunegfx && n->gpu.texture_id)
    {
        /* Zunegfx FBO texture — shared via GL context, always valid */
        return TRUE;
    }

    /* Standard bitmap — need our own texture */
    if (!n->gpu.texture_id)
    {
        glGenTextures(1, &n->gpu.texture_id);
        if (!n->gpu.texture_id)
            return FALSE;

        glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        n->gpu.tex_width = 0;
        n->gpu.tex_height = 0;
        n->gpu.needs_upload = TRUE;
    }

    /* Upload pixel data if needed */
    if (n->gpu.needs_upload || n->gpu.tex_width != (UWORD)width || n->gpu.tex_height != (UWORD)height)
    {
        UBYTE *baseaddress;
        ULONG bmwidth, bmheight, banksize, memsize;

        if (HIDD_BM_ObtainDirectAccess(n->bm, &baseaddress, &bmwidth, &bmheight, &banksize, &memsize))
        {
            IPTR modulo;
            OOP_GetAttr(n->bm, aHidd_BitMap_BytesPerRow, &modulo);

            glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);

            if (n->gpu.tex_width != (UWORD)width || n->gpu.tex_height != (UWORD)height)
            {
                /* Full re-upload */
                glPixelStorei(GL_UNPACK_ROW_LENGTH, modulo / 4);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                             0, GL_BGRA, GL_UNSIGNED_BYTE, baseaddress);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

                n->gpu.tex_width = (UWORD)width;
                n->gpu.tex_height = (UWORD)height;

                DGPU(bug("[GLCompositor] %s: Full texture upload %dx%d for bm %p (tex %d)\n",
                     __func__, width, height, n->bm, n->gpu.texture_id));
            }
            else
            {
                /* Incremental sub-image update */
                glPixelStorei(GL_UNPACK_ROW_LENGTH, modulo / 4);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                                GL_BGRA, GL_UNSIGNED_BYTE, baseaddress);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            }

            HIDD_BM_ReleaseDirectAccess(n->bm);
            n->gpu.needs_upload = FALSE;
        }
        else
        {
            D(bug("[GLCompositor] %s: Failed to obtain direct access for bm %p\n", __func__, n->bm));
            return FALSE;
        }
    }

    return TRUE;
}

static void GPUFreeNodeTexture(struct StackBitMapNode *n)
{
    if (n->gpu.texture_id && !n->gpu.is_zunegfx)
    {
        glDeleteTextures(1, &n->gpu.texture_id);
    }
    n->gpu.texture_id = 0;
    n->gpu.tex_width = 0;
    n->gpu.tex_height = 0;
    n->gpu.is_zunegfx = FALSE;
    n->gpu.pipe_resource = NULL;
    n->gpu.needs_upload = FALSE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* GPU Rendering                                                             */
/* ────────────────────────────────────────────────────────────────────────── */

/*
 * Draw a shadow behind the given bitmap node using the SDF shadow shader.
 */
static void GPUDrawShadow(struct HIDDCompositorData *compdata, struct StackBitMapNode *n)
{
    IPTR width, height;
    GLint a_position;

    if (!compdata->gpu.shadow_shader)
        return;

    OOP_GetAttr(n->bm, aHidd_BitMap_Width, &width);
    OOP_GetAttr(n->bm, aHidd_BitMap_Height, &height);

    compdata->gpu.glUseProgram(compdata->gpu.shadow_shader);

    compdata->gpu.glUniform2f(compdata->gpu.u_shadow_screen_size,
        (GLfloat)(compdata->displayrect.MaxX - compdata->displayrect.MinX + 1),
        (GLfloat)(compdata->displayrect.MaxY - compdata->displayrect.MinY + 1));
    compdata->gpu.glUniform2f(compdata->gpu.u_shadow_window_pos,
        (GLfloat)n->leftedge, (GLfloat)n->topedge);
    compdata->gpu.glUniform2f(compdata->gpu.u_shadow_window_size,
        (GLfloat)width, (GLfloat)height);
    compdata->gpu.glUniform2f(compdata->gpu.u_shadow_offset, 4.0f, 4.0f);
    compdata->gpu.glUniform4f(compdata->gpu.u_shadow_color,
        0.0f, 0.0f, 0.0f, 0.5f);
    compdata->gpu.glUniform1f(compdata->gpu.u_shadow_blur, 12.0f);

    /* Draw using unit quad VBO */
    compdata->gpu.glBindBuffer(GL_ARRAY_BUFFER, compdata->gpu.quad_vbo);
    a_position = compdata->gpu.glGetAttribLocation(compdata->gpu.shadow_shader, "a_position");
    if (a_position >= 0)
    {
        compdata->gpu.glEnableVertexAttribArray(a_position);
        compdata->gpu.glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE,
                                            4 * sizeof(GLfloat), (void *)0);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    compdata->gpu.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*
 * Render a single bitmap node as a textured quad.
 * The rect is in screen coordinates.
 */
static void GPUCompositorRedrawBitmap(struct HIDDCompositorData *compdata,
    struct StackBitMapNode *n, struct Rectangle *rect)
{
    IPTR bmwidth, bmheight;
    GLfloat screenw, screenh;
    GLfloat x0, y0, x1, y1;
    GLfloat u0, v0, u1, v1;
    GLint a_position, a_texcoord;
    GLfloat alpha;
    GLfloat vertices[16]; /* 4 vertices * (2 pos + 2 tex) */

    DREDRAWBM(bug("[GLCompositor] %s: Redraw bm %p, rect [%d, %d - %d, %d]\n",
                  __func__, n->bm, _RECT((*rect))));

    if (!GPUEnsureTexture(compdata, n))
    {
        D(bug("[GLCompositor] %s: Failed to ensure texture for bm %p\n", __func__, n->bm));
        return;
    }

    OOP_GetAttr(n->bm, aHidd_BitMap_Width, &bmwidth);
    OOP_GetAttr(n->bm, aHidd_BitMap_Height, &bmheight);

    screenw = (GLfloat)(compdata->displayrect.MaxX - compdata->displayrect.MinX + 1);
    screenh = (GLfloat)(compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);

    /* Screen-space pixel coordinates for the visible rect */
    x0 = (GLfloat)rect->MinX;
    y0 = (GLfloat)rect->MinY;
    x1 = (GLfloat)(rect->MaxX + 1);
    y1 = (GLfloat)(rect->MaxY + 1);

    /* Compute texture coordinates from bitmap-relative pixel coords */
    if (bmwidth > 0 && bmheight > 0)
    {
        u0 = (GLfloat)(rect->MinX - n->leftedge) / (GLfloat)bmwidth;
        v0 = (GLfloat)(rect->MinY - n->topedge)  / (GLfloat)bmheight;
        u1 = (GLfloat)(rect->MaxX - n->leftedge + 1) / (GLfloat)bmwidth;
        v1 = (GLfloat)(rect->MaxY - n->topedge + 1)  / (GLfloat)bmheight;
    }
    else
    {
        u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
    }

    /* Activate composite shader */
    compdata->gpu.glUseProgram(compdata->gpu.composite_shader);
    compdata->gpu.glUniform2f(compdata->gpu.u_screen_size, screenw, screenh);
    compdata->gpu.glUniform1i(compdata->gpu.u_texture, 0);

    alpha = (n->sbmflags & COMPF_ALPHA) ? 1.0f : 1.0f;
    compdata->gpu.glUniform1f(compdata->gpu.u_alpha, alpha);

    /* Bind bitmap texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);

    /* Build vertex data: pos.x, pos.y, tex.u, tex.v */
    vertices[0]  = x0; vertices[1]  = y0; vertices[2]  = u0; vertices[3]  = v0;
    vertices[4]  = x1; vertices[5]  = y0; vertices[6]  = u1; vertices[7]  = v0;
    vertices[8]  = x1; vertices[9]  = y1; vertices[10] = u1; vertices[11] = v1;
    vertices[12] = x0; vertices[13] = y1; vertices[14] = u0; vertices[15] = v1;

    /* Set up vertex attributes */
    a_position = compdata->gpu.glGetAttribLocation(compdata->gpu.composite_shader, "a_position");
    a_texcoord = compdata->gpu.glGetAttribLocation(compdata->gpu.composite_shader, "a_texcoord");

    if (a_position >= 0)
    {
        compdata->gpu.glEnableVertexAttribArray(a_position);
        compdata->gpu.glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE,
                                            4 * sizeof(GLfloat), &vertices[0]);
    }
    if (a_texcoord >= 0)
    {
        compdata->gpu.glEnableVertexAttribArray(a_texcoord);
        compdata->gpu.glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE,
                                            4 * sizeof(GLfloat), &vertices[2]);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
}

/*
 * CPU fallback: blit using the HIDD software path (identical to original compositor).
 */
static inline void CPUCompositorRedrawBitmap(struct HIDDCompositorData *compdata,
    OOP_Object *renderTarget, struct StackBitMapNode *n, struct Rectangle *rect)
{
    ULONG blitwidth  = rect->MaxX - rect->MinX + 1;
    ULONG blitheight = rect->MaxY - rect->MinY + 1;

    if (renderTarget)
    {
        if (!(n->sbmflags & COMPF_ALPHA))
        {
            HIDD_Gfx_CopyBox(compdata->gfx, n->bm,
                            rect->MinX - n->leftedge, rect->MinY - n->topedge,
                            renderTarget,
                            rect->MinX, rect->MinY, blitwidth, blitheight,
                            compdata->gc);
        }
        else
        {
            UBYTE *baseaddress;
            ULONG width, height, banksize, memsize;
            IPTR modulo;

            if (HIDD_BM_ObtainDirectAccess(n->bm, &baseaddress, &width, &height, &banksize, &memsize))
            {
                OOP_GetAttr(n->bm, aHidd_BitMap_BytesPerRow, &modulo);
                HIDD_BM_PutAlphaImage(renderTarget, compdata->gfx,
                    baseaddress + ((rect->MinY - n->topedge) * modulo) + ((rect->MinX - n->leftedge) << 2),
                    modulo, rect->MinX, rect->MinY, blitwidth, blitheight);
                HIDD_BM_ReleaseDirectAccess(n->bm);
            }
        }
    }
}

static inline void HIDDCompositorFillRect(struct HIDDCompositorData *compdata,
    OOP_Object *renderTarget, ULONG MinX, ULONG MinY, ULONG MaxX, ULONG MaxY)
{
    HIDD_BM_FillRect(renderTarget, compdata->gc, MinX, MinY, MaxX, MaxY);
}

/* ────────────────────────────────────────────────────────────────────────── */
/* GPU Full-scene Redraw                                                     */
/* ────────────────────────────────────────────────────────────────────────── */

static VOID GPUCompositorRedrawVisibleRegions(struct HIDDCompositorData *compdata,
    struct Rectangle *drawrect)
{
    struct StackBitMapNode *n;
    GLfloat screenw, screenh;

    DREDRAWSCR(bug("[GLCompositor] %s: GPU redraw\n", __func__));

    if (!compdata->gpu.available || !compdata->gpu.gl_context)
    {
        D(bug("[GLCompositor] %s: GPU not available, skipping\n", __func__));
        return;
    }

    /* Make compositor GL context current */
    glAMakeCurrent(compdata->gpu.gl_context);

    screenw = (GLfloat)(compdata->displayrect.MaxX - compdata->displayrect.MinX + 1);
    screenh = (GLfloat)(compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);

    /* Render to the default framebuffer (screen) */
    if (compdata->gpu.glBindFramebuffer)
        compdata->gpu.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, (GLsizei)screenw, (GLsizei)screenh);

    /* Clear with background color */
    glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Enable alpha blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);

    /*
     * Render bitmaps back-to-front (bottom of stack first).
     * For alpha bitmaps, draw shadow first, then the bitmap.
     */
    for (n = (struct StackBitMapNode *)compdata->bitmapstack.mlh_TailPred;
         n->n.mln_Pred; n = (struct StackBitMapNode *)n->n.mln_Pred)
    {
        if (!(n->sbmflags & STACKNODEF_VISIBLE))
            continue;

        if (!n->screenregion)
            continue;

        struct RegionRectangle *srrect = n->screenregion->RegionRectangle;
        if (!srrect)
            continue;

        /* Draw shadow for alpha bitmaps */
        if ((n->sbmflags & COMPF_ALPHA) && compdata->gpu.shadow_shader)
            GPUDrawShadow(compdata, n);

        /* Upload/bind texture and draw visible regions */
        while (srrect)
        {
            struct Rectangle tmprect;

            tmprect.MinX = srrect->bounds.MinX + n->screenregion->bounds.MinX;
            tmprect.MinY = srrect->bounds.MinY + n->screenregion->bounds.MinY;
            tmprect.MaxX = srrect->bounds.MaxX + n->screenregion->bounds.MinX;
            tmprect.MaxY = srrect->bounds.MaxY + n->screenregion->bounds.MinY;

            if (!drawrect || AndRectRect(drawrect, &tmprect, &tmprect))
            {
                GPUCompositorRedrawBitmap(compdata, n, &tmprect);
            }
            srrect = srrect->Next;
        }
    }

    glDisable(GL_BLEND);

    /* Present to screen */
    glASwapBuffers(compdata->gpu.gl_context);

    DREDRAWSCR(bug("[GLCompositor] %s: GPU redraw complete\n", __func__));
}

/* ────────────────────────────────────────────────────────────────────────── */
/* CPU Fallback Redraw (from original compositor)                            */
/* ────────────────────────────────────────────────────────────────────────── */

static VOID CPUCompositorRedrawAlphaRegions(struct HIDDCompositorData *compdata,
    struct Rectangle *drawrect)
{
    OOP_Object *renderTarget = compdata->displaybitmap;
    struct Rectangle alpharect;
    struct StackBitMapNode *n;
    struct BitMap *backbm;

    if (compdata->intermedbitmap)
        renderTarget = compdata->intermedbitmap;

    OOP_GetAttr(renderTarget, aHidd_BitMap_BMStruct, (IPTR *)&backbm);

    for (n = (struct StackBitMapNode *)compdata->bitmapstack.mlh_TailPred;
         n->n.mln_Pred; n = (struct StackBitMapNode *)n->n.mln_Pred)
    {
        if ((n->sbmflags & STACKNODEF_VISIBLE) &&
            (n->sbmflags & COMPF_ALPHA) &&
            (n->screenregion))
        {
            struct RegionRectangle *srrect;

            if ((srrect = n->screenregion->RegionRectangle) != NULL)
            {
                while (srrect)
                {
                    alpharect.MinX = srrect->bounds.MinX + n->screenregion->bounds.MinX;
                    alpharect.MinY = srrect->bounds.MinY + n->screenregion->bounds.MinY;
                    alpharect.MaxX = srrect->bounds.MaxX + n->screenregion->bounds.MinX;
                    alpharect.MaxY = srrect->bounds.MaxY + n->screenregion->bounds.MinY;

                    if (!(drawrect) || AndRectRect(drawrect, &alpharect, &alpharect))
                    {
                        if ((n->prealphacomphook) && (backbm != NULL))
                        {
                            struct HIDD_BackFillHookMsg preprocessmsg;
                            preprocessmsg.bounds = &alpharect;
                            preprocessmsg.offsetx = 0;
                            preprocessmsg.offsety = 0;
                            CallHookPkt(n->prealphacomphook, backbm, &preprocessmsg);
                        }

                        CPUCompositorRedrawBitmap(compdata, renderTarget, n, &alpharect);
                        if (renderTarget == compdata->displaybitmap)
                            HIDD_BM_UpdateRect(compdata->displaybitmap,
                               alpharect.MinX, alpharect.MinY,
                               alpharect.MaxX - alpharect.MinX + 1,
                               alpharect.MaxY - alpharect.MinY + 1);
                    }
                    srrect = srrect->Next;
                }
            }
        }
    }
}

static VOID CPUCompositorRedrawVisibleRegions(struct HIDDCompositorData *compdata,
    struct Rectangle *drawrect)
{
    OOP_Object *renderTarget = compdata->displaybitmap;
    struct Region *dispvisregion = NULL;
    struct Rectangle tmprect;
    struct BitMap *clearbm;
    struct StackBitMapNode *n;

    if (!renderTarget)
        return;

    if (!(drawrect))
        HIDDCompositorRecalculateVisibleRegions(compdata);

    if ((compdata->flags & COMPSTATEF_HASALPHA) && (compdata->intermedbitmap))
        renderTarget = compdata->intermedbitmap;

    if ((dispvisregion = NewRegion()) != NULL)
    {
        if (drawrect)
            OrRectRegion(dispvisregion, drawrect);
        else
            OrRectRegion(dispvisregion, &compdata->displayrect);

        ForeachNode(&compdata->bitmapstack, n)
        {
            if ((n->sbmflags & STACKNODEF_VISIBLE) &&
                (!(n->sbmflags & COMPF_ALPHA)) &&
                (n->screenregion))
            {
                struct RegionRectangle *srrect;

                if ((srrect = n->screenregion->RegionRectangle) != NULL)
                {
                    while (srrect)
                    {
                        tmprect.MinX = srrect->bounds.MinX + n->screenregion->bounds.MinX;
                        tmprect.MinY = srrect->bounds.MinY + n->screenregion->bounds.MinY;
                        tmprect.MaxX = srrect->bounds.MaxX + n->screenregion->bounds.MinX;
                        tmprect.MaxY = srrect->bounds.MaxY + n->screenregion->bounds.MinY;

                        if (!(drawrect) || AndRectRect(drawrect, &tmprect, &tmprect))
                        {
                            CPUCompositorRedrawBitmap(compdata, renderTarget, n, &tmprect);
                            if (renderTarget == compdata->displaybitmap)
                                HIDD_BM_UpdateRect(compdata->displaybitmap,
                                   tmprect.MinX, tmprect.MinY,
                                   tmprect.MaxX - tmprect.MinX + 1,
                                   tmprect.MaxY - tmprect.MinY + 1);
                        }
                        srrect = srrect->Next;
                    }
                    ClearRegionRegion(n->screenregion, dispvisregion);
                }
            }
        }

        OOP_GetAttr(renderTarget, aHidd_BitMap_BMStruct, (IPTR *)&clearbm);

        struct RegionRectangle *dispclrrect = dispvisregion->RegionRectangle;
        while (dispclrrect)
        {
            struct HIDD_BackFillHookMsg clearmsg;

            tmprect.MinX = dispclrrect->bounds.MinX + dispvisregion->bounds.MinX;
            tmprect.MinY = dispclrrect->bounds.MinY + dispvisregion->bounds.MinY;
            tmprect.MaxX = dispclrrect->bounds.MaxX + dispvisregion->bounds.MinX;
            tmprect.MaxY = dispclrrect->bounds.MaxY + dispvisregion->bounds.MinY;

            if (!(drawrect) || AndRectRect(drawrect, &tmprect, &tmprect))
            {
                if (clearbm)
                {
                    clearmsg.bounds = &tmprect;
                    clearmsg.offsetx = 0;
                    clearmsg.offsety = 0;
                    CallHookPkt(compdata->backfillhook, clearbm, &clearmsg);
                }
                else
                    HIDDCompositorFillRect(compdata, renderTarget,
                        tmprect.MinX, tmprect.MinY, tmprect.MaxX, tmprect.MaxY);

                if (renderTarget == compdata->displaybitmap)
                    HIDD_BM_UpdateRect(compdata->displaybitmap,
                       tmprect.MinX, tmprect.MinY,
                       tmprect.MaxX - tmprect.MinX + 1,
                       tmprect.MaxY - tmprect.MinY + 1);
            }
            dispclrrect = dispclrrect->Next;
        }

        if (compdata->flags & COMPSTATEF_HASALPHA)
            CPUCompositorRedrawAlphaRegions(compdata, drawrect);

        DisposeRegion(dispvisregion);
    }

    if (renderTarget != compdata->displaybitmap)
    {
        if (!(drawrect))
        {
            HIDD_Gfx_CopyBox(compdata->gfx, renderTarget,
                    compdata->displayrect.MinX, compdata->displayrect.MinY,
                    compdata->displaybitmap,
                    compdata->displayrect.MinX, compdata->displayrect.MinY,
                    compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                    compdata->displayrect.MaxY - compdata->displayrect.MinY + 1,
                    compdata->gc);
            HIDD_BM_UpdateRect(compdata->displaybitmap,
                    compdata->displayrect.MinX, compdata->displayrect.MinY,
                    compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                    compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);
        }
        else
        {
            HIDD_Gfx_CopyBox(compdata->gfx, renderTarget,
                    drawrect->MinX, drawrect->MinY,
                    compdata->displaybitmap,
                    drawrect->MinX, drawrect->MinY,
                    drawrect->MaxX - drawrect->MinX + 1,
                    drawrect->MaxY - drawrect->MinY + 1,
                    compdata->gc);
            HIDD_BM_UpdateRect(compdata->displaybitmap,
                    drawrect->MinX, drawrect->MinY,
                    drawrect->MaxX - drawrect->MinX + 1,
                    drawrect->MaxY - drawrect->MinY + 1);
        }
    }
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Unified Redraw dispatcher                                                 */
/* ────────────────────────────────────────────────────────────────────────── */

static VOID HIDDCompositorRedrawVisibleRegions(struct HIDDCompositorData *compdata,
    struct Rectangle *drawrect)
{
    DREDRAWSCR(bug("[GLCompositor] %s: Redrawing display (GPU=%d)\n", __func__,
                   compdata->gpu.available));

    if (!(drawrect))
        HIDDCompositorRecalculateVisibleRegions(compdata);

    if (compdata->gpu.available)
    {
        GPUCompositorRedrawVisibleRegions(compdata, drawrect);
    }
    else
    {
        /* CPU fallback */
        CPUCompositorRedrawVisibleRegions(compdata, drawrect);
    }
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Compositing Toggle                                                        */
/* ────────────────────────────────────────────────────────────────────────── */

static BOOL HIDDCompositorToggleCompositing(struct HIDDCompositorData *compdata, BOOL newtop)
{
    OOP_Object *olddisplaybitmap = compdata->displaybitmap;
    OOP_Object *oldintermedbitmap = compdata->intermedbitmap;
    struct StackBitMapNode *topnode = (struct StackBitMapNode *)compdata->bitmapstack.mlh_Head;
    OOP_Object *newsdispbitmap = NULL;
    struct BitMap *tmpBM;
    struct TagItem bmtags[2];

    BOOL ok = TRUE, composit = FALSE;

    if (compdata->modeschanged)
    {
        DTOGGLE(bug("[GLCompositor] %s: Display Mode changed\n", __func__));
        compdata->displaybitmap = NULL;
    }

    bmtags[0].ti_Tag = BMATags_DisplayID;
    bmtags[0].ti_Data = (compdata->displayid | compdata->displaymode);
    bmtags[1].ti_Tag = TAG_DONE;
    bmtags[1].ti_Data = TAG_DONE;

    if ((topnode->topedge > 0) ||
        ((compdata->displayrect.MaxY - compdata->displayrect.MinY + 1) > OOP_GET(topnode->bm, aHidd_BitMap_Height)))
        composit = TRUE;
    else if ((topnode->leftedge > 0) ||
             ((compdata->displayrect.MaxX - compdata->displayrect.MinX + 1) > OOP_GET(topnode->bm, aHidd_BitMap_Width)))
        composit = TRUE;
    else if (topnode->sbmflags & COMPF_ALPHA)
        composit = TRUE;

    if (composit)
    {
        if (compdata->displaybitmap == NULL)
        {
            DTOGGLE(bug("[GLCompositor] %s: Initialising Display-Compositor..\n", __func__));

            if (compdata->fb)
            {
                if (olddisplaybitmap != compdata->fb)
                {
                    compdata->screenbitmap = HIDD_Gfx_Show(compdata->gfx, compdata->fb, fHidd_Gfx_Show_CopyBack);
                }
                OOP_SetAttrsTags(compdata->fb, aHidd_BitMap_ModeID, compdata->displaymode, TAG_DONE);
                compdata->displaybitmap = compdata->fb;
            }
            else
            {
                tmpBM = AllocBitMap(
                    compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                    compdata->displayrect.MaxY - compdata->displayrect.MinY + 1,
                    compdata->displaydepth,
                    BMF_DISPLAYABLE|BMF_CHECKVALUE, (struct BitMap *)bmtags);
                if (tmpBM)
                {
                    compdata->displaybitmap = HIDD_BM_OBJ(tmpBM);
                    newsdispbitmap = compdata->displaybitmap;
                    if (!newsdispbitmap)
                        ok = FALSE;
                }
            }
        }
        else
        {
            olddisplaybitmap = NULL;
        }

        if ((compdata->flags & COMPSTATEF_HASALPHA) && !(compdata->intermedbitmap) &&
            !(compdata->gpu.available))
        {
            /* Intermediate bitmap only needed for CPU fallback alpha compositing */
            tmpBM = AllocBitMap(
                compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                compdata->displayrect.MaxY - compdata->displayrect.MinY + 1,
                compdata->displaydepth,
                BMF_CHECKVALUE, (struct BitMap *)bmtags);
            if (tmpBM)
            {
                compdata->intermedbitmap = HIDD_BM_OBJ(tmpBM);
            }
        }
        else if (!(compdata->flags & COMPSTATEF_HASALPHA) && (compdata->intermedbitmap))
            compdata->intermedbitmap = NULL;

        if (ok)
            HIDDCompositorRedrawVisibleRegions(compdata, NULL);
    }
    else if (olddisplaybitmap || newtop)
    {
        newsdispbitmap = compdata->topbitmap;
        compdata->displaybitmap = NULL;
    }

    DTOGGLE(bug("[GLCompositor] %s: oldcompbm 0x%p, topbm 0x%p, dispbm 0x%p, newdispbm 0x%p\n",
            __func__, olddisplaybitmap, compdata->topbitmap,
            compdata->displaybitmap, newsdispbitmap));

    if (newsdispbitmap)
    {
        IPTR w, h;

        compdata->screenbitmap = HIDD_Gfx_Show(compdata->gfx, newsdispbitmap, fHidd_Gfx_Show_CopyBack);

        if (compdata->screenbitmap)
        {
            OOP_GetAttr(compdata->screenbitmap, aHidd_BitMap_Width, &w);
            OOP_GetAttr(compdata->screenbitmap, aHidd_BitMap_Height, &h);
            HIDD_BM_UpdateRect(compdata->screenbitmap, 0, 0, w, h);
        }
    }

    if (!(compdata->flags & COMPSTATEF_HASALPHA) && (oldintermedbitmap))
    {
        struct BitMap *freebm;
        OOP_GetAttr(oldintermedbitmap, aHidd_BitMap_BMStruct, (IPTR *)&freebm);
        if (freebm)
            FreeBitMap(freebm);
        else
            OOP_DisposeObject(oldintermedbitmap);
        compdata->intermedbitmap = NULL;
    }

    if (olddisplaybitmap && (olddisplaybitmap != compdata->fb))
    {
        struct BitMap *freebm;
        OOP_GetAttr(olddisplaybitmap, aHidd_BitMap_BMStruct, (IPTR *)&freebm);
        if (freebm)
            FreeBitMap(freebm);
        else
            OOP_DisposeObject(olddisplaybitmap);
    }

    compdata->modeschanged = FALSE;

    return ok;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Stack purge and reset                                                     */
/* ────────────────────────────────────────────────────────────────────────── */

static VOID HIDDCompositorPurgeBitMapStack(struct HIDDCompositorData *compdata)
{
    struct StackBitMapNode *curr, *next;

    ForeachNodeSafe(&compdata->bitmapstack, curr, next)
    {
        /* Clean up GPU texture for this node */
        if (compdata->gpu.available && compdata->gpu.gl_context)
        {
            glAMakeCurrent(compdata->gpu.gl_context);
            GPUFreeNodeTexture(curr);
        }

        if (curr->screenregion)
            DisposeRegion(curr->screenregion);

        FreeMem(curr, sizeof(struct StackBitMapNode));
    }

    NEWLIST(&compdata->bitmapstack);
}

static void HIDDCompositorShowSingle(struct HIDDCompositorData *compdata, OOP_Object *bm)
{
    compdata->topbitmap = bm;
    compdata->screenbitmap = HIDD_Gfx_Show(compdata->gfx, bm, fHidd_Gfx_Show_CopyBack);

    if (compdata->displaybitmap)
    {
        if (compdata->displaybitmap != compdata->fb)
            OOP_DisposeObject(compdata->displaybitmap);
        compdata->displaybitmap = NULL;
    }
}

static void HIDDCompositorReset(struct HIDDCompositorData *compdata)
{
    HIDDCompositorPurgeBitMapStack(compdata);

    compdata->displaymode = vHidd_ModeID_Invalid;
    compdata->screenbitmap = NULL;
    compdata->flags &= ~COMPSTATEF_HASALPHA;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* Config and Backfill                                                       */
/* ────────────────────────────────────────────────────────────────────────── */

VOID CompositorParseConfig(struct HIDDCompositorData *compdata)
{
    struct RDArgs *rdargs;
    struct Process *me;
    IPTR CompArgs[NOOFARGS] = { 0 };
    TEXT CompConfig[1024];
    APTR old_windowptr;
    int len;

    me = (struct Process *)FindTask(NULL);
    old_windowptr = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;

    compdata->capabilities = COMPF_ABOVE;

    rdargs = AllocDosObjectTags(DOS_RDARGS, TAG_END);
    if (rdargs != NULL)
    {
        if ((len = GetVar(COMPOSITOR_PREFS, CompConfig, 1024, GVF_GLOBAL_ONLY)) != -1)
        {
            rdargs->RDA_Source.CS_Buffer = CompConfig;
            rdargs->RDA_Source.CS_Length = len;
            rdargs->RDA_DAList = (IPTR)NULL;
            rdargs->RDA_Buffer = NULL;
            rdargs->RDA_BufSiz = 0;
            rdargs->RDA_ExtHelp = NULL;
            rdargs->RDA_Flags = 0;

            if (ReadArgs(COMPOSITOR_PEFSTEMPLATE, CompArgs, rdargs) != NULL)
            {
                if (CompArgs[ARG_ABOVE])
                    compdata->capabilities |= COMPF_ABOVE;
                else
                    compdata->capabilities &= ~COMPF_ABOVE;

                if (CompArgs[ARG_BELOW])
                    compdata->capabilities |= COMPF_BELOW;
                else
                    compdata->capabilities &= ~COMPF_BELOW;

                if (CompArgs[ARG_LEFT])
                    compdata->capabilities |= COMPF_LEFT;
                else
                    compdata->capabilities &= ~COMPF_LEFT;

                if (CompArgs[ARG_RIGHT])
                    compdata->capabilities |= COMPF_RIGHT;
                else
                    compdata->capabilities &= ~COMPF_RIGHT;

                if (CompArgs[ARG_ALPHA])
                    compdata->capabilities |= COMPF_ALPHA;
                else
                    compdata->capabilities &= ~COMPF_ALPHA;

                FreeArgs(rdargs);
            }
        }
        FreeDosObject(DOS_RDARGS, rdargs);
    }

    me->pr_WindowPtr = old_windowptr;
}

AROS_UFH3(void, CompositorDefaultBackFillFunc,
    AROS_UFHA(struct Hook *,               h,   A0),
    AROS_UFHA(struct BitMap *,             bm,  A2),
    AROS_UFHA(struct HIDD_BackFillHookMsg *, msg, A1))
{
    AROS_USERFUNC_INIT

    struct HIDDCompositorData *compdata = h->h_Data;

    HIDDCompositorFillRect(compdata, HIDD_BM_OBJ(bm),
        msg->bounds->MinX, msg->bounds->MinY,
        msg->bounds->MaxX, msg->bounds->MaxY);

    AROS_USERFUNC_EXIT
}

/* ════════════════════════════════════════════════════════════════════════ */
/* PUBLIC OOP METHODS                                                      */
/* ════════════════════════════════════════════════════════════════════════ */

OOP_Object *METHOD(Compositor, Root, New)
{
    D(bug("[GLCompositor] %s()\n", __func__));

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);

    if (o)
    {
        OOP_MethodID disposemid;
        struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);

        D(bug("[GLCompositor] %s: Compositor @ 0x%p, data @ 0x%p\n", __func__, o, compdata));

        CompositorParseConfig(compdata);

        compdata->capabilities = (ULONG)GetTagData(aHidd_Compositor_State,
            compdata->capabilities, msg->attrList);
        compdata->flags |= COMPSTATEF_DEEPLUT;

        compdata->displaymode = vHidd_ModeID_Invalid;

        NEWLIST(&compdata->bitmapstack);

        compdata->defaultbackfill.h_Entry = (HOOKFUNC)AROS_ASMSYMNAME(CompositorDefaultBackFillFunc);
        compdata->defaultbackfill.h_Data = compdata;
        compdata->backfillhook = &compdata->defaultbackfill;

        InitSemaphore(&compdata->semaphore);

        compdata->displayid = (ULONG)GetTagData(aHidd_Compositor_DisplayID, 0, msg->attrList);
        compdata->gfx = (OOP_Object *)GetTagData(aHidd_Compositor_GfxHidd, 0, msg->attrList);
        compdata->fb  = (OOP_Object *)GetTagData(aHidd_Compositor_FrameBuffer, 0, msg->attrList);

        D(bug("[GLCompositor] %s: DisplayID %08lx for Gfx Driver @ 0x%p\n",
              __func__, compdata->displayid, compdata->gfx));

        GfxBase = (APTR)OpenLibrary("graphics.library", 41);
        IntuitionBase = (APTR)OpenLibrary("intuition.library", 50);

        if ((compdata->GraphicsBase) && (compdata->gfx != NULL))
        {
            compdata->gc = HIDD_Gfx_CreateObject(compdata->gfx,
                OOP_FindClass(CLID_Hidd_GC), NULL);

            D(bug("[GLCompositor] %s: GC @ %p\n", __func__, compdata->gc));

            if ((compdata->gfx) && (compdata->gc))
            {
                /* Initialize GPU compositor */
                if (!InitGPUCompositor(compdata))
                {
                    D(bug("[GLCompositor] %s: GPU init failed, using CPU fallback\n", __func__));
                    /* Continue without GPU — CPU fallback will be used */
                }

                return o;
            }
        }

        disposemid = OOP_GetMethodID(IID_Root, moRoot_Dispose);
        OOP_CoerceMethod(cl, o, &disposemid);
    }

    return NULL;
}

void METHOD(Compositor, Root, Dispose)
{
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);

    D(bug("[GLCompositor] %s: HIDDCompositorData @ 0x%p\n", __func__, compdata));

    ShutdownGPUCompositor(compdata);

    OOP_DoSuperMethod(cl, o, &msg->mID);
}

VOID METHOD(Compositor, Root, Get)
{
    ULONG idx;
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);

    if (IS_COMPOSITOR_ATTR(msg->attrID, idx))
    {
        switch (idx)
        {
            case aoHidd_Compositor_Capabilities:
                *msg->storage = (IPTR)CAPABILITY_FLAGS;
                return;
            case aoHidd_Compositor_State:
                *msg->storage = (IPTR)(compdata->capabilities & CAPABILITY_FLAGS);
                return;
            case aoHidd_Compositor_BackFillHook:
                *msg->storage = (IPTR)compdata->backfillhook;
                return;
        }
    }
    OOP_DoSuperMethod(cl, o, &msg->mID);
}

VOID METHOD(Compositor, Root, Set)
{
    ULONG idx;
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);
    struct TagItem *tag, *tstate = msg->attrList;

    while ((tag = NextTagItem(&tstate)))
    {
        if (IS_COMPOSITOR_ATTR(tag->ti_Tag, idx))
        {
            switch (idx)
            {
                case aoHidd_Compositor_State:
                    compdata->capabilities = (ULONG)(tag->ti_Data & CAPABILITY_FLAGS);
                    break;
                case aoHidd_Compositor_BackFillHook:
                    if (tag->ti_Data)
                        compdata->backfillhook = (struct Hook *)tag->ti_Data;
                    else
                        compdata->backfillhook = &compdata->defaultbackfill;
                    break;
            }
        }
    }

    OOP_DoSuperMethod(cl, o, &msg->mID);
}

OOP_Object *METHOD(Compositor, Hidd_Compositor, BitMapStackChanged)
{
    struct HIDD_ViewPortData *vpdata;
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);
    struct StackBitMapNode *n;
    struct Screen *bmScreen;
    OOP_Object *bmpxfmt;
    int bmstdfmt;
    BOOL newtop = FALSE;
    BOOL ok = TRUE;

    DSTACK(bug("[GLCompositor] %s: Top bitmap: 0x%lx\n", __func__, msg->data->Bitmap));

    LOCK_COMPOSITOR_WRITE

    HIDDCompositorPurgeBitMapStack(compdata);

    if (!msg->data)
    {
        UNLOCK_COMPOSITOR

        HIDDCompositorShowSingle(compdata, NULL);

        *msg->active = FALSE;
        return compdata->screenbitmap;
    }

    for (vpdata = msg->data; vpdata; vpdata = vpdata->Next)
    {
        n = AllocMem(sizeof(struct StackBitMapNode), MEMF_ANY | MEMF_CLEAR);
        if (!n)
        {
            DSTACK(bug("[GLCompositor] %s: Error allocating StackBitMapNode!!!\n", __func__));
            ok = FALSE;
            break;
        }

        n->bm       = vpdata->Bitmap;
        n->sbmflags = STACKNODEF_DISPLAYABLE;
        n->leftedge = vpdata->vpe->ViewPort->DxOffset;
        n->topedge  = vpdata->vpe->ViewPort->DyOffset;

        n->screenregion = NewRegion();

        /* GPU fields are zero from MEMF_CLEAR */
        n->gpu.needs_upload = TRUE;

        if ((bmScreen = HIDDCompositorFindBitMapScreen(compdata, n->bm)) != NULL)
        {
            DSTACK(bug("[GLCompositor] %s: Screen @ 0x%p\n", __func__, bmScreen));
            GetAttr(SA_CompositingFlags, (Object *)bmScreen, &n->sbmflags);
            n->sbmflags |= STACKNODEF_DISPLAYABLE;
            if (n->sbmflags & COMPF_ALPHA)
            {
                GetAttr(SA_AlphaPreCompositingHook, (Object *)bmScreen, (IPTR *)&n->prealphacomphook);
            }
        }

        if (n->sbmflags & COMPF_ALPHA)
        {
            bmpxfmt = (OOP_Object *)OOP_GET(n->bm, aHidd_BitMap_PixFmt);
            bmstdfmt = (int)OOP_GET(bmpxfmt, aHidd_PixFmt_StdPixFmt);

            switch (bmstdfmt)
            {
                case vHidd_StdPixFmt_ARGB32:
                case vHidd_StdPixFmt_BGRA32:
                case vHidd_StdPixFmt_RGBA32:
                case vHidd_StdPixFmt_ABGR32:
                    compdata->flags |= COMPSTATEF_HASALPHA;
                    break;
                default:
                    n->sbmflags &= ~COMPF_ALPHA;
                    break;
            }
        }

        if (!(n->sbmflags & COMPF_ALPHA))
        {
            if (((BOOL)OOP_GET(n->bm, aHidd_BitMap_Displayable)) != TRUE)
                n->sbmflags &= ~STACKNODEF_DISPLAYABLE;
        }

        AddTail((struct List *)&compdata->bitmapstack, (struct Node *)n);
    }

    UpdateDisplayMode(compdata);

    if (msg->data->Bitmap != compdata->topbitmap)
    {
        compdata->topbitmap = msg->data->Bitmap;
        newtop = TRUE;
    }

    if (ok)
    {
        ForeachNode(&compdata->bitmapstack, n)
        {
            HIDDCompositorValidateBitMapPositionChange(n->bm, &n->leftedge, &n->topedge,
                compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);
        }

        ok = HIDDCompositorToggleCompositing(compdata, newtop);
    }

    if (!ok)
    {
        HIDDCompositorReset(compdata);
        HIDDCompositorShowSingle(compdata, msg->data->Bitmap);
    }

    UNLOCK_COMPOSITOR

    *msg->active = compdata->displaybitmap ? TRUE : FALSE;
    return compdata->screenbitmap;
}

VOID METHOD(Compositor, Hidd_Compositor, BitMapRectChanged)
{
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);

    if (compdata->displaybitmap)
    {
        struct StackBitMapNode *n;

        DUPDATE(bug("[GLCompositor] %s: Bitmap 0x%p\n", __func__, msg->bm));

        LOCK_COMPOSITOR_READ

        n = HIDDCompositorFindBitMapStackNode(compdata, msg->bm);
        if (n && (n->sbmflags & STACKNODEF_VISIBLE))
        {
            struct Rectangle srcrect;

            srcrect.MinX = n->leftedge + msg->x;
            srcrect.MinY = n->topedge + msg->y;
            srcrect.MaxX = srcrect.MinX + msg->width - 1;
            srcrect.MaxY = srcrect.MinY + msg->height - 1;

            /* Mark texture as needing re-upload */
            n->gpu.needs_upload = TRUE;

            if (compdata->gpu.available)
            {
                /* GPU path: just re-render the affected area */
                GPUCompositorRedrawVisibleRegions(compdata, &srcrect);
            }
            else
            {
                /* CPU fallback path */
                OOP_Object *renderTarget = compdata->displaybitmap;
                if (compdata->intermedbitmap)
                    renderTarget = compdata->intermedbitmap;

                struct RegionRectangle *srrect = n->screenregion->RegionRectangle;
                while (srrect)
                {
                    BOOL updateAlphaBmps = FALSE;
                    struct Rectangle dstandvisrect;

                    dstandvisrect.MinX = srrect->bounds.MinX + n->screenregion->bounds.MinX;
                    dstandvisrect.MinY = srrect->bounds.MinY + n->screenregion->bounds.MinY;
                    dstandvisrect.MaxX = srrect->bounds.MaxX + n->screenregion->bounds.MinX;
                    dstandvisrect.MaxY = srrect->bounds.MaxY + n->screenregion->bounds.MinY;

                    if (AndRectRect(&srcrect, &dstandvisrect, &dstandvisrect))
                    {
                        if (!(n->sbmflags & COMPF_ALPHA))
                        {
                            if ((compdata->alpharegion) && (isRectInRegion(compdata->alpharegion, &dstandvisrect)))
                                updateAlphaBmps = TRUE;
                            CPUCompositorRedrawBitmap(compdata, renderTarget, n, &dstandvisrect);
                        }
                        else
                        {
                            CPUCompositorRedrawVisibleRegions(compdata, &dstandvisrect);
                        }

                        if (updateAlphaBmps)
                            CPUCompositorRedrawAlphaRegions(compdata, &dstandvisrect);

                        if (renderTarget != compdata->displaybitmap)
                        {
                            HIDD_Gfx_CopyBox(compdata->gfx, renderTarget,
                                dstandvisrect.MinX, dstandvisrect.MinY,
                                compdata->displaybitmap,
                                dstandvisrect.MinX, dstandvisrect.MinY,
                                dstandvisrect.MaxX - dstandvisrect.MinX + 1,
                                dstandvisrect.MaxY - dstandvisrect.MinY + 1,
                                compdata->gc);
                        }
                    }
                    srrect = srrect->Next;
                }
                HIDD_BM_UpdateRect(compdata->displaybitmap,
                    srcrect.MinX, srcrect.MinY,
                    srcrect.MaxX - srcrect.MinX + 1,
                    srcrect.MaxY - srcrect.MinY + 1);
            }
        }

        UNLOCK_COMPOSITOR
    }
    else
    {
        /* Passthrough mode */
        HIDD_BM_UpdateRect(msg->bm, msg->x, msg->y, msg->width, msg->height);
    }
}

IPTR METHOD(Compositor, Hidd_Compositor, BitMapPositionChange)
{
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);
    struct StackBitMapNode *n;
    IPTR disp_width, disp_height;

    LOCK_COMPOSITOR_READ

    n = HIDDCompositorFindBitMapStackNode(compdata, msg->bm);
    if (n)
    {
        disp_width  = compdata->displayrect.MaxX + 1;
        disp_height = compdata->displayrect.MaxY + 1;
    }
    else
    {
        HIDDT_ModeID modeid = vHidd_ModeID_Invalid;
        OOP_Object *bmfriend, *sync, *pf;

        OOP_GetAttr(msg->bm, aHidd_BitMap_ModeID, &modeid);

        if ((modeid == vHidd_ModeID_Invalid) && (OOP_GET(msg->bm, aHidd_BitMap_Compositable)))
        {
            OOP_GetAttr(msg->bm, aHidd_BitMap_Friend, (IPTR *)&bmfriend);
            if (bmfriend)
                OOP_GetAttr(bmfriend, aHidd_BitMap_ModeID, &modeid);
        }

        if (modeid == vHidd_ModeID_Invalid)
        {
            UNLOCK_COMPOSITOR
            return FALSE;
        }

        HIDD_Gfx_GetMode(compdata->gfx, modeid, &sync, &pf);
        OOP_GetAttr(sync, aHidd_Sync_HDisp, &disp_width);
        OOP_GetAttr(sync, aHidd_Sync_VDisp, &disp_height);
    }

    DMOVE(bug("[GLCompositor] %s: Validating bitmap 0x%p, position (%ld, %ld), limits %ld x %ld\n",
              __func__, msg->bm, *msg->newxoffset, *msg->newyoffset, disp_width, disp_height));

    HIDDCompositorValidateBitMapPositionChange(msg->bm, msg->newxoffset, msg->newyoffset,
                                               disp_width, disp_height);

    if (n && ((*msg->newxoffset != n->leftedge) || (*msg->newyoffset != n->topedge)))
    {
        n->leftedge = *msg->newxoffset;
        n->topedge  = *msg->newyoffset;

        if (compdata->topbitmap == msg->bm)
        {
            HIDDCompositorToggleCompositing(compdata, FALSE);
        }
        else
            HIDDCompositorRedrawVisibleRegions(compdata, NULL);
    }

    UNLOCK_COMPOSITOR

    return compdata->displaybitmap ? TRUE : FALSE;
}

IPTR METHOD(Compositor, Hidd_Compositor, BitMapValidate)
{
    if (IS_HIDD_BM(msg->bm))
        return TRUE;
    return FALSE;
}

IPTR METHOD(Compositor, Hidd_Compositor, BitMapEnable)
{
    if (IS_HIDD_BM(msg->bm))
    {
        if (!(OOP_GET(HIDD_BM_OBJ(msg->bm), aHidd_BitMap_Displayable)))
        {
            struct TagItem composittags[] = {
                { aHidd_BitMap_Compositable, TRUE },
                { TAG_DONE, 0 }
            };

            D(bug("[GLCompositor] %s: Marking BitMap 0x%lx as Compositable\n", __func__, msg->bm));
            OOP_SetAttrs(HIDD_BM_OBJ(msg->bm), composittags);
        }
        return TRUE;
    }
    return FALSE;
}

/* ────────────────────────────────────────────────────────────────────────── */
/* OOP Interface Descriptors                                                 */
/* ────────────────────────────────────────────────────────────────────────── */

#define NUM_Compositor_Root_METHODS 4

static const struct OOP_MethodDescr Compositor_Root_descr[] =
{
    { (OOP_MethodFunc)Compositor__Root__New,     moRoot_New     },
    { (OOP_MethodFunc)Compositor__Root__Dispose, moRoot_Dispose },
    { (OOP_MethodFunc)Compositor__Root__Get,     moRoot_Get     },
    { (OOP_MethodFunc)Compositor__Root__Set,     moRoot_Set     },
    { NULL, 0 }
};

#define NUM_Compositor_Hidd_Compositor_METHODS 5

static const struct OOP_MethodDescr Compositor_Hidd_Compositor_descr[] =
{
    { (OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapStackChanged,   moHidd_Compositor_BitMapStackChanged   },
    { (OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapRectChanged,    moHidd_Compositor_BitMapRectChanged    },
    { (OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapPositionChange, moHidd_Compositor_BitMapPositionChange },
    { (OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapValidate,       moHidd_Compositor_BitMapValidate       },
    { (OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapEnable,         moHidd_Compositor_BitMapEnable         },
    { NULL, 0 }
};

const struct OOP_InterfaceDescr Compositor_ifdescr[] =
{
    { Compositor_Root_descr,            IID_Root,            NUM_Compositor_Root_METHODS            },
    { Compositor_Hidd_Compositor_descr, IID_Hidd_Compositor, NUM_Compositor_Hidd_Compositor_METHODS },
    { NULL, NULL }
};
