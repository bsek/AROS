/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Simplified AROS Initialization

    This module provides the AROS library initialization and cleanup using
    the standard AROS conf file system. The initialization follows AROS
    conventions and integrates with the genmodule build system.
*/

#include <aros/libcall.h>
#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <exec/resident.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>

#include "src/zunegfx_intern.h"

/*****************************************************************************/
/* Library Version Information */
/*****************************************************************************/

#define VERSION    1
#define REVISION   0
#define DATESTR    __DATE__
#define VERS       "zunegfx.library 1.0"
#define VSTRING    "zunegfx.library 1.0 (" DATESTR ")\r\n"
#define VERSTAG    "\0$VER: " VSTRING

/*****************************************************************************/
/* Global Variables */
/*****************************************************************************/

/* These will be available to all library functions */
struct Library *CyberGfxBase = NULL;
struct Library *GLBase = NULL;

/*****************************************************************************/
/* Library Initialization Functions */
/*****************************************************************************/

static int ZuneRenderer_Init(struct IntZuneGfxBase *base)
{
    D(bug("ZuneRenderer: Library Init\n"));

    /* Initialize the library base */
    if (!InitializeZuneRenderer(base)) {
        D(bug("ZuneRenderer: Library initialization failed\n"));
        return FALSE;
    }

    D(bug("ZuneRenderer: Library initialized successfully\n"));
    return TRUE;
}

static int ZuneRenderer_Expunge(struct IntZuneGfxBase *base)
{
    D(bug("ZuneRenderer: Library Expunge\n"));

    /* Clean up library resources */
    CleanupZuneRenderer(base);

    D(bug("ZuneRenderer: Library cleanup completed\n"));
    return TRUE;
}

static int ZuneRenderer_Open(struct IntZuneGfxBase *base)
{
    D(bug("ZuneRenderer: Library Open (OpenCnt: %ld)\n", base->libnode.lib_OpenCnt));

    /* Nothing special needed for open in this library */
    return TRUE;
}

static int ZuneRenderer_Close(struct IntZuneGfxBase *base)
{
    D(bug("ZuneRenderer: Library Close (OpenCnt: %ld)\n", base->libnode.lib_OpenCnt));

    /* Nothing special needed for close in this library */
    return TRUE;
}

/*****************************************************************************/
/* AROS Symbol Sets */
/*****************************************************************************/

/* Tell AROS when to call our initialization functions */
ADD2INITLIB(ZuneRenderer_Init, 0)
ADD2EXPUNGELIB(ZuneRenderer_Expunge, 0)
ADD2OPENLIB(ZuneRenderer_Open, 0)
ADD2CLOSELIB(ZuneRenderer_Close, 0)

/*****************************************************************************/
/* Version Information */
/*****************************************************************************/

/* Version string for the library */
const UBYTE LibName[] = "zunegfx.library";
const UBYTE LibID[] = VSTRING;
const UWORD LibVersion = VERSION;
const UWORD LibRevision = REVISION;
