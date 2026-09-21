/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    glblitbench: benchmark the 2D present path of the GL driver, i.e. what an
    emulator does per frame: upload a WxH texture (glTexSubImage2D), draw it
    as one textured quad, swap. Each phase is timed separately (avg / max /
    p99 in microseconds) so upload cost, GPU submit and swap pacing can be
    told apart.

    Usage: glblitbench W/N,H/N,BPP/N,FRAMES/N,DIRTY/N,SCALE/N,
                       NOUPLOAD/S,NODRAW/S,FINISH/S,FULLSCREEN/S
      W,H        texture size (default 376x287, an A500 lores frame)
      BPP        16 (RGB565) or 32 (RGBA8888, default)
      FRAMES     frames to measure (default 500)
      DIRTY      rows uploaded per frame (default 0 = all rows)
      SCALE      window size = texture size * SCALE (default 2)
      NOUPLOAD   skip glTexSubImage2D (draw + swap only)
      NODRAW     skip the quad (upload + swap only)
      FINISH     glFinish() after the draw, so GPU render time shows up as
                 its own phase instead of hiding in the next upload/swap
      FULLSCREEN open a custom screen of window size (flip path)
*/

#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include <GL/gla.h>
#include <GL/glext.h>

#include <proto/oop.h>
#include <hidd/gfx.h>

struct Library *OOPBase;

/* Mirrored from the driver-internal vcgfx_bitmap.h, the way vc4gallium
 * mirrors it. Milestone 0 only: the real path will be a cgxvideo overlay
 * bitmap object created through moHidd_Gfx_CreateObject. */
#define IID_Hidd_BitMap_VideoCore4  "hidd.bitmap.bcmvc4"
#define aoHidd_VC4BM_Overlay        3
#define aoHidd_VC4BM_LatchWait      4
#define VC4GFX_OVL_NOWAIT           (1 << 0)

struct vc4gfx_overlay
{
    ULONG ovl_Phys;
    ULONG ovl_Pitch;
    ULONG ovl_Width, ovl_Height;
    LONG  ovl_X, ovl_Y;
    ULONG ovl_DestW, ovl_DestH;
    ULONG ovl_Flags;
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

CONST_STRPTR version = "$VER: glblitbench 1.0 (12.09.2026) AROS";

/* Buffer objects are an extension here, so they come through
 * glAGetProcAddress() rather than the library's export table. */
static PFNGLGENBUFFERSPROC    xglGenBuffers;
static PFNGLBINDBUFFERPROC    xglBindBuffer;
static PFNGLBUFFERDATAPROC    xglBufferData;
static PFNGLBUFFERSUBDATAPROC xglBufferSubData;
static PFNGLDELETEBUFFERSPROC xglDeleteBuffers;

static BOOL resolve_buffer_entrypoints(void)
{
    xglGenBuffers    = (PFNGLGENBUFFERSPROC)glAGetProcAddress("glGenBuffers");
    xglBindBuffer    = (PFNGLBINDBUFFERPROC)glAGetProcAddress("glBindBuffer");
    xglBufferData    = (PFNGLBUFFERDATAPROC)glAGetProcAddress("glBufferData");
    xglBufferSubData = (PFNGLBUFFERSUBDATAPROC)glAGetProcAddress("glBufferSubData");
    xglDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glAGetProcAddress("glDeleteBuffers");
    return xglGenBuffers && xglBindBuffer && xglBufferData &&
           xglBufferSubData && xglDeleteBuffers;
}

struct phase
{
    const char *name;
    ULONG *samples;
    ULONG  count;
    ULONG  sum, max;
};

static ULONG now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ULONG)(ts.tv_sec * 1000000UL + ts.tv_nsec / 1000);
}

static void phase_add(struct phase *p, ULONG us)
{
    p->samples[p->count++] = us;
    p->sum += us;
    if (us > p->max)
        p->max = us;
}

static int cmp_ulong(const void *a, const void *b)
{
    ULONG x = *(const ULONG *)a, y = *(const ULONG *)b;
    return (x > y) - (x < y);
}

static void phase_report(struct phase *p)
{
    ULONG p99 = 0;
    if (!p->count)
        return;
    qsort(p->samples, p->count, sizeof(ULONG), cmp_ulong);
    p99 = p->samples[(p->count * 99) / 100];
    printf("  %-8s avg %6lu us  p99 %6lu us  max %6lu us\n",
           p->name, (unsigned long)(p->sum / p->count),
           (unsigned long)p99, (unsigned long)p->max);
}

int main(void)
{
    IPTR args[15] = { 0 };
    struct RDArgs *rda;
    LONG w = 376, h = 287, bpp = 32, frames = 500, dirty = 0, scale = 2;
    BOOL noupload, nodraw, finish, fullscreen, rgb8, pad, vbo, overlay;
    GLuint buf[8] = { 0 };
    struct Screen *scr = NULL;
    struct Window *win = NULL;
    GLAContext ctx = NULL;
    GLuint tex[8] = { 0 };
    LONG ring = 1, texh;
    GLenum fmt, type, internalfmt;
    UBYTE *src, *scratch;
    ULONG bytes, rowbytes, f;
    struct phase ph_upload = { "upload" }, ph_draw = { "draw" },
                 ph_finish = { "finish" }, ph_swap = { "swap" },
                 ph_frame = { "frame" };
    ULONG t_start, t_end;
    int ret = 1;

    rda = ReadArgs("W/N,H/N,BPP/N,FRAMES/N,DIRTY/N,SCALE/N,NOUPLOAD/S,NODRAW/S,FINISH/S,FULLSCREEN/S,RGB8/S,RING/N,PAD/S,VBO/S,OVERLAY/S",
                   args, NULL);
    if (args[0]) w      = *(LONG *)args[0];
    if (args[1]) h      = *(LONG *)args[1];
    if (args[2]) bpp    = *(LONG *)args[2];
    if (args[3]) frames = *(LONG *)args[3];
    if (args[4]) dirty  = *(LONG *)args[4];
    if (args[5]) scale  = *(LONG *)args[5];
    noupload   = args[6] != 0;
    nodraw     = args[7] != 0;
    finish     = args[8] != 0;
    fullscreen = args[9] != 0;
    rgb8       = args[10] != 0;
    if (args[11]) ring = *(LONG *)args[11];
    pad        = args[12] != 0;
    vbo        = args[13] != 0;
    overlay    = args[14] != 0;
    if (ring < 1) ring = 1;
    if (ring > 8) ring = 8;
    if (rda)
        FreeArgs(rda);

    if (bpp != 16 && bpp != 32) bpp = 32;
    if (frames < 10) frames = 10;
    if (scale < 1) scale = 1;
    if (dirty <= 0 || dirty > h) dirty = h;

    fmt  = (bpp == 16) ? GL_RGB : GL_RGBA;
    type = (bpp == 16) ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE;
    /* GL_RGB as internalformat yields an 8888 texture, so every 565 upload is
     * converted per pixel on the CPU. Ask for a real 565 texture instead. */
    internalfmt = (bpp == 16) ? (rgb8 ? (GLenum)GL_RGB : (GLenum)GL_RGB565)
                              : (GLenum)GL_RGBA;
    rowbytes = w * bpp / 8;
    bytes    = rowbytes * h;

    src     = malloc(bytes);
    scratch = malloc(bytes);
    ph_upload.samples = malloc(frames * sizeof(ULONG));
    ph_draw.samples   = malloc(frames * sizeof(ULONG));
    ph_finish.samples = malloc(frames * sizeof(ULONG));
    ph_swap.samples   = malloc(frames * sizeof(ULONG));
    ph_frame.samples  = malloc(frames * sizeof(ULONG));
    if (!src || !scratch || !ph_upload.samples || !ph_draw.samples ||
        !ph_finish.samples || !ph_swap.samples || !ph_frame.samples)
    {
        printf("out of memory\n");
        goto done;
    }
    for (f = 0; f < bytes; f++)
        src[f] = (UBYTE)(f * 7);

    /* CPU baseline: plain cached memcpy of one frame, best of 5. The sink
     * keeps the compiler from eliminating a copy nothing reads back. */
    {
        ULONG best = (ULONG)~0U, i;
        volatile UBYTE sink = 0;
        for (i = 0; i < 5; i++)
        {
            ULONG t0 = now_us();
            memcpy(scratch, src, bytes);
            ULONG dt = now_us() - t0;
            sink += scratch[i * 977 % bytes];
            if (dt < best) best = dt;
        }
        (void)sink;
        printf("glblitbench: %ldx%ld %ld bpp = %lu KB/frame, dirty rows %ld, "
               "window x%ld%s%s%s%s\n",
               (long)w, (long)h, (long)bpp, (unsigned long)(bytes / 1024), (long)dirty, (long)scale,
               fullscreen ? ", fullscreen" : "", noupload ? ", no upload" : "",
               nodraw ? ", no draw" : "", finish ? ", glFinish" : "");
        if (vbo)
            printf("  uploading into a VBO (untiled) instead of a texture\n");
        if (ring > 1 || pad)
            printf("  ring of %ld texture(s)%s\n", (long)ring,
                   pad ? ", padded by one row (forces the direct tiled store)" : "");
        printf("  memcpy baseline: %lu us/frame (%lu MB/s cached RAM->RAM)\n",
               (unsigned long)best,
               best ? (unsigned long)((UQUAD)bytes / best) : 0UL);
    }

    if (fullscreen)
    {
        scr = OpenScreenTags(NULL,
            SA_Width,     w * scale,
            SA_Height,    h * scale,
            SA_Depth,     24,
            SA_Quiet,     TRUE,
            SA_ShowTitle, FALSE,
            SA_Title,     (IPTR)"glblitbench",
            TAG_DONE);
        if (!scr)
        {
            printf("OpenScreen %ldx%ld failed\n", (long)(w * scale), (long)(h * scale));
            goto done;
        }
        win = OpenWindowTags(NULL,
            WA_Left,         0,
            WA_Top,          0,
            WA_InnerWidth,   scr->Width,
            WA_InnerHeight,  scr->Height,
            WA_CustomScreen, (IPTR)scr,
            WA_Flags,        WFLG_ACTIVATE | WFLG_BACKDROP | WFLG_BORDERLESS | WFLG_RMBTRAP,
            WA_IDCMP,        IDCMP_VANILLAKEY,
            TAG_DONE);
    }
    else
    {
        win = OpenWindowTags(NULL,
            WA_Title,         (IPTR)"glblitbench",
            WA_PubScreen,     NULL,
            WA_CloseGadget,   TRUE,
            WA_DragBar,       TRUE,
            WA_DepthGadget,   TRUE,
            WA_Left,          50,
            WA_Top,           50,
            WA_InnerWidth,    w * scale,
            WA_InnerHeight,   h * scale,
            WA_Activate,      TRUE,
            WA_RMBTrap,       TRUE,
            WA_SimpleRefresh, TRUE,
            WA_NoCareRefresh, TRUE,
            WA_IDCMP,         IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW,
            TAG_DONE);
    }
    if (!win)
    {
        printf("OpenWindow failed\n");
        goto done;
    }

    if (overlay)
    {
        /* Milestone 0: no GL at all. Write the frame linearly into VideoCore
         * memory and hand it to the HVS as a plane. AllocMem(MEMF_CHIP)
         * reaches the pool vcgfx registers with exec, and that memory is
         * identity-mapped, so the pointer is the physical address the HVS
         * wants. */
        OOP_Object *bm_obj;
        OOP_AttrBase vc4ab;
        UBYTE *vcbuf[8] = { NULL };
        struct phase ph_write = { "write" }, ph_present = { "present" },
                     ph_wait = { "wait" };
        LONG absX, absY;
        ULONG i;
        int oret = 1;

        ph_write.samples   = malloc(frames * sizeof(ULONG));
        ph_present.samples = malloc(frames * sizeof(ULONG));
        ph_wait.samples    = malloc(frames * sizeof(ULONG));
        if (!ph_write.samples || !ph_present.samples || !ph_wait.samples)
            goto ovl_done;

        OOPBase = OpenLibrary("oop.library", 0);
        if (!OOPBase)
        {
            printf("  cannot open oop.library\n");
            goto ovl_done;
        }
        vc4ab = OOP_ObtainAttrBase((STRPTR)IID_Hidd_BitMap_VideoCore4);
        if (!vc4ab)
        {
            printf("  no %s attribute base: this is not a VideoCore screen\n",
                   IID_Hidd_BitMap_VideoCore4);
            goto ovl_done;
        }
        bm_obj = HIDD_BM_OBJ(win->WScreen->RastPort.BitMap);

        for (i = 0; i < (ULONG)ring; i++)
        {
            vcbuf[i] = AllocMem(bytes, MEMF_CHIP);
            if (!vcbuf[i])
            {
                printf("  AllocMem(%lu, MEMF_CHIP) failed\n",
                       (unsigned long)bytes);
                goto ovl_done;
            }
            memcpy(vcbuf[i], src, bytes);
        }
        printf("  overlay buffers at %p", vcbuf[0]);
        for (i = 1; i < (ULONG)ring; i++)
            printf(", %p", vcbuf[i]);
        printf(" (VideoCore RAM sits high; a low address means this landed in "
               "ordinary cached RAM and the numbers are not comparable)\n");

        /* Colour bars, so the byte order the HVS expects can be read off the
         * screen. Left to right the bars should be red, green, blue, white.
         * Any other order means the plane wants a different component order
         * than this 32-bit little-endian 0xAARRGGBB fill. */
        if (bpp == 32)
        {
            ULONG y, x;
            for (y = 0; y < (ULONG)h; y++)
            {
                ULONG *row = (ULONG *)(src + y * rowbytes);
                for (x = 0; x < (ULONG)w; x++)
                {
                    switch ((x * 4) / w)
                    {
                    case 0:  row[x] = 0xffff0000; break;
                    case 1:  row[x] = 0xff00ff00; break;
                    case 2:  row[x] = 0xff0000ff; break;
                    default: row[x] = 0xffffffff; break;
                    }
                }
            }
        }
        else
            printf("  note: the HVS overlay entry is hardcoded to 8888 today, "
                   "so BPP=16 will not display correctly yet\n");

        absX = win->LeftEdge + win->BorderLeft;
        absY = win->TopEdge + win->BorderTop;

        t_start = now_us();
        for (f = 0; f < (ULONG)frames; f++)
        {
            struct vc4gfx_overlay desc;
            struct TagItem ovltags[2];
            IPTR active = 0, dummy = 0;
            ULONG t0, t1, t2, t3;
            BOOL quit = FALSE;
            struct IntuiMessage *msg;

            t0 = now_us();
            /* The buffer we are about to write may still have been on screen
             * until the last update latched. */
            if (f >= (ULONG)ring)
                OOP_GetAttr(bm_obj, vc4ab + aoHidd_VC4BM_LatchWait, &dummy);
            t1 = now_us();

            memset(src + (f % h) * rowbytes, (UBYTE)f, rowbytes);
            memcpy(vcbuf[f % ring], src, bytes);
            t2 = now_us();

            desc.ovl_Phys   = (ULONG)(IPTR)vcbuf[f % ring];
            desc.ovl_Pitch  = rowbytes;
            desc.ovl_Width  = w;
            desc.ovl_Height = h;
            desc.ovl_X      = absX;
            desc.ovl_Y      = absY;
            desc.ovl_DestW  = w * scale;
            desc.ovl_DestH  = h * scale;
            desc.ovl_Flags  = VC4GFX_OVL_NOWAIT;

            ovltags[0].ti_Tag  = vc4ab + aoHidd_VC4BM_Overlay;
            ovltags[0].ti_Data = (IPTR)&desc;
            ovltags[1].ti_Tag  = TAG_DONE;
            OOP_SetAttrs(bm_obj, ovltags);
            OOP_GetAttr(bm_obj, vc4ab + aoHidd_VC4BM_Overlay, &active);
            t3 = now_us();

            if (!active)
            {
                printf("  the overlay was refused (scaled desktop, firmware "
                       "owns the display, or the window is obscured)\n");
                break;
            }

            phase_add(&ph_wait,    t1 - t0);
            phase_add(&ph_write,   t2 - t1);
            phase_add(&ph_present, t3 - t2);
            phase_add(&ph_frame,   t3 - t0);

            while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
            {
                if (msg->Class == IDCMP_CLOSEWINDOW ||
                    (msg->Class == IDCMP_VANILLAKEY && msg->Code == 27))
                    quit = TRUE;
                ReplyMsg((struct Message *)msg);
            }
            if (quit)
                break;
        }
        t_end = now_us();

        if (ph_frame.count)
        {
            printf("  %lu frames in %lu ms = %lu fps (%lu us/frame wall)\n",
                   (unsigned long)ph_frame.count,
                   (unsigned long)((t_end - t_start) / 1000),
                   (unsigned long)((UQUAD)ph_frame.count * 1000000 / (t_end - t_start)),
                   (unsigned long)((t_end - t_start) / ph_frame.count));
            phase_report(&ph_wait);
            phase_report(&ph_write);
            phase_report(&ph_present);
            phase_report(&ph_frame);
            if (ph_write.sum)
                printf("  linear write into VideoCore RAM: %lu MB/s\n",
                       (unsigned long)((UQUAD)bytes * ph_write.count / ph_write.sum));
            oret = 0;
        }

        /* Take the plane down before the buffers go away. */
        {
            struct TagItem clr[2];
            clr[0].ti_Tag  = vc4ab + aoHidd_VC4BM_Overlay;
            clr[0].ti_Data = 0;
            clr[1].ti_Tag  = TAG_DONE;
            OOP_SetAttrs(bm_obj, clr);
        }

ovl_done:
        for (i = 0; i < 8; i++)
            if (vcbuf[i])
                FreeMem(vcbuf[i], bytes);
        free(ph_write.samples);
        free(ph_present.samples);
        free(ph_wait.samples);
        if (OOPBase)
            CloseLibrary(OOPBase);
        ret = oret;
        goto done;
    }

    {
        struct TagItem attrs[10];
        int i = 0;
        attrs[i].ti_Tag = GLA_Window;    attrs[i++].ti_Data = (IPTR)win;
        attrs[i].ti_Tag = GLA_Left;      attrs[i++].ti_Data = win->BorderLeft;
        attrs[i].ti_Tag = GLA_Top;       attrs[i++].ti_Data = win->BorderTop;
        attrs[i].ti_Tag = GLA_Bottom;    attrs[i++].ti_Data = win->BorderBottom;
        attrs[i].ti_Tag = GLA_Right;     attrs[i++].ti_Data = win->BorderRight;
        attrs[i].ti_Tag = GLA_DoubleBuf; attrs[i++].ti_Data = GL_TRUE;
        attrs[i].ti_Tag = GLA_RGBMode;   attrs[i++].ti_Data = GL_TRUE;
        attrs[i].ti_Tag = GLA_NoStencil; attrs[i++].ti_Data = GL_TRUE;
        attrs[i].ti_Tag = GLA_NoAccum;   attrs[i++].ti_Data = GL_TRUE;
        attrs[i].ti_Tag = TAG_DONE;
        ctx = glACreateContext(attrs);
    }
    if (!ctx)
    {
        printf("glACreateContext failed\n");
        goto done;
    }
    glAMakeCurrent(ctx);
    printf("  GL: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    glViewport(0, 0, w * scale, h * scale);
    /* PAD makes the texture one row taller than the frame, so a full-frame
     * upload is still a SUB-rectangle: Mesa then skips the
     * DISCARD_WHOLE_RESOURCE path (staging copy + fresh BO) and stores
     * straight into the tiled BO. RING cycles textures so that direct store
     * never lands on the texture the GPU is still reading. */
    texh = pad ? h + 1 : h;
    glGenTextures(ring, tex);
    for (f = 0; f < (ULONG)ring; f++)
    {
        glBindTexture(GL_TEXTURE_2D, tex[f]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, internalfmt, w, texh, 0, fmt, type, NULL);
    }
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    if (vbo)
    {
        /* Mesa never tiles PIPE_BUFFER, so a buffer upload is the same write
         * into the same kind of uncached BO, only linear. It isolates the
         * cost of the tiling scatter from the cost of uncached memory. */
        if (!resolve_buffer_entrypoints())
        {
            printf("  buffer entry points unavailable, staying on the texture path\n");
            vbo = FALSE;
        }
        else
        {
        xglGenBuffers(ring, buf);
        for (f = 0; f < (ULONG)ring; f++)
        {
            xglBindBuffer(GL_ARRAY_BUFFER, buf[f]);
            xglBufferData(GL_ARRAY_BUFFER, bytes + 4096, NULL, GL_STREAM_DRAW);
        }
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internalfmt, w, h, 0, fmt, type, src);
    if (glGetError() != GL_NO_ERROR && internalfmt != fmt)
    {
        printf("  internalformat 0x%04x rejected, falling back to 0x%04x\n",
               (unsigned)internalfmt, (unsigned)fmt);
        internalfmt = fmt;
        glTexImage2D(GL_TEXTURE_2D, 0, internalfmt, w, h, 0, fmt, type, src);
    }
    printf("  texture internalformat 0x%04x, upload format 0x%04x\n",
           (unsigned)internalfmt, (unsigned)fmt);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /* Same fixed-function setup as amiberry's direct renderer. */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    /* Warm up: first frames allocate pages and take the entry path. */
    for (f = 0; f < 5; f++)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glASwapBuffers(ctx);
    }

    t_start = now_us();
    for (f = 0; f < (ULONG)frames; f++)
    {
        ULONG t0 = now_us(), t1, t2, t3, t4;
        BOOL quit = FALSE;
        struct IntuiMessage *msg;

        /* Touch the source so the upload can't be skipped/cached: shift
         * one byte per frame in the dirty region. */
        memset(src + (f % h) * rowbytes, (UBYTE)f, rowbytes);

        if (ring > 1)
            glBindTexture(GL_TEXTURE_2D, tex[f % ring]);
        if (!noupload)
        {
            if (vbo)
            {
                xglBindBuffer(GL_ARRAY_BUFFER, buf[f % ring]);
                xglBufferSubData(GL_ARRAY_BUFFER, 0, rowbytes * dirty, src);
            }
            else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, dirty, fmt, type, src);
        }
        t1 = now_us();

        if (!nodraw)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glBegin(GL_TRIANGLE_FAN);
            glTexCoord2f(0, 1); glVertex2f(-1, -1);
            glTexCoord2f(1, 1); glVertex2f( 1, -1);
            glTexCoord2f(1, 0); glVertex2f( 1,  1);
            glTexCoord2f(0, 0); glVertex2f(-1,  1);
            glEnd();
            glDisable(GL_TEXTURE_2D);
        }
        t2 = now_us();

        if (finish)
            glFinish();
        t3 = now_us();

        glASwapBuffers(ctx);
        t4 = now_us();

        phase_add(&ph_upload, t1 - t0);
        phase_add(&ph_draw,   t2 - t1);
        phase_add(&ph_finish, t3 - t2);
        phase_add(&ph_swap,   t4 - t3);
        phase_add(&ph_frame,  t4 - t0);

        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            if (msg->Class == IDCMP_CLOSEWINDOW ||
                (msg->Class == IDCMP_VANILLAKEY && msg->Code == 27))
                quit = TRUE;
            ReplyMsg((struct Message *)msg);
        }
        if (quit)
            break;
    }
    t_end = now_us();

    printf("  %lu frames in %lu ms = %lu.%lu fps (%lu us/frame wall)\n",
           (unsigned long)ph_frame.count, (unsigned long)((t_end - t_start) / 1000),
           (unsigned long)((UQUAD)ph_frame.count * 1000000 / (t_end - t_start)),
           (unsigned long)(((UQUAD)ph_frame.count * 10000000 / (t_end - t_start)) % 10),
           (unsigned long)((t_end - t_start) / ph_frame.count));
    phase_report(&ph_upload);
    phase_report(&ph_draw);
    if (finish)
        phase_report(&ph_finish);
    phase_report(&ph_swap);
    phase_report(&ph_frame);
    if (!noupload && ph_upload.sum)
        printf("  upload bandwidth: %lu MB/s into the texture\n",
               (unsigned long)((UQUAD)rowbytes * dirty * ph_upload.count / ph_upload.sum));
    {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
            printf("  GL error 0x%04x\n", (unsigned)err);
    }
    ret = 0;

done:
    if (tex[0])
        glDeleteTextures(ring, tex);
    if (buf[0] && xglDeleteBuffers)
        xglDeleteBuffers(ring, buf);
    if (ctx)
        glADestroyContext(ctx);
    if (win)
        CloseWindow(win);
    if (scr)
        CloseScreen(scr);
    free(src);
    free(scratch);
    free(ph_upload.samples);
    free(ph_draw.samples);
    free(ph_finish.samples);
    free(ph_swap.samples);
    free(ph_frame.samples);
    return ret;
}
