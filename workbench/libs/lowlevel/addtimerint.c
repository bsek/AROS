/*
    Copyright (C) 1995-2017, The AROS Development Team. All rights reserved.

    Desc:
*/

#include "lowlevel_intern.h"

#include <aros/libcall.h>
#include <exec/types.h>
#include <libraries/lowlevel.h>

/*****************************************************************************

    NAME */

      AROS_LH2(APTR, AddTimerInt,

/*  SYNOPSIS */
      AROS_LHA(APTR, intRoutine, A0),
      AROS_LHA(APTR, intData, A1),

/*  LOCATION */
      struct LowLevelBase *, LowLevelBase, 13, LowLevel)

/*  FUNCTION
 
    Add a callback function that should be executed every time the timer
    interrupt triggers.
    
    The timer will be allocated, but not configured or enabled - StartIntTimer()
    must be called to initalize the correct paramaters.

    INPUTS

    intRoutine  --  the callback function to invoke each vertical blank
    intData     --  data passed to the callback function
 
    RESULT
 
    A handle used to manipulate the interrupt or NULL if the call failed.
 
    BUGS
        This function is unimplemented.

    INTERNALS

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

    /* TODO: Write lowlevel/AddTimerInt() */
    aros_print_not_implemented ("lowlevel/AddTimerInt");

    return NULL; // return failure until implemented

  AROS_LIBFUNC_EXIT
} /* AddTimerInt */
