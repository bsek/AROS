/*
    (C) 1995-97 AROS - The Amiga Research OS
    $Id$

    Desc:
    Lang: english
*/

#include <exec/types.h>
#include <workbench/workbench.h>
#include <exec/ports.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include "workbench_intern.h"

/*****************************************************************************

    NAME */

    #include <proto/workbench.h>

    AROS_LH4(struct AppWindowDropZone *, AddAppWindowDropZoneA,

/*  SYNOPSIS */
    AROS_LHA(struct AppWindow *, aw      , A0),
    AROS_LHA(ULONG               , id      , D0),
    AROS_LHA(ULONG               , userdata, D1),
    AROS_LHA(struct TagItem *  , tags    , A1),

/*  LOCATION */
    struct WorkbenchBase *, WorkbenchBase, 19, Workbench)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

******************************************************************************/
{
    AROS_LIBFUNC_INIT
    AROS_LIBBASE_EXT_DECL(struct WorkbenchBase *, WorkbenchBase)

    aros_print_not_implemented ("AddAppWindowDropZoneA");
#warning TODO: Write Workbench/AddAppWindowDropZoneA

    return NULL;

    AROS_LIBFUNC_EXIT
} /* AddAppWindowDropZoneA */

