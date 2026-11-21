/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Native RISC-V exec platform definitions.

    Uniprocessor builds keep the per-CPU scheduler state in ExecBase,
    exactly as rom/exec/exec_platform.h does. SMP builds keep it in the
    per-hart TLS that tp points to (see kernel/tls.h), following the
    aarch64-native port.
*/
#ifndef __RISCV_EXEC_PLATFORM_H
#define __RISCV_EXEC_PLATFORM_H

#include <aros/config.h>

#if !defined(__AROSEXEC_SMP__)

/* Uniprocessor: the generic definitions (they guard on __EXEC_PLATFORM_H) */
#include "../../../rom/exec/exec_platform.h"

#else /* __AROSEXEC_SMP__ */

#define __EXEC_PLATFORM_H

#define SCHEDQUANTUM_VALUE      4

#include <aros/atomic.h>
#include <asm/riscv/csr.h>   /* rv_intr_disable/restore, not the generic helpers */
#include <aros/types/spinlock_s.h>
#include <utility/hooks.h>

extern void Kernel_49_KrnSpinInit(spinlock_t *, void *);
#define EXEC_SPINLOCK_INIT(a) Kernel_49_KrnSpinInit((a), NULL)
extern spinlock_t *Kernel_52_KrnSpinLock(spinlock_t *, struct Hook *, ULONG, void *);
/* (lock, failhook, mode) - rom/exec passes all three */
#define EXEC_SPINLOCK_LOCK(a,b,c) Kernel_52_KrnSpinLock((a), (b), (c), NULL)
extern void Kernel_53_KrnSpinUnLock(spinlock_t *, void *);
#define EXEC_SPINLOCK_UNLOCK(a) Kernel_53_KrnSpinUnLock((a), NULL)

/*
 * Store-store barrier for publishing a freshly built structure to
 * readers that walk it without taking a lock.
 */
#define EXEC_MEMORY_BARRIER()   asm volatile("fence w, w" ::: "memory")

#include "kernel_syscall.h"

/* Run list changes are made in kernel context only. Scheduler syscalls
 * are ebreak with the code in a7 (see kernel_cpu.h). */
extern int Kernel_13_KrnIsSuper();
extern void Exec_ReschedTask(struct Task *, ULONG);
#define krnSysCallReschedTask(task, state) \
    do { \
        if (Kernel_13_KrnIsSuper()) \
            Exec_ReschedTask((task), (state)); \
        else \
        { \
            register IPTR __a0 asm("a0") = (IPTR)(task); \
            register IPTR __a1 asm("a1") = (IPTR)(state); \
            asm volatile("addi a7, zero, %[n]\n\tebreak" \
                : : "r"(__a0), "r"(__a1), [n] "I"(SC_RESCHEDTASK) \
                : "a7", "memory"); \
        } \
    } while(0)

/* Without this RemTask/ServiceTask mutate the scheduler lists unlocked */
#define EXEC_REMTASK_NEEDSSWITCH

/* RemTask's suicide path: detach and tombstone, then RETURN so RemTask
 * can finish its teardown (KrnSwitch would never come back) */
extern void Exec_SuicideSwitch(void);
#define krnSysCallSwitch() \
    asm volatile("addi a7, zero, %[n]\n\tebreak" \
        : : [n] "I"(SC_SUICIDESWITCH) : "a7", "memory")

/*
 * Interrupt masking for the kernel-side scheduler code. The IPI is an
 * ordinary interrupt here, so IRQ+FIQ is just the IE bit. Nests.
 */
#define EXEC_IRQFIQ_DISABLE()       rv_intr_disable()
#define EXEC_IRQFIQ_RESTORE(prev)   rv_intr_restore(prev)

#include "tls.h"

struct Exec_PlatformData
{
    /* No platform-specific data by default */
};

#define __RV_TLS() \
    ({ tls_t *__tls; asm volatile("mv %0, tp" : "=r"(__tls)); __tls; })

#if defined(AROS_NO_ATOMIC_OPERATIONS)
#define IDNESTCOUNT_INC             do { __RV_TLS()->IDNestCnt++; } while(0)
#define IDNESTCOUNT_DEC             do { __RV_TLS()->IDNestCnt--; } while(0)
#define TDNESTCOUNT_INC             do { __RV_TLS()->TDNestCnt++; } while(0)
#define TDNESTCOUNT_DEC             do { __RV_TLS()->TDNestCnt--; } while(0)
#define FLAG_SCHEDQUANTUM_CLEAR     do { __RV_TLS()->ScheduleFlags &= ~TLSSF_Quantum; } while(0)
#define FLAG_SCHEDQUANTUM_SET       do { __RV_TLS()->ScheduleFlags |= TLSSF_Quantum; } while(0)
#define FLAG_SCHEDSWITCH_CLEAR      do { __RV_TLS()->ScheduleFlags &= ~TLSSF_Switch; } while(0)
#define FLAG_SCHEDSWITCH_SET        do { __RV_TLS()->ScheduleFlags |= TLSSF_Switch; } while(0)
#define FLAG_SCHEDDISPATCH_CLEAR    do { __RV_TLS()->ScheduleFlags &= ~TLSSF_Dispatch; } while(0)
#define FLAG_SCHEDDISPATCH_SET      do { __RV_TLS()->ScheduleFlags |= TLSSF_Dispatch; } while(0)
#else
#define IDNESTCOUNT_INC             AROS_ATOMIC_INC(__RV_TLS()->IDNestCnt)
#define IDNESTCOUNT_DEC             AROS_ATOMIC_DEC(__RV_TLS()->IDNestCnt)
#define TDNESTCOUNT_INC             AROS_ATOMIC_INC(__RV_TLS()->TDNestCnt)
#define TDNESTCOUNT_DEC             AROS_ATOMIC_DEC(__RV_TLS()->TDNestCnt)
#define FLAG_SCHEDQUANTUM_CLEAR     AROS_ATOMIC_AND(__RV_TLS()->ScheduleFlags, ~TLSSF_Quantum)
#define FLAG_SCHEDQUANTUM_SET       AROS_ATOMIC_OR(__RV_TLS()->ScheduleFlags, TLSSF_Quantum)
#define FLAG_SCHEDSWITCH_CLEAR      AROS_ATOMIC_AND(__RV_TLS()->ScheduleFlags, ~TLSSF_Switch)
#define FLAG_SCHEDSWITCH_SET        AROS_ATOMIC_OR(__RV_TLS()->ScheduleFlags, TLSSF_Switch)
#define FLAG_SCHEDDISPATCH_CLEAR    AROS_ATOMIC_AND(__RV_TLS()->ScheduleFlags, ~TLSSF_Dispatch)
#define FLAG_SCHEDDISPATCH_SET      AROS_ATOMIC_OR(__RV_TLS()->ScheduleFlags, TLSSF_Dispatch)
#endif

#define IDNESTCOUNT_GET             ((LONG)__RV_TLS()->IDNestCnt)
#define IDNESTCOUNT_SET(val)        do { __RV_TLS()->IDNestCnt = (val); } while(0)
#define TDNESTCOUNT_GET             ((LONG)__RV_TLS()->TDNestCnt)
#define TDNESTCOUNT_SET(val)        do { __RV_TLS()->TDNestCnt = (val); } while(0)
#define FLAG_SCHEDQUANTUM_ISSET     ((BOOL)((__RV_TLS()->ScheduleFlags & TLSSF_Quantum) != 0))
#define FLAG_SCHEDSWITCH_ISSET      ((BOOL)((__RV_TLS()->ScheduleFlags & TLSSF_Switch) != 0))
#define FLAG_SCHEDDISPATCH_ISSET    ((BOOL)((__RV_TLS()->ScheduleFlags & TLSSF_Dispatch) != 0))

/* Wait() asks the next switch to park the task on TaskWait */
#define FLAG_SCHEDPARK_SET          do { __RV_TLS()->ScheduleFlags |= TLSSF_Park; } while(0)
#define FLAG_SCHEDPARK_CLEAR        do { __RV_TLS()->ScheduleFlags &= ~TLSSF_Park; } while(0)
#define FLAG_SCHEDPARK_ISSET        ((BOOL)((__RV_TLS()->ScheduleFlags & TLSSF_Park) != 0))

/* TDNestCnt is per-CPU TLS, so no atomics. Blocks IRQ-exit dispatch
 * without Forbid()/Permit() */
#define EXEC_BLOCK_DISPATCH_INC     do { __RV_TLS()->TDNestCnt++; } while(0)
#define EXEC_BLOCK_DISPATCH_DEC     do { __RV_TLS()->TDNestCnt--; } while(0)

#define GET_THIS_TASK                   TLS_GET(ThisTask)
#define SCHEDQUANTUM_SET(val)           TLS_SET(Quantum,(val))
#define SCHEDQUANTUM_GET                TLS_GET(Quantum)
#define SCHEDELAPSED_SET(val)           TLS_SET(Elapsed,(val))
#define SCHEDELAPSED_GET                TLS_GET(Elapsed)
#define SET_THIS_TASK(x)        TLS_SET(ThisTask,(x)); \
    KrnSpinLock(&PrivExecBase(SysBase)->TaskRunningSpinLock, NULL, SPINLOCK_MODE_WRITE); \
    AddHead(&PrivExecBase(SysBase)->TaskRunning, (struct Node *)(x)); \
    KrnSpinUnLock(&PrivExecBase(SysBase)->TaskRunningSpinLock)

#endif /* __AROSEXEC_SMP__ */

#endif /* __RISCV_EXEC_PLATFORM_H */
