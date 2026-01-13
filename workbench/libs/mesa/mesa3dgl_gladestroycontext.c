/*
    Copyright (C) 2009-2020, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/gallium.h>

#include "mesa3dgl_support.h"
#include "mesa3dgl_gallium.h"

/*****************************************************************************

    NAME */

      void glADestroyContext(

/*  SYNOPSIS */
      GLAContext ctx)

/*  FUNCTION
        Destroys the GL rendering context and frees all resoureces.

    INPUTS
        ctx - pointer to GL rendering context. A NULL pointer will be
                ignored.

    RESULT
        The GL context is destroyed. Do no use it anymore.

    BUGS

    INTERNALS

    HISTORY

*****************************************************************************/
{
    struct mesa3dgl_context * _ctx = (struct mesa3dgl_context *)ctx;

    /* Destroy a MESA3DGL context */
    D(bug("[MESA3DGL] %s(ctx @ %x)\n", __func__, ctx));

    if (_ctx)
    {
        struct st_context_iface * st_ctx = _ctx->st;

        if (st_ctx)
        {
            struct st_context_iface * cur_ctx = glstapi->get_current(glstapi);

            if (cur_ctx == st_ctx)
            {
                /* Unbind if current */
                _ctx->st->flush(_ctx->st, 0, NULL, NULL, NULL);
                glstapi->make_current(glstapi, NULL, NULL, NULL);
            }

            _ctx->st->destroy(_ctx->st);
            MESA3DGLFreeFrameBuffer(_ctx->framebuffer);
            
            /*
             * Shared Context Handling:
             * Only free the stmanager if this context owns it AND no other
             * contexts are still sharing it. Contexts that share use
             * reference counting to track usage.
             */
            if (_ctx->owns_stmanager)
            {
                /* This context owns the stmanager - check ref count */
                D(bug("[MESA3DGL] %s: Context owns stmanager, ref_count=%ld\n", __func__, _ctx->ref_count));
                
                if (_ctx->ref_count <= 1)
                {
                    /* No other contexts sharing - safe to free */
                    D(bug("[MESA3DGL] %s: Freeing owned stmanager\n", __func__));
                    MESA3DGLFreeStManager(_ctx->driver, _ctx->stmanager);
                }
                else
                {
                    /* Other contexts still sharing - don't free yet */
                    /* Note: This is a problem - we're destroying the owner while
                     * others still reference it. In a proper implementation,
                     * ownership should transfer. For now, we just decrement. */
                    D(bug("[MESA3DGL] %s: WARNING - destroying owner context while %ld contexts still sharing!\n", 
                          __func__, _ctx->ref_count - 1));
                    _ctx->ref_count--;
                }
            }
            else if (_ctx->share_ctx)
            {
                /* This context is sharing from another - decrement its ref count */
                D(bug("[MESA3DGL] %s: Decrementing ref_count on share context %p\n", __func__, _ctx->share_ctx));
                _ctx->share_ctx->ref_count--;
                
                /* Don't free stmanager - we don't own it */
            }
            else
            {
                /* Legacy context without sharing info - free stmanager */
                D(bug("[MESA3DGL] %s: Legacy context - freeing stmanager\n", __func__));
                MESA3DGLFreeStManager(_ctx->driver, _ctx->stmanager);
            }
            
            /* 
             * NOTE: Do NOT call glstapi->destroy(glstapi) here!
             * glstapi is a global singleton initialized in MESA3DGLInit() 
             * and should only be destroyed in MESA3DGLExit() when the 
             * library is closed. Destroying it here breaks subsequent
             * context creation.
             */
            MESA3DGLFreeContext(_ctx);
        }
    }
}
