#ifndef KERNEL_SPINLOCK_H_
#define KERNEL_SPINLOCK_H_
/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: x86 atomic steps for the shared KrnSpinLock() body.
*/

#include <aros/types/spinlock_s.h>
#include <asm/cpu.h>

#define KRNSPIN_ATOMICS

/* UPDATING is no longer set, but stays reserved and counts as busy */
#define KRNSPIN_BUSY_FOR_READ   (SPINLOCKF_WRITE | SPINLOCKF_UPDATING)

static inline BOOL krnSpinTryWrite(spinlock_t *lock)
{
    return compare_and_exchange_long((ULONG *)&lock->lock,
        SPINLOCK_UNLOCKED, SPINLOCKF_WRITE, NULL);
}

static inline BOOL krnSpinTryRead(spinlock_t *lock)
{
    ULONG old;

    for (;;)
    {
        old = lock->lock;
        if (old & KRNSPIN_BUSY_FOR_READ)
            return FALSE;
        if (compare_and_exchange_long((ULONG *)&lock->lock, old, old + 1, NULL))
            return TRUE;
        asm volatile("pause");
    }
}

static inline void krnSpinReleaseWrite(spinlock_t *lock)
{
    compare_and_exchange_long((ULONG *)&lock->lock,
        SPINLOCKF_WRITE, SPINLOCK_UNLOCKED, NULL);
}

static inline void krnSpinReleaseRead(spinlock_t *lock)
{
    ULONG old;

    do
    {
        old = lock->lock;
        /* Unlocking an unlocked lock */
        if (!(old & 0x00ffffff))
            return;
    } while (!compare_and_exchange_long((ULONG *)&lock->lock, old, old - 1, NULL));
}

static inline void krnSpinPause(spinlock_t *lock, ULONG busy)
{
    (void)lock;
    (void)busy;
    asm volatile("pause");
}

#endif /* KERNEL_SPINLOCK_H_ */
