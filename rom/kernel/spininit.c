/*
    Copyright (C) 2015-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <aros/types/spinlock_s.h>
#include <aros/kernel.h>
#include <aros/libcall.h>

#include <kernel_base.h>
#include <kernel_spinlock.h>

/*****************************************************************************

    NAME */
#include <proto/kernel.h>

        AROS_LH1(void, KrnSpinInit,

/*  SYNOPSIS */
        AROS_LHA(spinlock_t *, lock, A0),

/*  LOCATION */
        struct KernelBase *, KernelBase, 49, Kernel)

/*  FUNCTION
        Initialize a spinlock to its unlocked state.

    INPUTS
        lock - Spinlock to initialize.

    RESULT

    NOTES
        Without SMP support in the port this function does nothing.

    EXAMPLE

    BUGS

    SEE ALSO
        KrnSpinLock(), KrnSpinTryLock(), KrnSpinUnLock()

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

#if defined(KRNSPIN_ATOMICS)
    lock->lock = SPINLOCK_UNLOCKED;
    lock->s_Owner = NULL;
#endif

    return;

    AROS_LIBFUNC_EXIT
}
