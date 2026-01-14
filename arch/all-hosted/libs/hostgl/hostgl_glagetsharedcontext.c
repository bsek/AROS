/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#include "hostgl_types.h"

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

    NOTES
        The hostgl.library does not currently support context sharing,
        so this function always returns NULL.

    BUGS

    INTERNALS

    HISTORY

*****************************************************************************/
{
    D(bug("[HostGL] %s(ctx=%p)\n", __func__, ctx));

    /* hostgl does not support context sharing */
    return NULL;
}
