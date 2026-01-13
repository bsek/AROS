/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#include "mesa3dgl_types.h"

/*****************************************************************************

    NAME */

      GLAContext glAGetSharedContext(

/*  SYNOPSIS */
      GLAContext ctx)

/*  FUNCTION
        Returns the context that the given context is sharing resources with,
        or NULL if the context is not sharing.

    INPUTS
        ctx - The GL context to query

    RESULT
        The GL context that resources are being shared with, or NULL if
        the context is not sharing resources with any other context.

    BUGS

    INTERNALS

    HISTORY

*****************************************************************************/
{
    struct mesa3dgl_context *_ctx = (struct mesa3dgl_context *)ctx;

    D(bug("[MESA3DGL] %s(ctx=%p)\n", __func__, ctx));

    if (!_ctx)
        return NULL;

    return (GLAContext)_ctx->share_ctx;
}
