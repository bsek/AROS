/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#include "mesa3dgl_types.h"

/*****************************************************************************

    NAME */

      APTR glAGetPipeScreen(

/*  SYNOPSIS */
      GLAContext ctx)

/*  FUNCTION
        Returns the Gallium pipe_screen associated with the given GL context.
        This can be used to check if two contexts share the same pipe_screen,
        which is required for resource sharing.

    INPUTS
        ctx - The GL context to query

    RESULT
        A pointer to the pipe_screen structure, or NULL if the context is
        invalid. This is an opaque pointer for use with Gallium APIs.

    NOTES
        Two contexts can share textures and buffers only if they use the
        same pipe_screen. Use this function to verify sharing compatibility.

    BUGS

    INTERNALS

    HISTORY

*****************************************************************************/
{
    struct mesa3dgl_context *_ctx = (struct mesa3dgl_context *)ctx;

    D(bug("[MESA3DGL] %s(ctx=%p)\n", __func__, ctx));

    if (!_ctx || !_ctx->stmanager)
        return NULL;

    return (APTR)_ctx->stmanager->screen;
}
