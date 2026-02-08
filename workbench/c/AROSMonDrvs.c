/*
    Copyright (C) 1995-2017, The AROS Development Team. All rights reserved.

    Desc: Code that loads and initializes necessary HIDDs.
*/

/******************************************************************************


    NAME

        AROSMonDrvs

    SYNOPSIS

        NOCOMPOSITION/S,ONLYCOMPOSITION/S

    LOCATION

        C:

    FUNCTION

        This command does almost the same thing as C:LoadMonDrvs does on
        other systems. However, additionally we support priority-based
        sorting for display drivers. This is needed in order to make monitor
        ID assignment more predictable.

    INPUTS

        NOCOMPOSITION   -- Only load Monitors
        ONLYCOMPOSITION -- Only load Compositor

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

******************************************************************************/

#define DEBUG 0

#include <aros/debug.h>
#include <dos/dosextens.h>
#include <workbench/icon.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>

#include <aros/shcommands.h>

#include <stdlib.h>
#include <string.h>

#define MONITORS_DIR        "DEVS:Monitors"
#define COMPOSITING_DEFAULT  "Compositor"
#define COMPOSITING_ENV_VAR  "SYS/compositor.name"
#define COMPOSITING_BUFSIZE  64

/************************************************************************/

struct MonitorNode
{
    struct Node n;
    char        Name[1];
};

static BOOL isCompositor(const char *name)
{
    return (strcmp(name, "Compositor") == 0 ||
            strcmp(name, "GLCompositor") == 0);
}

static BYTE checkIcon(STRPTR name, struct Library *IconBase)
{
    LONG pri = 0;
    struct DiskObject *dobj = GetDiskObject(name);

    if (dobj == NULL)
        return 0;

    if ((dobj->do_Type == WBTOOL) || (dobj->do_Type == WBPROJECT))
    {
        const STRPTR *toolarray = (const STRPTR *)dobj->do_ToolTypes;
        STRPTR s;

        if ((s = FindToolType(toolarray, "STARTPRI")))
        {
            pri = atol(s);
            if (pri < -128)
                pri = -128;
            else if (pri > 127)
                pri = 127;
        }
        FreeDiskObject(dobj);
    }
    return pri;
}

static BOOL findMonitors(struct List *monitorsList, struct DosLibrary *DOSBase, struct Library *IconBase, struct ExecBase *SysBase, APTR poolmem)
{
    BOOL retvalue = TRUE;
    LONG error;
    struct AnchorPath *ap = AllocPooled(poolmem, sizeof(struct AnchorPath));

    DB2(bug("[LoadMonDrvs] AnchorPath 0x%p\n", ap));
    if (ap)
    {
        /* Initialize important fields in AnchorPath, especially
           ap_Strlen (prevents memory trashing) */
        ap->ap_Flags     = 0;
        ap->ap_Strlen    = 0;
        ap->ap_BreakBits = 0;

        error = MatchFirst("~((#?.info)|(#?.dbg))", ap);
        while (!error)
        {
            struct MonitorNode *newnode;

            DB2(bug("[LoadMonDrvs] Found monitor name %s\n", ap->ap_Info.fib_FileName));

            /* Skip compositor executables -- they were loaded separately */
            if (!isCompositor(ap->ap_Info.fib_FileName))
            {
                newnode = AllocPooled(poolmem, sizeof(struct MonitorNode) + strlen(ap->ap_Info.fib_FileName));
                DB2(bug("[LoadMonDrvs] Monitor node 0x%p\n", newnode));
                if (newnode == NULL)
                {
                    retvalue = FALSE;
                    break;
                }

                strcpy(newnode->Name, ap->ap_Info.fib_FileName);
                if (IconBase)
                    newnode->n.ln_Pri = checkIcon(ap->ap_Info.fib_FileName, IconBase);
                else
                    newnode->n.ln_Pri = 0;
                Enqueue(monitorsList, &newnode->n);
            }

            error = MatchNext(ap);
        }

        if (error != ERROR_NO_MORE_ENTRIES)
        {
            retvalue = FALSE;
/*          FIXME: Why no MatchEnd() in this case?
            goto exit; */
        }
        MatchEnd(ap);
    }
    else
        retvalue = FALSE;

    return retvalue;
}

static void loadMonitors(struct List *monitorsList, struct DosLibrary *DOSBase,
    struct ExecBase *SysBase)
{
    struct MonitorNode *node;

    D(bug("[LoadMonDrvs] Loading monitor drivers...\n"));
    D(bug(" Pri Name\n"));

    ForeachNode(monitorsList, node)
    {
        D(bug("%4d %s\n", node->n.ln_Pri, node->Name));
        Execute(node->Name, BNULL, BNULL);
    }

    D(bug("--------------------------\n"));
}

AROS_SH2H(AROSMonDrvs, 1.0, "Load AROS Monitor and Compositor drivers",
        AROS_SHAH(BOOL, , NOCOMPOSITION,/S,FALSE, "Only load Monitors"),
        AROS_SHAH(BOOL, , ONLYCOMPOSITION,/S,FALSE, "Only load Compositor"))
{
    AROS_SHCOMMAND_INIT

    APTR pool;
    struct Library *IconBase;
    BPTR dir, olddir;
    BOOL res = TRUE;

    dir = Lock(MONITORS_DIR, SHARED_LOCK);
    D(bug("[LoadMonDrvs] Monitors directory 0x%p\n", dir));
    if (dir)
    {
        olddir = CurrentDir(dir);

        if (!SHArg(NOCOMPOSITION))
        {
            TEXT compositorName[COMPOSITING_BUFSIZE];

            /* Read preferred compositor name from ENV:, fall back to default */
            if (GetVar(COMPOSITING_ENV_VAR, compositorName,
                       COMPOSITING_BUFSIZE, GVF_GLOBAL_ONLY) <= 0)
            {
                strcpy(compositorName, COMPOSITING_DEFAULT);
            }

            D(bug("[LoadMonDrvs] Loading compositor: %s\n", compositorName));
            Execute(compositorName, BNULL, BNULL);

            /* If we tried a non-default compositor, also run the fallback.
             * If the first one succeeded, the fallback detects the existing
             * class via OOP_FindClass() and exits cleanly.
             * If the first one failed, the fallback takes over. */
            if (strcmp(compositorName, COMPOSITING_DEFAULT) != 0)
            {
                D(bug("[LoadMonDrvs] Running fallback: %s\n", COMPOSITING_DEFAULT));
                Execute(COMPOSITING_DEFAULT, BNULL, BNULL);
            }
        }
        
        if (!SHArg(ONLYCOMPOSITION))
        {
            pool = CreatePool(MEMF_ANY, sizeof(struct MonitorNode) * 10, sizeof(struct MonitorNode) * 5);
            DB2(bug("[LoadMonDrvs] Created pool 0x%p\n", pool));
            if (pool)
            {
                struct List MonitorsList;

                NewList(&MonitorsList);
                IconBase = OpenLibrary("icon.library", 0);
                if (IconBase)
                {
                    findMonitors(&MonitorsList, DOSBase, IconBase, SysBase, pool);
                    loadMonitors(&MonitorsList, DOSBase, SysBase);
                    CloseLibrary(IconBase);
                }
                DeletePool(pool);
            }
            else
                res = FALSE;
        }

        CurrentDir(olddir);
        UnLock(dir);
    }
    
    return res;

    AROS_SHCOMMAND_EXIT
}
