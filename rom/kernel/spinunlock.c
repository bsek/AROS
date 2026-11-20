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

        AROS_LH1(void, KrnSpinUnLock,

/*  SYNOPSIS */
        AROS_LHA(spinlock_t *, lock, A0),

/*  LOCATION */
        struct KernelBase *, KernelBase, 53, Kernel)

/*  FUNCTION
        Release one read or write acquisition of a spinlock.

    INPUTS
        lock - Spinlock to release.

    RESULT

    NOTES
        Without SMP support in the port this function does nothing.

    EXAMPLE

    BUGS

    SEE ALSO
        KrnSpinInit(), KrnSpinLock(), KrnSpinTryLock()

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

#if defined(KRNSPIN_ATOMICS)
    if (lock->lock == SPINLOCK_UNLOCKED)
        return;

    if (lock->lock & SPINLOCKF_WRITE)
    {
        lock->s_Owner = NULL;
        krnSpinReleaseWrite(lock);
    }
    else
        krnSpinReleaseRead(lock);
#endif

    return;

    AROS_LIBFUNC_EXIT
}
