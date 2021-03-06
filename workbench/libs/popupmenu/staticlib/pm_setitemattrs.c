/*
    Copyright (C) 2010, The AROS Development Team.
    All rights reserved.
    
*/

#include <utility/tagitem.h>
#include <proto/alib.h>

/*****************************************************************************

    NAME */
#define NO_INLINE_STDARG /* turn off inline def */
#include <proto/popupmenu.h>
extern struct PopupMenuBase * PopupMenuBase;

        LONG PM_SetItemAttrs(

/*  SYNOPSIS */
        struct PopupMenu *p,
        Tag tag1,
        ...)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_SLOWSTACKTAGS_PRE(tag1)

    retval = PM_SetItemAttrsA(p, AROS_SLOWSTACKTAGS_ARG(tag1));
    
    AROS_SLOWSTACKTAGS_POST
} /* PM_SetItemAttrs */
