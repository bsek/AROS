#ifndef KERNEL_SPINLOCK_H_
#define KERNEL_SPINLOCK_H_
/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: ARMv7 atomic steps for the shared KrnSpinLock() body.
*/

#include <aros/types/spinlock_s.h>
#include <asm/arm/cpu.h>

#define KRNSPIN_ATOMICS

/* UPDATING is never set here, but counts as busy like on pc */
#define KRNSPIN_BUSY_FOR_READ   (SPINLOCKF_WRITE | SPINLOCKF_UPDATING)

static inline BOOL krnSpinTryWrite(spinlock_t *lock)
{
    unsigned long old, fail;

    asm volatile(
        "1:     ldrex   %0, [%2]        \n\t"
        "       teq     %0, #0          \n\t"
        "       bne     2f              \n\t"   /* held - give up */
        "       strex   %1, %3, [%2]    \n\t"
        "       teq     %1, #0          \n\t"
        "       bne     1b              \n\t"   /* lost exclusivity */
        "2:     clrex                   \n\t"
        : "=&r"(old), "=&r"(fail)
        : "r"(&lock->lock), "r"(SPINLOCKF_WRITE)
        : "cc", "memory");

    if (old != 0)
        return FALSE;

    dmb();
    return TRUE;
}

static inline BOOL krnSpinTryRead(spinlock_t *lock)
{
    unsigned long old, fail, tmp;

    asm volatile(
        "1:     ldrex   %0, [%3]        \n\t"
        "       tst     %0, %4          \n\t"
        "       bne     2f              \n\t"   /* write held - give up */
        "       add     %2, %0, #1      \n\t"
        "       strex   %1, %2, [%3]    \n\t"
        "       teq     %1, #0          \n\t"
        "       bne     1b              \n\t"
        "2:     clrex                   \n\t"
        : "=&r"(old), "=&r"(fail), "=&r"(tmp)
        : "r"(&lock->lock), "r"(KRNSPIN_BUSY_FOR_READ)
        : "cc", "memory");

    if (old & KRNSPIN_BUSY_FOR_READ)
        return FALSE;

    dmb();
    return TRUE;
}

/* dmb publishes the critical section, dsb keeps the sev behind the store */
static inline void krnSpinReleaseWrite(spinlock_t *lock)
{
    dmb();
    lock->lock = 0;
    dsb();
    sev();
}

static inline void krnSpinReleaseRead(spinlock_t *lock)
{
    unsigned long val, fail;

    dmb();
    asm volatile(
        "1:     ldrex   %0, [%2]        \n\t"
        "       sub     %0, %0, #1      \n\t"
        "       strex   %1, %0, [%2]    \n\t"
        "       teq     %1, #0          \n\t"
        "       bne     1b              \n\t"
        : "=&r"(val), "=&r"(fail)
        : "r"(&lock->lock)
        : "cc", "memory");

    if (val == 0)
    {
        dsb();
        sev();
    }
}

/*
 * Re-read before parking: a release after the read sets the event
 * register, so the wfe cannot miss it.
 */
static inline void krnSpinPause(spinlock_t *lock, ULONG busy)
{
    if (lock->lock & busy)
        asm volatile("wfe" ::: "memory");
}

#endif /* KERNEL_SPINLOCK_H_ */
