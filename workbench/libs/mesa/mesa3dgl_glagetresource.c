/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#include "mesa3dgl_types.h"
#include "mesa3dgl_gallium.h"

/*****************************************************************************

    NAME */

      APTR glAGetRenderResource(

/*  SYNOPSIS */
      GLAContext ctx)

/*  FUNCTION
        Returns the Gallium pipe_resource that contains the rendered frame
        for the given GL context. This is the back buffer that gets displayed
        when glASwapBuffers() is called.

        This function is primarily useful for a compositor that wants to
        access the rendered content directly without going through a
        glReadPixels/WritePixelArray roundtrip.

    INPUTS
        ctx - The GL context to query

    RESULT
        A pointer to the pipe_resource structure containing the rendered
        frame, or NULL if the context is invalid or has no framebuffer.
        
        The resource has PIPE_BIND_SAMPLER_VIEW set, so it can be used
        as a texture input in another context that shares the same
        pipe_screen.

    NOTES
        For zero-copy compositing:
        1. Create a compositor context with GLA_ShareContext pointing to
           an application's context
        2. Call glAGetRenderResource() on the application context to get
           its back buffer
        3. Use the resource as a texture in the compositor's rendering

        The application should call glFlush() or glFinish() before the
        compositor reads from the resource to ensure all rendering is
        complete.

    BUGS

    INTERNALS

    HISTORY

*****************************************************************************/
{
    struct mesa3dgl_context *_ctx = (struct mesa3dgl_context *)ctx;

    D(bug("[MESA3DGL] %s(ctx=%p)\n", __func__, ctx));

    if (!_ctx || !_ctx->framebuffer)
        return NULL;

    return (APTR)_ctx->framebuffer->render_resource;
}
