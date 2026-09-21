/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    glfillbench: how much does a pixel cost? Draws N full-window textured
    quads on top of each other (N = 1, 2, 4, ... LAYERS), glFinish()es, and
    reports GPU time per frame and per Kpix for each layer count. The same
    source builds on AROS and on Linux, so the two drivers can be compared
    on the same board. It answers the question glblitbench cannot: whether
    a slow 3D frame is the hardware or the driver.

    us/Kpix constant across layers = fill-bound, the pixels cost.
    Falling with layers = per-frame overhead (tile load/store, submit).

    Usage: glfillbench W=1280 H=720 LAYERS=8 FRAMES=100 TEX=256 TILE=4
                       BLEND NODEPTH FRONT CLEARCOLOR MIPMAP FULLSCREEN
      W,H        window (drawable) size
      LAYERS     largest layer count in the sweep 1,2,4,..,LAYERS
      FRAMES     frames timed per step (after 10 of warm-up)
      TEX        texture size (square, RGBA8)
      TILE       texture repeats across the window (TMU fetch spread)
      BLEND      alpha-blend the layers (alpha 0.5) instead of overwriting
      NODEPTH    no depth buffer: shows what Z load/store and test cost
      FRONT      draw front-to-back so early-Z rejects layers 2..N
      CLEARCOLOR clear colour too, not just depth (polymost never does;
                 without it the tile buffer is loaded every frame)
      MIPMAP     full mip chain + trilinear, like a filtered game texture
      FULLSCREEN SDL fullscreen (AROS: own screen, the flip path)

    Linux (Raspberry Pi OS):
        gcc -O2 -o glfillbench glfillbench.c $(pkg-config --cflags --libs sdl2 gl)

    Arguments use KEY=VALUE / FLAG on both, like glblitbench-linux.
*/

/* Before SDL: its headers pull in a time.h of their own, and after that
 * the POSIX clock declarations are no longer reachable. */
#include <time.h>

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

struct phase
{
    unsigned   *samples;
    unsigned    count;
    unsigned    sum, max;
};

/* NOT SDL_GetPerformanceCounter: on AROS that is ReadEClock(), whose
 * frequency is ex_EClockFrequency = 100 Hz on the Pi, so every sample
 * quantises to 10ms. CLOCK_MONOTONIC resolves through EClockUpdate() and
 * the 1MHz system timer instead, and is what Linux uses anyway. */
static unsigned now_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned)(ts.tv_sec * 1000000UL + ts.tv_nsec / 1000);
}

static void phase_add(struct phase *p, unsigned us)
{
    p->samples[p->count++] = us;
    p->sum += us;
    if (us > p->max)
        p->max = us;
}

static int cmp_u(const void *a, const void *b)
{
    unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
    return (x > y) - (x < y);
}

static unsigned phase_p99(struct phase *p)
{
    qsort(p->samples, p->count, sizeof(unsigned), cmp_u);
    return p->samples[(p->count * 99) / 100];
}

static long arg_num(int argc, char **argv, const char *key, long def)
{
    size_t klen = strlen(key);
    for (int i = 1; i < argc; i++)
        if (!strncasecmp(argv[i], key, klen) && argv[i][klen] == '=')
            return strtol(argv[i] + klen + 1, NULL, 0);
    return def;
}

static int arg_flag(int argc, char **argv, const char *key)
{
    for (int i = 1; i < argc; i++)
        if (!strcasecmp(argv[i], key))
            return 1;
    return 0;
}

/* A busy pattern, so neither the TMU cache nor a compressor gets an easy
 * time of it. Levels are 2x2 box-filtered from the one above. */
static void make_texture(long size, int mipmap)
{
    uint32_t *pix = malloc(size * size * 4);
    uint32_t seed = 0x9e3779b9u;
    long level = 0;

    for (long i = 0; i < size * size; i++)
    {
        seed = seed * 1664525u + 1013904223u;
        pix[i] = (seed & 0x00ffffffu) | 0x80000000u;
    }

    for (;;)
    {
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, size, size, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pix);
        if (!mipmap || size == 1)
            break;

        long half = size / 2;
        for (long y = 0; y < half; y++)
            for (long x = 0; x < half; x++)
            {
                uint32_t sum[4] = { 0, 0, 0, 0 };
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++)
                    {
                        uint32_t p = pix[(y * 2 + dy) * size + x * 2 + dx];
                        for (int c = 0; c < 4; c++)
                            sum[c] += (p >> (c * 8)) & 0xff;
                    }
                uint32_t out = 0;
                for (int c = 0; c < 4; c++)
                    out |= ((sum[c] + 2) / 4) << (c * 8);
                pix[y * half + x] = out;
            }
        size = half;
        level++;
    }
    free(pix);
}

int main(int argc, char **argv)
{
    long w      = arg_num(argc, argv, "W", 1280);
    long h      = arg_num(argc, argv, "H", 720);
    long layers = arg_num(argc, argv, "LAYERS", 8);
    long frames = arg_num(argc, argv, "FRAMES", 100);
    long tex    = arg_num(argc, argv, "TEX", 256);
    long tile   = arg_num(argc, argv, "TILE", 4);
    int blend      = arg_flag(argc, argv, "BLEND");
    int nodepth    = arg_flag(argc, argv, "NODEPTH");
    int front      = arg_flag(argc, argv, "FRONT");
    int clearcolor = arg_flag(argc, argv, "CLEARCOLOR");
    int mipmap     = arg_flag(argc, argv, "MIPMAP");
    int fullscreen = arg_flag(argc, argv, "FULLSCREEN");

    if (layers < 1) layers = 1;
    if (layers > 64) layers = 64;
    if (frames < 10) frames = 10;
    if (tex < 1) tex = 1;
    if (tile < 1) tile = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, nodepth ? 0 : 24);

    SDL_Window *win = SDL_CreateWindow("glfillbench",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)w, (int)h,
        SDL_WINDOW_OPENGL | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    if (!win)
    {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx)
    {
        printf("SDL_GL_CreateContext: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(0);

    int dw, dh;
    SDL_GL_GetDrawableSize(win, &dw, &dh);

    printf("glfillbench: %dx%d drawable, texture %ldx%ld%s, %ld repeats%s%s%s%s, "
           "swap interval %d\n",
           dw, dh, tex, tex, mipmap ? " trilinear" : " bilinear", tile,
           blend ? ", blended" : "", nodepth ? ", no depth" : "",
           front ? ", front-to-back" : "",
           clearcolor ? ", colour cleared" : "", SDL_GL_GetSwapInterval());
    printf("  GL: %s / %s\n", glGetString(GL_RENDERER),
           glGetString(GL_VERSION));

    GLuint texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    make_texture(tex, mipmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glEnable(GL_TEXTURE_2D);

    glViewport(0, 0, dw, dh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (nodepth)
        glDisable(GL_DEPTH_TEST);
    else
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    }
    if (blend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1, 1, 1, 0.5f);
    }
    else
        glColor4f(1, 1, 1, 1);

    /* One quad, z set per layer. Texture coordinates get a per-layer
     * offset so the layers do not all fetch the same texels. */
    GLfloat xyz[4][3] = {
        { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }
    };
    GLfloat st[4][2];
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, xyz);
    glTexCoordPointer(2, GL_FLOAT, 0, st);

    GLbitfield clearbits = (nodepth ? 0 : GL_DEPTH_BUFFER_BIT)
                         | (clearcolor ? GL_COLOR_BUFFER_BIT : 0);

    struct phase render, swap;
    render.samples = malloc(frames * sizeof(unsigned));
    swap.samples   = malloc(frames * sizeof(unsigned));

    unsigned long kpix_frame = (unsigned long)dw * dh / 1000;
    int quit = 0;

    printf("  %-6s %10s  %-32s %14s  %10s  %8s\n", "layers", "Kpix/frame",
           "render avg / p99 / max (us)", "us/Kpix", "Mpix/s", "swap avg");

    for (long n = 1; n <= layers && !quit; n *= 2)
    {
        render.count = render.sum = render.max = 0;
        swap.count = swap.sum = swap.max = 0;

        for (long f = 0; f < frames + 10 && !quit; f++)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN
                    && ev.key.keysym.sym == SDLK_ESCAPE))
                    quit = 1;

            unsigned t0 = now_us();
            if (clearbits)
                glClear(clearbits);

            for (long l = 0; l < n; l++)
            {
                /* glOrtho(0,1,0,1,-1,1) maps z_window = (1 - z)/2, so a
                 * LARGER vertex z is nearer. Back-to-front therefore
                 * counts z up - every layer passes GL_LEQUAL and is
                 * shaded; front-to-back counts down, so only the first
                 * one survives the depth test. */
                GLfloat z = front ? 1.0f - (GLfloat)l / n : (GLfloat)l / n;
                GLfloat off = (GLfloat)l / 7.0f;

                for (int v = 0; v < 4; v++)
                    xyz[v][2] = z;
                st[0][0] = off;        st[0][1] = off;
                st[1][0] = off + tile; st[1][1] = off;
                st[2][0] = off + tile; st[2][1] = off + tile;
                st[3][0] = off;        st[3][1] = off + tile;

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
            glFinish();
            unsigned t1 = now_us();
            SDL_GL_SwapWindow(win);
            unsigned t2 = now_us();

            if (f >= 10)
            {
                phase_add(&render, t1 - t0);
                phase_add(&swap, t2 - t1);
            }
        }

        if (!render.count)
            break;

        unsigned avg = render.sum / render.count;
        unsigned long kpix = kpix_frame * n;
        /* x.y us/Kpix and Mpix/s without floating point, like the sibling */
        unsigned long uspk10 = kpix ? (unsigned long)avg * 10 / kpix : 0;
        unsigned long mpixs10 = avg ? kpix * 10000 / avg : 0;

        printf("  %-6ld %10lu  %8u / %8u / %8u   %10lu.%lu  %7lu.%lu  %8u\n",
               n, kpix, avg, phase_p99(&render), render.max,
               uspk10 / 10, uspk10 % 10, mpixs10 / 10, mpixs10 % 10,
               swap.sum / swap.count);
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        printf("  GL error 0x%04x\n", (unsigned)err);

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
