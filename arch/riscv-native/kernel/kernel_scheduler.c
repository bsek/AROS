/*
    Copyright (C) 2015-2026, The AROS Development Team. All rights reserved.

    Desc: Task scheduler for native RISC-V kernels.

    Uniprocessor builds use the generic scheduler unchanged. SMP builds
    use this port of the aarch64-native scheduler: the ready list is
    shared, each CPU only dispatches tasks whose affinity includes it,
    and every (state, list) change happens in the kernel under the locks
    of both lists.
*/

#include <aros/config.h>

#if !defined(__AROSEXEC_SMP__)

#include "../../../rom/kernel/kernel_scheduler.c"

#else /* __AROSEXEC_SMP__ */

#include <exec/alerts.h>
#include <exec/execbase.h>
#include <exec/lists.h>
#include <proto/exec.h>

#include <kernel_base.h>
#include <kernel_debug.h>
#include <kernel_scheduler.h>

#include "kernel_cpu.h"
#include "kernel_riscv.h"
#include "kernel_spinlock.h"

#define AROS_NO_ATOMIC_OPERATIONS
#include <exec_platform.h>

#include <aros/types/spinlock_s.h>

#include <etask.h>

#define __AROS_KERNEL__
#include "exec_intern.h"

#define DSCHED(x)

extern spinlock_t *Kernel_52_KrnSpinLock(spinlock_t *, struct Hook *, ULONG, void *);
extern void Kernel_53_KrnSpinUnLock(spinlock_t *, void *);

/*
 * iet_CpuAffinity is a mask buffer or the TASKAFFINITY_ANY sentinel.
 * NULL is not a valid mask and matches no CPU - but masks are only
 * meaningful once exec has assigned one to every task and set
 * EXECF_CPUAffinity (exec_smp.c); until then only the boot CPU runs.
 */
static inline BOOL core_AffinityMatch(struct Task *t, int cpunum)
{
    void *aff;

    if (!(PrivExecBase(SysBase)->IntFlags & EXECF_CPUAffinity))
        return TRUE;

    if (!(t->tc_Flags & TF_ETASK) || !t->tc_UnionETask.tc_ETask)
        return FALSE;

    aff = (void *)(IPTR)GetIntETask(t)->iet_CpuAffinity;
    if ((IPTR)aff == TASKAFFINITY_ANY)
        return TRUE;
    if (!aff || ((IPTR)aff == TASKAFFINITY_ALL_BUT_SELF))
        return FALSE;

    return (((ULONG *)aff)[cpunum / 32] & (1UL << (cpunum % 32))) != 0;
}

/* Check if the currently running task on this cpu should be rescheduled */
BOOL core_Schedule(void)
{
    int cpunum = krnRVCPUNumber();
    struct Task *task = GET_THIS_TASK;
    BOOL corereschedule = TRUE;

    DSCHED(bug("[Kernel:%02d] core_Schedule()\n", cpunum));

    FLAG_SCHEDSWITCH_CLEAR;

    /* If task has pending exception, reschedule it so that the dispatcher may handle the exception */
    if (!(task->tc_Flags & TF_EXCEPT))
    {
#if defined(__AROSEXEC_SMP__)
        /* IRQ+FIQ off: the IPI and the timer IRQ take this lock as a
         * write, and re-entering it self-deadlocks this CPU. */
        unsigned int __if = EXEC_IRQFIQ_DISABLE();
        Kernel_52_KrnSpinLock(&PrivExecBase(SysBase)->TaskReadySpinLock, NULL,
                    SPINLOCK_MODE_READ, NULL);
#endif
        /* Is the TaskReady empty? If yes, then the running task is the only one. Let it work */
        if (IsListEmpty(&SysBase->TaskReady))
            corereschedule = FALSE;
        else
        {
            struct Task *nexttask;
            /*
                    If there are tasks ready for this cpu that have equal or lower priority,
                    and the current task has used its alloted time - reschedule so they can run
                */
            for (nexttask = (struct Task *)GetHead(&SysBase->TaskReady); nexttask != NULL; nexttask = (struct Task *)GetSucc(nexttask))
            {
#if defined(__AROSEXEC_SMP__)
                if (core_AffinityMatch(nexttask, cpunum))
                {
#endif
                    if (nexttask->tc_Node.ln_Pri <= task->tc_Node.ln_Pri)
                    {
                        /* If the running task did not used it's whole quantum yet, let it work */
                        if (!FLAG_SCHEDQUANTUM_ISSET)
                            corereschedule = FALSE;
                    }
                    break;
#if defined(__AROSEXEC_SMP__)
                }
#endif
            }
        }
#if defined(__AROSEXEC_SMP__)
        Kernel_53_KrnSpinUnLock(&PrivExecBase(SysBase)->TaskReadySpinLock, NULL);
        EXEC_IRQFIQ_RESTORE(__if);
#endif
    }

    DSCHED
        (
            if (corereschedule)
                bug("[Kernel:%02d] '%s' @ 0x%p needs rescheduled ..\n", cpunum, task->tc_Node.ln_Name, task);
        )

    return corereschedule;
}

/* Switch the currently running task on this cpu to ready state */
void core_Switch(void)
{
    int cpunum = krnRVCPUNumber();
    struct Task *task = GET_THIS_TASK;
#if defined(__AROSEXEC_SMP__)
    unsigned int __if;
#endif

    DSCHED(bug("[Kernel:%02d] core_Switch(%08x)\n", cpunum, task->tc_State));

    /* Every outgoing task, not only TS_RUN: Wait() switches away in
     * TS_WAIT holding a Disable() level. */
    task->tc_IDNestCnt = IDNESTCOUNT_GET;

    /* Carry the unused slice with the task, or a task preempted more
     * often than the heartbeat ticks never expires its quantum. */
    if (GetIntETask(task))
        GetIntETask(task)->iet_QuantumLeft = SCHEDELAPSED_GET;

    __if = EXEC_IRQFIQ_DISABLE();
    if (FLAG_SCHEDPARK_ISSET)
    {
        FLAG_SCHEDPARK_CLEAR;
        /* Wait(): park, unless a Signal() that saw TS_RUN got in first */
        exec_TaskPark(task);
        EXEC_IRQFIQ_RESTORE(__if);
        return;
    }

    if (task->tc_State == TS_RUN)
    {
        DSCHED(bug("[Kernel:%02d] Switching away from '%s' @ 0x%p\n", cpunum, task->tc_Node.ln_Name, task));

        /* if the current task has gone out of stack bounds, suspend it to prevent further damage to the system */
        if (task->tc_SPReg <= task->tc_SPLower || task->tc_SPReg > task->tc_SPUpper)
        {
            bug("[Kernel:%02d] '%s' @ 0x%p went out of stack limits\n", cpunum, task->tc_Node.ln_Name, task);
            bug("[Kernel:%02d]  - Lower 0x%p, upper 0x%p, SP 0x%p\n", cpunum, task->tc_SPLower, task->tc_SPUpper, task->tc_SPReg);

            task->tc_SigWait    = 0;
            exec_TaskMove(task, TS_RUN, TS_WAIT);

            Alert(AN_StackProbe);
        }

        if (task->tc_Flags & TF_SWITCH)
            AROS_UFC1NR(void, task->tc_Switch, AROS_UFCA(struct ExecBase *, SysBase, A6));

        if (task->tc_State == TS_RUN)
        {
            DSCHED(bug("[Kernel:%02d] Setting '%s' @ 0x%p as ready\n", cpunum, task->tc_Node.ln_Name, task));
            exec_TaskMove(task, TS_RUN, TS_READY);
        }
    }
    EXEC_IRQFIQ_RESTORE(__if);
}

/* Dispatch a "new" ready task on this cpu */
struct Task *core_Dispatch(void)
{
    struct Task *newtask;
    struct Task *task = GET_THIS_TASK;
    int cpunum = krnRVCPUNumber();
    /* Interrupts off: the IPI and the timer take the list locks too.
     * Nestable across the recursive call below. */
    unsigned int __if = EXEC_IRQFIQ_DISABLE();

    DSCHED(bug("[Kernel:%02d] core_Dispatch()\n", cpunum));

    /* Ready to running in one step, under both list locks */
    exec_TaskListsLock(EXEC_TASKLIST_RUNNING, EXEC_TASKLIST_READY);
    for (newtask = (struct Task *)GetHead(&SysBase->TaskReady); newtask != NULL; newtask = (struct Task *)GetSucc(newtask))
    {
        if (core_AffinityMatch(newtask, cpunum))
        {
            Remove(&newtask->tc_Node);
            newtask->tc_State = TS_RUN;
            AddHead(&PrivExecBase(SysBase)->TaskRunning, &newtask->tc_Node);
            break;
        }
    }
    exec_TaskListsUnlock(EXEC_TASKLIST_RUNNING, EXEC_TASKLIST_READY);

    /* Nothing else to run: the current task carries on */
    if ((!newtask) && (task))
    {
        if (task->tc_State == TS_RUN)
            newtask = task;
    }

    if (newtask != NULL)
    {
        DSCHED(bug("[Kernel:%02d] Preparing to run '%s' @ 0x%p\n",
            cpunum, newtask->tc_Node.ln_Name, newtask));

        SysBase->DispCount++;
        IDNESTCOUNT_SET(newtask->tc_IDNestCnt);
        TLS_SET(ThisTask, newtask);
        /* Only a fresh slice when the task changes or used its own,
         * or a fast-dispatching core never expires a quantum. */
        {
            struct IntETask *iet = GetIntETask(newtask);
            ULONG left = iet ? iet->iet_QuantumLeft : 0;

            SCHEDELAPSED_SET(left ? left : SCHEDQUANTUM_GET);
        }
        FLAG_SCHEDQUANTUM_CLEAR;

        /* Check the stack of the task we are about to launch. */
        if ((newtask->tc_SPReg <= newtask->tc_SPLower) ||
            (newtask->tc_SPReg > newtask->tc_SPUpper))
        {
            /* if the new task shouldn't run - force a reschedule */
            DSCHED(bug("[Kernel:%02d] Skipping '%s' @ 0x%p\n", cpunum, newtask->tc_Node.ln_Name, newtask));
            exec_TaskMove(newtask, TS_RUN, TS_WAIT);
            core_Switch();
            newtask = core_Dispatch();
        }
        else
        {
            DSCHED(bug("[Kernel:%02d] Launching '%s' @ 0x%p\n", cpunum, newtask->tc_Node.ln_Name, newtask));
        }
    }
    else
    {
        /* Go idle if there is nothing to do */
        DSCHED(bug("[Kernel:%02d] No ready Task(s) - entering sleep mode\n", cpunum));

        /*
         * Idle counter is incremented every time when we enter here,
         * not only once. This is correct.
         */
        SysBase->IdleCount++;
        FLAG_SCHEDSWITCH_SET;
    }

    EXEC_IRQFIQ_RESTORE(__if);
    return newtask;
}

#endif /* __AROSEXEC_SMP__ */
