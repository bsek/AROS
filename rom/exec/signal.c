/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Send some signal to a given task
*/

#define DEBUG 0
#include <aros/debug.h>

#include <exec/execbase.h>
#include <aros/libcall.h>
#include <proto/exec.h>

#define __AROS_KERNEL__
#include "exec_intern.h"

#if defined(__AROSEXEC_SMP__)
#include <aros/atomic.h>
#include <utility/hooks.h>

#include "etask.h"
#include "kernel_ipi.h"

AROS_UFH3(IPTR, signal_hook,
    AROS_UFHA(struct IPIHook *, hook, A0),
    AROS_UFHA(APTR, object, A2),
    AROS_UFHA(APTR, message, A1)
)
{
    AROS_USERFUNC_INIT

    struct ExecBase *SysBase = (struct ExecBase *)hook->ih_Args[0];
    struct Task *target = (struct Task *)hook->ih_Args[1];
    ULONG sigset = (ULONG)hook->ih_Args[2];

    D(
        struct KernelBase *KernelBase = __kernelBase;
        int cpunum = KrnGetCPUNumber();
        bug("[Exec] CPU%03d: Using IPI to do Signal(%p, %08x), SysBase=%p\n", cpunum, target, sigset, SysBase);
    );

    Signal(target, sigset);

    return 0;

    AROS_USERFUNC_EXIT
}

/*
 * Called from RemTask() once the task is terminal, before its memory can
 * go away, so no core drains a queued Signal-IPI naming a freed task.
 */
void Exec_CancelSignalIPIs(struct Task *task, struct ExecBase *SysBase)
{
#if defined(KERNEL_IPI_CALL_CANCELABLE)
    /*
     * The task is terminal now, and core_CommitCallIPI() checks that
     * under the queue lock, so nothing new is queued after this sweep.
     */
    Disable();
    core_CancelCallIPIs((APTR)signal_hook, (IPTR)task);
    Enable();
#endif
}
#endif

/*****************************************************************************

    NAME */

#if defined(__mc68000)
void Exec_SignalSlow(struct Task *task, ULONG signalSet, struct ExecBase *SysBase)
#else
        AROS_LH2(void, Signal,

/*  SYNOPSIS */
        AROS_LHA(struct Task *,     task,      A1),
        AROS_LHA(ULONG,             signalSet, D0),

/*  LOCATION */
        struct ExecBase *, SysBase, 54, Exec)
#endif

/*  FUNCTION
        Send some signals to a given task. If the task is currently waiting
        on these signals, has a higher priority as the current one and if
        taskswitches are allowed the new task begins to run immediately.

    INPUTS
        task      - Pointer to task structure.
        signalSet - The set of signals to send to the task.

    RESULT

    NOTES
        This function may be used from interrupts.

    EXAMPLE

    BUGS

    SEE ALSO
        AllocSignal(), FreeSignal(), Wait(), SetSignal(), SetExcept()

    INTERNALS

    HISTORY

******************************************************************************/
{
#if !defined(__mc68000)
    AROS_LIBFUNC_INIT
#endif

    struct Task *thisTask = GET_THIS_TASK;

#if defined(__AROSEXEC_SMP__)
    struct KernelBase *KernelBase = __kernelBase;
    int cpunum = KrnGetCPUNumber();

    /* Never forward to a task being torn down, as RemTask() may be
     * waiting for this very call on another CPU */
    if (task->tc_State == TS_INVALID ||
        task->tc_State == TS_REMOVED ||
        task->tc_State == TS_TOMBSTONED)
    {
        __AROS_ATOMIC_OR_L(task->tc_SigRecvd, signalSet);
        return;
    }

    /*
        * # If current CPU number is not the task's CPU and the task is running now, send signal to that task
        *   from CPU which the task is running on.
        * # If task is not running and the current CPU is not in the Affinitymask, send signal to CPU form Affinity mask
        * # If task is not running and the current CPU is in the Affinity mask, just proceed with regular signal
    */
    if ((PrivExecBase(SysBase)->IntFlags & EXECF_CPUAffinity) &&
        ((IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuNumber != cpunum && task->tc_State == TS_RUN) ||
        !KrnCPUInMask(cpunum, IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity)))
    {
        struct Hook h;
        IPTR args[3];
        int targetcpu;

        args[0] = (IPTR)SysBase;
        args[1] = (IPTR)task;
        args[2] = (IPTR)signalSet;

        /* Task is running *now* on another CPU, send signal there */
        if (task->tc_State == TS_RUN)
        {
            targetcpu = IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuNumber;
        }
        else
        {
            int i;
            int cpumax = KrnGetCPUCount();

            /* Task is not running now, find first cpu suitable to run this task. Use CPU balancing some day... */
            targetcpu = 0;
            for (i=0; i < cpumax; i++)
            {
                if (KrnCPUInMask(i, IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity))
                {
                    targetcpu = i;
                    break;
                }
            }
        }

        D(bug("[Exec] %s: Signaling from CPU%03d -> CPU%03d using IPI...\n", __func__, cpunum, targetcpu));

        h.h_Entry = signal_hook;

#if defined(KERNEL_IPI_CALL_CANCELABLE)
        {
            struct CallIPIEntry *cie;

            /*
             * The queued call outlives this function. The commit refuses
             * a terminal task under the queue lock, so teardown's sweep
             * cannot miss it.
             */
            Disable();
            cie = core_ClaimCallIPI(targetcpu);
            if (!core_CommitCallIPI(cie, targetcpu, &h, 3, args, task))
                __AROS_ATOMIC_OR_L(task->tc_SigRecvd, signalSet);
            Enable();
        }
#else
        {
            // We cannot use KrnAllocCPUMask() since this function uses AllocMem
            // And we cannot use AllocMem from interrupts (where Signal() is allowed)...
            ULONG mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; // CPU mask large enough for 256 CPUs...
            void *cpu_mask = &mask;

            KrnGetCPUMask(targetcpu, cpu_mask);

            D(bug("[Exec] %s: Sending IPI...\n", __func__));
            core_DoCallIPI(&h, cpu_mask, 1, 3, args, (void *)KernelBase);
        }
#endif
        D(bug("[Exec] %s: IPI Sent\n", __func__));
    }
    else
    {

        if (cpunum != 0)
        {
            D(bug("[Exec] %s(0x%p, %08lX) on CPU%03d\n", __func__, task, signalSet, cpunum));
        }
#else
        D(bug("[Exec] %s(0x%p, %08lX)\n", __func__, task, signalSet);)
#endif

        Disable();
#if defined(__AROSEXEC_SMP__)
        /*
         * A task mid-teardown may be on no scheduler list, so
         * rescheduling it would Remove() from the wrong one. Record the
         * signal and leave.
         */
        if (task->tc_State == TS_INVALID ||
            task->tc_State == TS_REMOVED ||
            task->tc_State == TS_TOMBSTONED)
        {
            __AROS_ATOMIC_OR_L(task->tc_SigRecvd, signalSet);
            Enable();
            return;
        }
#endif
        D(
            bug("[Exec] %s: Signaling 0x%p '%s', state = %08x\n", __func__, task, task->tc_Node.ln_Name, task->tc_State);
#if (0)
            if (((struct KernelBase *)KernelBase)->kb_ICFlags & KERNBASEF_IRQPROCESSING)
                bug("[Exec] Signal: (Called from Interrupt)\n");
            else
#endif
                bug("[Exec] %s: (Called from '%s')\n", __func__, thisTask->tc_Node.ln_Name);
        )

        D(bug("[Exec] %s: Target signal flags : %08x ->", __func__, task->tc_SigRecvd);)
        /* Set the signals in the task structure. */
#if defined(__AROSEXEC_SMP__)
        __AROS_ATOMIC_OR_L(task->tc_SigRecvd, signalSet);
        /* Bits before state; the switch that parks reads them after. */
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
        task->tc_SigRecvd |= signalSet;
#endif
        D(bug(" %08x\n", task->tc_SigRecvd);)

        /* Do those bits raise exceptions? */
        if (task->tc_SigRecvd & task->tc_SigExcept)
        {
            /* Yes. Set the exception flag. */
#if defined(__AROSEXEC_SMP__)
            __AROS_ATOMIC_OR_B(task->tc_Flags, TF_EXCEPT);
#else
            task->tc_Flags |= TF_EXCEPT;
#endif
            D(bug("[Exec] %s: TF_EXCEPT set\n", __func__);)

            /*
                    if the target task is running (called from within interrupt handler, or from another processor),
                    raise the exception or defer it for later.
                */
            if (task->tc_State == TS_RUN)
            {
#if defined(__AROSEXEC_SMP__)
                BOOL sameCpu = (!(PrivExecBase(SysBase)->IntFlags & EXECF_CPUAffinity) ||
                                (IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuNumber == cpunum));
                void *targetCpuAffinity = IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity;
                if (sameCpu)
                {
                    D(bug("[Exec] %s: calling Reschedule to raise Exception for RUN Task\n", __func__);)
                    Reschedule();
                }
                else
                {
                    /* Use the cached affinity - the ETask may be gone. */
                    D(bug("[Exec] %s: Raising Exception for RUN Task on another CPU\n", __func__));
                    KrnScheduleCPU(targetCpuAffinity);
                }
#else
                D(bug("[Exec] %s: calling Reschedule to raise Exception for RUN Task\n", __func__);)
                /* Order a reschedule */
                Reschedule();
#endif
                Enable();

                /* All done. */
                return;
            }
        }

        /* Does the target task have signals to process ? */
        if (task->tc_SigRecvd & (task->tc_SigWait | task->tc_SigExcept))
        {
            if (task->tc_State == TS_WAIT)
            {
                D(
                bug("[Exec] %s: Signaling WAIT Task 0x%p '%s' pri %d\n", __func__, task, task->tc_Node.ln_Name, task->tc_Node.ln_Pri);
                )
                /* Yes. Move it to the ready list. */
#if defined(__AROSEXEC_SMP__)
                /*
                 * Once the task is made READY another core can run and
                 * even free it, so read what we need first and do not
                 * touch *task afterwards.
                 */
                {
                    BYTE targetPri = task->tc_Node.ln_Pri;
                    BOOL onThisCpu = (!(PrivExecBase(SysBase)->IntFlags & EXECF_CPUAffinity) ||
                                      KrnCPUInMask(cpunum, IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity));
                    void *targetCpuAffinity = IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity;

                    krnSysCallReschedTask(task, TS_READY);

                    if (!onThisCpu)
                    {
                        D(bug("[Exec] %s: Signaling task on another CPU\n", __func__));
                        KrnScheduleCPU(targetCpuAffinity);
                    }
                    else if ((targetPri > thisTask->tc_Node.ln_Pri) &&
                             (thisTask->tc_State == TS_RUN))
                    {
                        D(bug("[Exec] %s: Task has higher priority ...\n", __func__);)
                        Reschedule();
                    }
                    Enable();
                    return;
                }
#else
                Remove(&task->tc_Node);
                task->tc_State = TS_READY;
                Enqueue(&SysBase->TaskReady, &task->tc_Node);
#endif
            }

            if (task->tc_State == TS_READY)
            {
                /* Has it a higher priority than the running task? */
                if (
#if defined(__AROSEXEC_SMP__)
                    (!(PrivExecBase(SysBase)->IntFlags & EXECF_CPUAffinity) ||
                    (KrnCPUInMask(cpunum, IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity))) &&
#endif
                    (task->tc_Node.ln_Pri > thisTask->tc_Node.ln_Pri))
                {
                    D(bug("[Exec] %s: Task has higher priority ...\n", __func__);)
                    /*
                        Yes. A taskswitch is necessary. Prepare one if possible.
                        (If the current task is not running it is already moved)
                    */
                    if (thisTask->tc_State == TS_RUN)
                    {
                        D(
                            bug("[Exec] %s: Rescheduling RUN Task 0x%p '%s' pri %d,  to let 0x%p '%s' process the signal...\n", __func__, thisTask, thisTask->tc_Node.ln_Name, thisTask->tc_Node.ln_Pri, task, task->tc_Node.ln_Name);
                        )
#if defined(__AROSEXEC_SMP__)
                        Reschedule();
                        Enable();
                        return;
#else
                        Reschedule();
                        D(
                            bug("[Exec] %s: returned to task 0x%p '%s' pri %d\n", __func__, thisTask, thisTask->tc_Node.ln_Name, thisTask->tc_Node.ln_Pri);
                        )
#endif
                    }
                }
#if defined(__AROSEXEC_SMP__)
                else if ((PrivExecBase(SysBase)->IntFlags & EXECF_CPUAffinity) &&
                    !(KrnCPUInMask(cpunum, IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity)))
                {
                    void *targetCpuAffinity = IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity;
                    D(bug("[Exec] %s: Signaling task on another CPU\n", __func__));
                    krnSysCallReschedTask(task, TS_READY);
                    KrnScheduleCPU(targetCpuAffinity);
                    Enable();
                    return;
                }
#endif
                D(else bug("[Exec] %s: Letting Task process signal when next scheduled to run...\n", __func__);)
            }
        }

        Enable();

        D(
            bug("[Exec] %s: 0x%p finished signal processing\n", __func__, task);
        )
#if defined(__AROSEXEC_SMP__)
    }
#endif

#if !defined(__mc68000)
    AROS_LIBFUNC_EXIT
#endif
}
