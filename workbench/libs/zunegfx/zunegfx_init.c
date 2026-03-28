/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Simplified AROS Initialization

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

static int ZuneGfx_Init(struct IntZuneGfxBase *base)
{
    D(bug("ZuneGfx: Library Init\n"));

    /* Initialize the library base */
    if (!InitializeZuneGfx(base)) {
        D(bug("ZuneGfx: Library initialization failed\n"));
        return FALSE;
    }

    D(bug("ZuneGfx: Library initialized successfully\n"));
    return TRUE;
}

static int ZuneGfx_Expunge(struct IntZuneGfxBase *base)
{
    D(bug("ZuneGfx: Library Expunge\n"));

    /* Clean up library resources */
    CleanupZuneGfx(base);

    D(bug("ZuneGfx: Library cleanup completed\n"));
    return TRUE;
}

static int ZuneGfx_Open(struct IntZuneGfxBase *base)
{
    D(bug("ZuneGfx: Library Open (OpenCnt: %ld)\n", base->libnode.lib_OpenCnt));

    /* Nothing special needed for open in this library */
    return TRUE;
}

static int ZuneGfx_Close(struct IntZuneGfxBase *base)
{
    D(bug("ZuneGfx: Library Close (OpenCnt: %ld)\n", base->libnode.lib_OpenCnt));

    /* Nothing special needed for close in this library */
    return TRUE;
}

/*****************************************************************************/
/* AROS Symbol Sets */
/*****************************************************************************/

/* Tell AROS when to call our initialization functions */
ADD2INITLIB(ZuneGfx_Init, 0)
ADD2EXPUNGELIB(ZuneGfx_Expunge, 0)
ADD2OPENLIB(ZuneGfx_Open, 0)
ADD2CLOSELIB(ZuneGfx_Close, 0)

/*****************************************************************************/
/* Version Information */
/*****************************************************************************/

/* Version string for the library */
const UBYTE LibName[] = "zunegfx.library";
const UBYTE LibID[] = VSTRING;
const UWORD LibVersion = VERSION;
const UWORD LibRevision = REVISION;
