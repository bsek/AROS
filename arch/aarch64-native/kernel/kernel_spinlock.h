#ifndef KERNEL_SPINLOCK_H_
#define KERNEL_SPINLOCK_H_
/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: AArch64 atomic steps for the shared KrnSpinLock() body.
*/

#include <aros/types/spinlock_s.h>
#include <asm/cpu.h>

#define KRNSPIN_ATOMICS

/* UPDATING is never set here, but counts as busy like on pc */
#define KRNSPIN_BUSY_FOR_READ   (SPINLOCKF_WRITE | SPINLOCKF_UPDATING)

static inline BOOL krnSpinTryWrite(spinlock_t *lock)
{
    unsigned int old, fail;

    asm volatile(
        "1:     ldaxr   %w0, [%2]       \n\t"
        "       cbnz    %w0, 2f         \n\t"   /* held - give up */
        "       stxr    %w1, %w3, [%2]  \n\t"
        "       cbnz    %w1, 1b         \n\t"   /* lost exclusivity */
        "       b       3f              \n\t"
        "2:     clrex                   \n\t"
        "3:                             \n\t"
        : "=&r"(old), "=&r"(fail)
        : "r"(&lock->lock), "r"(SPINLOCKF_WRITE)
        : "memory");

    return (old == 0);
}

static inline BOOL krnSpinTryRead(spinlock_t *lock)
{
    unsigned int old, fail, tmp;

    asm volatile(
        "1:     ldaxr   %w0, [%3]       \n\t"
        "       tst     %w0, %w4        \n\t"
        "       b.ne    2f              \n\t"   /* write held - give up */
        "       add     %w2, %w0, #1    \n\t"
        "       stxr    %w1, %w2, [%3]  \n\t"
        "       cbnz    %w1, 1b         \n\t"
        "       b       3f              \n\t"
        "2:     clrex                   \n\t"
        "3:                             \n\t"
        : "=&r"(old), "=&r"(fail), "=&r"(tmp)
        : "r"(&lock->lock), "r"(KRNSPIN_BUSY_FOR_READ)
        : "memory", "cc");

    return !(old & KRNSPIN_BUSY_FOR_READ);
}

/* The dsb keeps the sev from overtaking the release */
static inline void krnSpinReleaseWrite(spinlock_t *lock)
{
    asm volatile("stlr wzr, [%0]" : : "r"(&lock->lock) : "memory");
    dsb();
    sev();
}

static inline void krnSpinReleaseRead(spinlock_t *lock)
{
    unsigned int val, fail;

    asm volatile(
        "1:     ldxr    %w0, [%2]       \n\t"
        "       sub     %w0, %w0, #1    \n\t"
        "       stlxr   %w1, %w0, [%2]  \n\t"
        "       cbnz    %w1, 1b         \n\t"
        : "=&r"(val), "=&r"(fail)
        : "r"(&lock->lock)
        : "memory");

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
