/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - OpenGL Backend Geometry Batching

    Accumulates vertex data (position + color) in a CPU-side buffer.
    On flush, uploads to a VBO and draws with a single glDrawArrays call.
    This reduces per-draw-call overhead from N GPU submissions to 1.

    Batchable primitives:
    - Solid-colored quads (GL_TRIANGLES, 6 verts per quad)
    - Lines (GL_LINES, 2 verts per line)
    - Points (GL_POINTS, 1 vert per point)

    When the primitive type changes or the buffer is full, the current
    batch is auto-flushed before starting a new one.
*/

#include "opengl_intern.h"

/*****************************************************************************/
/* Internal helpers                                                          */
/*****************************************************************************/

static OpenGLBatchState *get_batch(struct RenderContext *rctx)
{
    return (OpenGLBatchState *)rctx->batch_state;
}

static inline void batch_vertex(OpenGLBatchState *batch,
                                GLfloat x, GLfloat y,
                                GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    ULONG off = batch->vertex_count * BATCH_VERTEX_FLOATS;
    batch->vertices[off + 0] = x;
    batch->vertices[off + 1] = y;
    batch->vertices[off + 2] = r;
    batch->vertices[off + 3] = g;
    batch->vertices[off + 4] = b;
    batch->vertices[off + 5] = a;
    batch->vertex_count++;
}

/* Flush if the primitive type changes or doesn't match */
static void batch_ensure_type(struct RenderContext *rctx, BatchPrimType type)
{
    OpenGLBatchState *batch = get_batch(rctx);
    if (!batch)
        return;

    if (batch->prim_type != type && batch->vertex_count > 0)
        OpenGL_BatchFlush(rctx);

    batch->prim_type = type;
}

/*****************************************************************************/
/* Lifecycle                                                                 */
/*****************************************************************************/

void OpenGL_BatchInit(struct RenderContext *rctx)
{
    OpenGLBatchState *batch;

    if (!rctx || rctx->batch_state)
        return;

    batch = (OpenGLBatchState *)AllocVec(sizeof(OpenGLBatchState), MEMF_ANY | MEMF_CLEAR);
    if (!batch)
        return;

    batch->vertex_count = 0;
    batch->prim_type = BATCH_NONE;
    batch->vbo_id = 0;
    batch->vbo_created = FALSE;

    /* Create a dynamic VBO for batch uploads */
    if (glGenBuffers_ptr && glBindBuffer_ptr && glBufferData_ptr)
    {
        glGenBuffers_ptr(1, &batch->vbo_id);
        if (batch->vbo_id != 0)
        {
            glBindBuffer_ptr(GL_ARRAY_BUFFER, batch->vbo_id);
            /* Pre-allocate the full buffer size with GL_DYNAMIC_DRAW */
            glBufferData_ptr(GL_ARRAY_BUFFER,
                             BATCH_MAX_VERTICES * BATCH_VERTEX_BYTES,
                             NULL, GL_DYNAMIC_DRAW);
            glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);
            batch->vbo_created = TRUE;
        }
    }

    rctx->batch_state = batch;
}

void OpenGL_BatchCleanup(struct RenderContext *rctx)
{
    OpenGLBatchState *batch;

    if (!rctx || !rctx->batch_state)
        return;

    batch = get_batch(rctx);

    if (batch->vbo_created && batch->vbo_id && glDeleteBuffers_ptr)
        glDeleteBuffers_ptr(1, &batch->vbo_id);

    FreeVec(batch);
    rctx->batch_state = NULL;
}

/*****************************************************************************/
/* Flush                                                                     */
/*****************************************************************************/

void OpenGL_BatchFlush(struct RenderContext *rctx)
{
    OpenGLBatchState *batch;
    GLenum gl_prim;

    if (!rctx)
        return;

    batch = get_batch(rctx);
    if (!batch || batch->vertex_count == 0)
        return;

    switch (batch->prim_type)
    {
    case BATCH_TRIANGLES: gl_prim = GL_TRIANGLES; break;
    case BATCH_LINES:     gl_prim = GL_LINES; break;
    case BATCH_POINTS:    gl_prim = GL_POINTS; break;
    default:
        batch->vertex_count = 0;
        return;
    }

    /* Ensure no shader program interferes */
    if (glUseProgram_ptr)
        glUseProgram_ptr(0);

    glDisable(GL_TEXTURE_2D);

    if (batch->vbo_created && glBindBuffer_ptr && glBufferData_ptr)
    {
        /* Upload vertex data to VBO and draw */
        glBindBuffer_ptr(GL_ARRAY_BUFFER, batch->vbo_id);
        glBufferData_ptr(GL_ARRAY_BUFFER,
                         batch->vertex_count * BATCH_VERTEX_BYTES,
                         batch->vertices, GL_DYNAMIC_DRAW);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        glVertexPointer(2, GL_FLOAT, BATCH_VERTEX_BYTES, (void *)0);
        glColorPointer(4, GL_FLOAT, BATCH_VERTEX_BYTES, (void *)(2 * sizeof(GLfloat)));

        glDrawArrays(gl_prim, 0, batch->vertex_count);

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        glBindBuffer_ptr(GL_ARRAY_BUFFER, 0);
    }
    else
    {
        /* Fallback: client-side vertex arrays */
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);

        glVertexPointer(2, GL_FLOAT, BATCH_VERTEX_BYTES, &batch->vertices[0]);
        glColorPointer(4, GL_FLOAT, BATCH_VERTEX_BYTES, &batch->vertices[2]);

        glDrawArrays(gl_prim, 0, batch->vertex_count);

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    batch->vertex_count = 0;
    batch->prim_type = BATCH_NONE;
}

/*****************************************************************************/
/* Add primitives                                                            */
/*****************************************************************************/

BOOL OpenGL_BatchAddQuad(struct RenderContext *rctx,
                         WORD x, WORD y, UWORD w, UWORD h,
                         UBYTE r, UBYTE g, UBYTE b, UBYTE a)
{
    OpenGLBatchState *batch;
    GLfloat fr, fg, fb, fa;

    if (!rctx)
        return FALSE;

    batch = get_batch(rctx);
    if (!batch)
        return FALSE;

    /* Need 6 vertices for 2 triangles */
    batch_ensure_type(rctx, BATCH_TRIANGLES);
    if (batch->vertex_count + 6 > BATCH_MAX_VERTICES)
        OpenGL_BatchFlush(rctx);

    batch->prim_type = BATCH_TRIANGLES;

    fr = r / 255.0f;
    fg = g / 255.0f;
    fb = b / 255.0f;
    fa = a / 255.0f;

    /* Triangle 1: top-left, top-right, bottom-right */
    batch_vertex(batch, (GLfloat)x,     (GLfloat)y,     fr, fg, fb, fa);
    batch_vertex(batch, (GLfloat)(x+w), (GLfloat)y,     fr, fg, fb, fa);
    batch_vertex(batch, (GLfloat)(x+w), (GLfloat)(y+h), fr, fg, fb, fa);

    /* Triangle 2: top-left, bottom-right, bottom-left */
    batch_vertex(batch, (GLfloat)x,     (GLfloat)y,     fr, fg, fb, fa);
    batch_vertex(batch, (GLfloat)(x+w), (GLfloat)(y+h), fr, fg, fb, fa);
    batch_vertex(batch, (GLfloat)x,     (GLfloat)(y+h), fr, fg, fb, fa);

    return TRUE;
}

BOOL OpenGL_BatchAddLine(struct RenderContext *rctx,
                         WORD x1, WORD y1, WORD x2, WORD y2,
                         UBYTE r, UBYTE g, UBYTE b, UBYTE a)
{
    OpenGLBatchState *batch;
    GLfloat fr, fg, fb, fa;

    if (!rctx)
        return FALSE;

    batch = get_batch(rctx);
    if (!batch)
        return FALSE;

    batch_ensure_type(rctx, BATCH_LINES);
    if (batch->vertex_count + 2 > BATCH_MAX_VERTICES)
        OpenGL_BatchFlush(rctx);

    batch->prim_type = BATCH_LINES;

    fr = r / 255.0f;
    fg = g / 255.0f;
    fb = b / 255.0f;
    fa = a / 255.0f;

    batch_vertex(batch, (GLfloat)x1, (GLfloat)y1, fr, fg, fb, fa);
    batch_vertex(batch, (GLfloat)x2, (GLfloat)y2, fr, fg, fb, fa);

    return TRUE;
}

BOOL OpenGL_BatchAddPoint(struct RenderContext *rctx,
                          WORD x, WORD y,
                          UBYTE r, UBYTE g, UBYTE b, UBYTE a)
{
    OpenGLBatchState *batch;
    GLfloat fr, fg, fb, fa;

    if (!rctx)
        return FALSE;

    batch = get_batch(rctx);
    if (!batch)
        return FALSE;

    batch_ensure_type(rctx, BATCH_POINTS);
    if (batch->vertex_count + 1 > BATCH_MAX_VERTICES)
        OpenGL_BatchFlush(rctx);

    batch->prim_type = BATCH_POINTS;

    fr = r / 255.0f;
    fg = g / 255.0f;
    fb = b / 255.0f;
    fa = a / 255.0f;

    batch_vertex(batch, (GLfloat)x, (GLfloat)y, fr, fg, fb, fa);

    return TRUE;
}
