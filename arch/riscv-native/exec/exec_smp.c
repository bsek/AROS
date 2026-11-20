/*
    Copyright (C) 2013-2026, The AROS Development Team. All rights reserved.

    Desc: exec SMP support for native RISC-V targets: the scheduler-list
          moves exec asks for (krnSysCallReschedTask/krnSysCallSwitch,
          from aarch64-native) and the switch to per-CPU affinity.
*/

#include <aros/config.h>

#if defined(__AROSEXEC_SMP__)

#define DEBUG 0
#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <proto/exec.h>

#define __KERNEL_NOLIBBASE__
#include <proto/kernel.h>

#include "kernel_riscv.h"

#include "exec_intern.h"
#include "etask.h"

/*
 * krnSysCallReschedTask(), kernel context only (SC_RESCHEDTASK from a
 * task). Only a parked or new task is made ready: another CPU may have
 * woken it already.
 */
void Exec_ReschedTask(struct Task *task, ULONG newState)
{
    /* Also runs from the IPI handler */
    unsigned int __if = EXEC_IRQFIQ_DISABLE();

    for (;;)
    {
        ULONG state = task->tc_State;

        if ((newState == TS_READY) && (state != TS_WAIT) &&
            (state != TS_INVALID) && (state != TS_ADDED))
            break;
        if (exec_TaskMove(task, state, newState))
            break;
    }

    EXEC_IRQFIQ_RESTORE(__if);
}

/* RemTask's self-removal (SC_SUICIDESWITCH): detach and tombstone for
 * the service task, then return - unlike KrnSwitch() this does NOT
 * dispatch. */
void Exec_SuicideSwitch(void)
{
    struct Task *task = GET_THIS_TASK;

    exec_TaskRelist(task, EXEC_TASKLIST_RUNNING, EXEC_TASKLIST_NONE,
                    task->tc_State, TS_TOMBSTONED);
}

/*
 * Give every task that exists so far an affinity mask, then switch exec
 * to per-CPU affinity (all-pc does the same in cpu_PrepareExec). A NULL
 * mask is not valid and would leave a task that no CPU dispatches, so
 * the flag is only set once every task has one. They all ran on the
 * boot CPU until now, and the boot sequence is not migration-safe, so
 * that is where they stay; tasks created from here on inherit their
 * creator's mask (InitETask).
 */
static BOOL Exec_RVAssignMask(struct Task *task)
{
    struct IntETask *iet;
    void *aff;

    if (!(task->tc_Flags & TF_ETASK) || !task->tc_UnionETask.tc_ETask)
        return TRUE;

    iet = IntETask(task->tc_UnionETask.tc_ETask);
    if (iet->iet_CpuAffinity)
        return TRUE;

    if (!(aff = KrnAllocCPUMask()))
        return FALSE;
    KrnGetCPUMask(0, aff);
    iet->iet_CpuAffinity = aff;
    iet->iet_CpuNumber = 0;

    return TRUE;
}

static int Exec_RVSMPInit(struct ExecBase *SysBase)
{
    struct Task *task;
    BOOL ok = TRUE;
    void *sysmask;

    D(bug("[Exec] %s()\n", __func__));

    /* The CPUs exec may schedule on; more are added as harts come online */
    if (!(sysmask = KrnAllocCPUMask()))
    {
        bug("[Exec] %s: no memory for the system CPU mask - staying on one CPU\n", __func__);
        return TRUE;
    }
    KrnGetCPUMask(0, sysmask);
    PrivExecBase(SysBase)->CPUMask = sysmask;

    Disable();
    ok = Exec_RVAssignMask(GET_THIS_TASK);
    ForeachNode(&SysBase->TaskReady, task)
        ok = ok && Exec_RVAssignMask(task);
    ForeachNode(&SysBase->TaskWait, task)
        ok = ok && Exec_RVAssignMask(task);
    if (ok)
        PrivExecBase(SysBase)->IntFlags |= EXECF_CPUAffinity;
    Enable();

    /* The other harts are started later, by kernel.post (platform_post.c) */
    if (!ok)
        bug("[Exec] %s: out of memory for affinity masks - staying on one CPU\n", __func__);

    return TRUE;
}

/* Before the idle task (exec_idle.c, -127) so it is created pinned */
ADD2INITLIB(Exec_RVSMPInit, -126)

#endif /* __AROSEXEC_SMP__ */
