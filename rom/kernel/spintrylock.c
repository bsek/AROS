/*
    Copyright (C) 2015-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <aros/types/spinlock_s.h>
#include <aros/kernel.h>
#include <aros/libcall.h>

#include <kernel_base.h>
#include <kernel_spinlock.h>

#if defined(KRNSPIN_ATOMICS)
#include <exec/tasks.h>
#define __KERNEL_NO_SPINLOCK_PROTOS__
#include <exec_platform.h>

/* Direct call: exec passes a NULL KernelBase */
int Kernel_13_KrnIsSuper();
#endif

/*****************************************************************************

    NAME */
#include <proto/kernel.h>

        AROS_LH2(spinlock_t *, KrnSpinTryLock,

/*  SYNOPSIS */
        AROS_LHA(spinlock_t *, lock, A0),
        AROS_LHA(ULONG, mode, D0),

/*  LOCATION */
        struct KernelBase *, KernelBase, 51, Kernel)

/*  FUNCTION
        Attempt to acquire a spinlock in read or write mode without
        waiting for a current lock holder to release it.

    INPUTS
        lock - Spinlock to acquire.
        mode - SPINLOCK_MODE_READ or SPINLOCK_MODE_WRITE.

    RESULT
        Lock pointer if acquired; NULL if the requested mode cannot
        be obtained on an implementation with spinlock support.

    NOTES
        A write lock taken by a task records the task as its owner; a
        caller in supervisor state records no owner.

        Without SMP support in the port this function returns lock
        without acquiring it.

    EXAMPLE

    BUGS

    SEE ALSO
        KrnSpinInit(), KrnSpinLock(), KrnSpinUnLock()

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

#if defined(KRNSPIN_ATOMICS)
    if (mode == SPINLOCK_MODE_WRITE)
    {
        if (!krnSpinTryWrite(lock))
            return NULL;
        lock->s_Owner = Kernel_13_KrnIsSuper() ? NULL : GET_THIS_TASK;
    }
    else if (!krnSpinTryRead(lock))
        return NULL;
#endif

    return lock;

    AROS_LIBFUNC_EXIT
}
