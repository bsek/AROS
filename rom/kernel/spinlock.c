/*
    Copyright (C) 2015-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <aros/types/spinlock_s.h>
#include <aros/kernel.h>
#include <aros/libcall.h>
#include <utility/hooks.h>

#include <kernel_base.h>
#include <kernel_spinlock.h>

#if defined(KRNSPIN_ATOMICS)
#include <exec/tasks.h>
#include <proto/exec.h>
#define __KERNEL_NO_SPINLOCK_PROTOS__
#include <exec_platform.h>

/* Direct call: exec passes a NULL KernelBase */
int Kernel_13_KrnIsSuper();
#endif

/*****************************************************************************

    NAME */
#include <proto/kernel.h>

        AROS_LH3(spinlock_t *, KrnSpinLock,

/*  SYNOPSIS */
        AROS_LHA(spinlock_t *, lock, A1),
        AROS_LHA(struct Hook *, failhook, A0),
        AROS_LHA(ULONG, mode, D0),

/*  LOCATION */
        struct KernelBase *, KernelBase, 52, Kernel)

/*  FUNCTION
        Acquire a spinlock in read or write mode, waiting while it is
        held.

    INPUTS
        lock     - Spinlock to acquire.
        failhook - Optional hook, called with the lock each time an
                   attempt fails.
        mode     - SPINLOCK_MODE_READ or SPINLOCK_MODE_WRITE.

    RESULT
        The supplied spinlock pointer.

    NOTES
        The lock gives exclusion between CPUs only. It does not stop task
        switching or mask interrupts; pair it with Forbid() or Disable()
        when that is needed.

        A task may be switched out while it holds a lock. A write lock
        taken by a task records the task as its owner. A task spinning on
        a write lock owned by a task of lower priority drops to that
        priority until it gets the lock, so the owner can run. It does not
        under Disable() or Forbid(), where it cannot give up the CPU. A
        caller in supervisor state records no owner.

        Without SMP support in the port this function does nothing.

    EXAMPLE

    BUGS

    SEE ALSO
        KrnSpinInit(), KrnSpinTryLock(), KrnSpinUnLock()

    INTERNALS
        The port supplies the atomic steps in kernel_spinlock.h.

******************************************************************************/
{
    AROS_LIBFUNC_INIT

#if defined(KRNSPIN_ATOMICS)
    /* Kernel context takes none of the task path (no owner, no hook) */
    struct Task *me = NULL;

    if (((mode == SPINLOCK_MODE_WRITE) || failhook) && !Kernel_13_KrnIsSuper())
        me = GET_THIS_TASK;

    if (mode == SPINLOCK_MODE_WRITE)
    {
        BYTE old_pri = 0;
        BOOL priority_changed = FALSE, can_drop;

        /* Under Disable() or Forbid() we keep the CPU anyway */
        can_drop = (me && (IDNESTCOUNT_GET < 0) && (TDNESTCOUNT_GET < 0));

        while (!krnSpinTryWrite(lock))
        {
            struct Task *t = lock->s_Owner;

            krnSpinPause(lock, ~0);

            if (failhook && me)
                CALLHOOKPKT(failhook, (APTR)lock, 0);

            /* Let a lower priority owner get the CPU */
            if (can_drop && t && (me->tc_Node.ln_Pri > t->tc_Node.ln_Pri))
            {
                BYTE pri = SetTaskPri(me, t->tc_Node.ln_Pri);

                if (!priority_changed)
                {
                    old_pri = pri;
                    priority_changed = TRUE;
                }
            }
        }

        lock->s_Owner = me;
        if (priority_changed)
            SetTaskPri(me, old_pri);
    }
    else
    {
        while (!krnSpinTryRead(lock))
        {
            krnSpinPause(lock, KRNSPIN_BUSY_FOR_READ);

            if (failhook && me)
                CALLHOOKPKT(failhook, (APTR)lock, 0);
        }
    }
#endif

    return lock;

    AROS_LIBFUNC_EXIT
}
