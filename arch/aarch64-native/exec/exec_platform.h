/*
    Copyright (C) 2015-2026, The AROS Development Team. All rights reserved.

    AArch64 exec platform definitions.
    TLS access via TPIDR_EL1 system register.
*/
#ifndef __EXEC_PLATFORM_H
#define __EXEC_PLATFORM_H

#include <aros/config.h>

#define SCHEDQUANTUM_VALUE      4

#if defined(__AROSEXEC_SMP__)
#include <aros/types/spinlock_s.h>
#include <utility/hooks.h>

#ifndef __KERNEL_NO_SPINLOCK_PROTOS__
extern void Kernel_49_KrnSpinInit(spinlock_t *, void *);
#endif
#define EXEC_SPINLOCK_INIT(a) Kernel_49_KrnSpinInit((a), NULL)
#ifndef __KERNEL_NO_SPINLOCK_PROTOS__
extern spinlock_t *Kernel_52_KrnSpinLock(spinlock_t *, struct Hook *, ULONG, void *);
#endif
/* (lock, failhook, mode) - rom/exec passes all three. */
#define EXEC_SPINLOCK_LOCK(a,b,c) Kernel_52_KrnSpinLock((a), (b), (c), NULL)
#ifndef __KERNEL_NO_SPINLOCK_PROTOS__
extern void Kernel_53_KrnSpinUnLock(spinlock_t *, void *);
#endif
#define EXEC_SPINLOCK_UNLOCK(a) Kernel_53_KrnSpinUnLock((a), NULL)

/* Fail hook for task-context spinlocks (platform_init.c) */
extern struct Hook Exec_TaskSpinLockFailHook;

/*
 * Store-store barrier for publishing a freshly built structure to readers
 * that walk it without taking a lock.
 */
#define EXEC_MEMORY_BARRIER()   asm volatile("dmb ishst" ::: "memory")

#include "kernel_syscall.h"

/* Tasks run at EL1t (SPSel 0), handlers at EL1h */
#define EXEC_IS_SUPER() \
    ({ unsigned long __spsel; \
       asm volatile("mrs %0, spsel" : "=r"(__spsel)); \
       (__spsel & 1); })

/* Run list changes are made in kernel context only */
extern void Exec_ReschedTask(struct Task *, ULONG);
#define krnSysCallReschedTask(task, state) \
    do { \
        if (EXEC_IS_SUPER()) \
            Exec_ReschedTask((task), (state)); \
        else \
        { \
            register IPTR __x0 asm("x0") = (IPTR)(task); \
            register IPTR __x1 asm("x1") = (IPTR)(state); \
            asm volatile("svc %[n]" : : "r"(__x0), "r"(__x1), \
                [n] "I"(SC_RESCHEDTASK) : "memory", "x30"); \
        } \
    } while(0)

/* Without this RemTask/ServiceTask mutate the scheduler lists unlocked. */
#define EXEC_REMTASK_NEEDSSWITCH

/* RemTask's suicide path: detach and tombstone, then RETURN so RemTask
 * can finish its teardown (KrnSwitch would never come back). */
extern void Exec_SuicideSwitch(void);
#define krnSysCallSwitch() \
    asm volatile("svc %[n]" : : [n] "I"(SC_SUICIDESWITCH) : "memory", "x30")

#endif

#include "tls.h"

struct Exec_PlatformData
{
    /* No platform-specific data by default */
};

/* AArch64: All TLS access uses TPIDR_EL1 via mrs/msr */

#if defined(AROS_NO_ATOMIC_OPERATIONS)
#define IDNESTCOUNT_INC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->IDNestCnt++; \
    } while(0)
#define IDNESTCOUNT_DEC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->IDNestCnt--; \
    } while(0)
#define TDNESTCOUNT_INC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->TDNestCnt++; \
    } while(0)
#define TDNESTCOUNT_DEC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->TDNestCnt--; \
    } while(0)
#define FLAG_SCHEDQUANTUM_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags &= ~TLSSF_Quantum; \
    } while(0)
#define FLAG_SCHEDQUANTUM_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags |= TLSSF_Quantum; \
    } while(0)
#define FLAG_SCHEDSWITCH_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags &= ~TLSSF_Switch; \
    } while(0)
#define FLAG_SCHEDSWITCH_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags |= TLSSF_Switch; \
    } while(0)
#define FLAG_SCHEDDISPATCH_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags &= ~TLSSF_Dispatch; \
    } while(0)
#define FLAG_SCHEDDISPATCH_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags |= TLSSF_Dispatch; \
    } while(0)
#else
#define IDNESTCOUNT_INC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_INC(__tls->IDNestCnt); \
    } while(0)
#define IDNESTCOUNT_DEC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_DEC(__tls->IDNestCnt); \
    } while(0)
#define TDNESTCOUNT_INC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_INC(__tls->TDNestCnt); \
    } while(0)
#define TDNESTCOUNT_DEC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_DEC(__tls->TDNestCnt); \
    } while(0)
#define FLAG_SCHEDQUANTUM_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_AND(__tls->ScheduleFlags, ~TLSSF_Quantum); \
    } while(0)
#define FLAG_SCHEDQUANTUM_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_OR(__tls->ScheduleFlags, TLSSF_Quantum); \
    } while(0)
#define FLAG_SCHEDSWITCH_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_AND(__tls->ScheduleFlags, ~TLSSF_Switch); \
    } while(0)
#define FLAG_SCHEDSWITCH_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_OR(__tls->ScheduleFlags, TLSSF_Switch); \
    } while(0)
#define FLAG_SCHEDDISPATCH_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_AND(__tls->ScheduleFlags, ~TLSSF_Dispatch); \
    } while(0)
#define FLAG_SCHEDDISPATCH_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        AROS_ATOMIC_OR(__tls->ScheduleFlags, TLSSF_Dispatch); \
    } while(0)
#endif
#define IDNESTCOUNT_GET \
    ({ \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        LONG __ret = (__tls->IDNestCnt); \
        __ret;  \
    })
#define IDNESTCOUNT_SET(val) \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->IDNestCnt = val; \
    } while(0)
#define TDNESTCOUNT_GET \
    ({ \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        LONG __ret = (__tls->TDNestCnt); \
        __ret;  \
    })
#define TDNESTCOUNT_SET(val) \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->TDNestCnt = val; \
    } while(0)
#define FLAG_SCHEDQUANTUM_ISSET \
    ({ \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        BOOL __ret = (__tls->ScheduleFlags & TLSSF_Quantum); \
        __ret;  \
    })
#define FLAG_SCHEDSWITCH_ISSET \
    ({ \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        BOOL __ret = (__tls->ScheduleFlags & TLSSF_Switch); \
        __ret;  \
    })
#define FLAG_SCHEDDISPATCH_ISSET \
    ({ \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        BOOL __ret = (__tls->ScheduleFlags & TLSSF_Dispatch); \
        __ret;  \
    })
#if defined(__AROSEXEC_SMP__)
/* Wait() asks the next switch to park the task on TaskWait */
#define FLAG_SCHEDPARK_SET \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags |= TLSSF_Park; \
    } while(0)
#define FLAG_SCHEDPARK_CLEAR \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->ScheduleFlags &= ~TLSSF_Park; \
    } while(0)
#define FLAG_SCHEDPARK_ISSET \
    ({ \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        BOOL __ret = (__tls->ScheduleFlags & TLSSF_Park) != 0; \
        __ret;  \
    })
#endif

/* TDNestCnt is per-CPU TLS, so no atomics. Blocks IRQ-exit dispatch
 * without Forbid()/Permit(). */
#define EXEC_BLOCK_DISPATCH_INC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->TDNestCnt++; \
    } while(0)

#define EXEC_BLOCK_DISPATCH_DEC \
    do { \
        tls_t *__tls; \
        asm volatile("mrs %0, tpidr_el1":"=r"(__tls)); \
        __tls->TDNestCnt--; \
    } while(0)

/* Raw IRQ+FIQ masking for the kernel-side scheduler code. Nests. */
#define EXEC_IRQFIQ_DISABLE() \
    ({ unsigned long __daif; \
       asm volatile("mrs %0, daif\n\tmsr daifset, #3" : "=r"(__daif) :: "memory"); \
       (unsigned int)(__daif & 0xc0); })

#define EXEC_IRQFIQ_RESTORE(prev) \
    do { \
        if (!((prev) & 0x80)) asm volatile("msr daifclr, #2" ::: "memory"); \
        if (!((prev) & 0x40)) asm volatile("msr daifclr, #1" ::: "memory"); \
    } while(0)

#define GET_THIS_TASK           TLS_GET(ThisTask)
#define SCHEDQUANTUM_SET(val)           TLS_SET(Quantum,(val))
#define SCHEDQUANTUM_GET                TLS_GET(Quantum)
#define SCHEDELAPSED_SET(val)           TLS_SET(Elapsed,(val))
#define SCHEDELAPSED_GET                TLS_GET(Elapsed)
#if !defined(__AROSEXEC_SMP__)
#define SET_THIS_TASK(x)        TLS_SET(ThisTask,(x))
#else
#define SET_THIS_TASK(x)        TLS_SET(ThisTask,(x)); \
    KrnSpinLock(&PrivExecBase(SysBase)->TaskRunningSpinLock, NULL, SPINLOCK_MODE_WRITE); \
    AddHead(&PrivExecBase(SysBase)->TaskRunning, (struct Node *)(x)); \
    KrnSpinUnLock(&PrivExecBase(SysBase)->TaskRunningSpinLock)
#endif

struct Task *Exec_ARMCreateIdleTask(struct ExecBase *);

#endif /* __EXEC_PLATFORM_H */
