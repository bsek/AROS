/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#include "hostgl_types.h"

/*****************************************************************************

    NAME */

      APTR glAGetRenderResource(

/*  SYNOPSIS */
      GLAContext ctx)

/*  FUNCTION
        Returns the Gallium pipe_resource that contains the rendered frame
        for the given GL context.

    INPUTS
        ctx - The GL context to query

    RESULT
        A pointer to the pipe_resource structure containing the rendered
        frame, or NULL if the context is invalid or the implementation
        does not use Gallium.

    NOTES
        The hostgl.library uses GLX directly and does not use Gallium,
        so this function always returns NULL.

    BUGS

    INTERNALS

    HISTORY

*****************************************************************************/
{
    D(bug("[HostGL] %s(ctx=%p)\n", __func__, ctx));

    /* hostgl does not use Gallium - no pipe_resource available */
    return NULL;
}
