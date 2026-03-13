/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneGetMasterGLContext
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../../include/zunegfx.h"
#include "../zunegfx_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH0(APTR, ZuneGetMasterGLContext,

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 104, zunegfx)

/*  FUNCTION
    Returns the master OpenGL context used by zunegfx for context sharing.

INPUTS
    None

RESULT
    Pointer to the master GL context, or NULL if not available.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  extern APTR OpenGL_GetMasterContext(void);

  return OpenGL_GetMasterContext();

  AROS_LIBFUNC_EXIT
}
