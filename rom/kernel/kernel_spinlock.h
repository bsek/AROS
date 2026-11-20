#ifndef KERNEL_SPINLOCK_H_
#define KERNEL_SPINLOCK_H_
/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Atomic steps for the shared KrnSpinLock() body.

    A port with real spinlocks overrides this header, defines
    KRNSPIN_ATOMICS and provides, on the <aros/types/spinlock_s.h> layout:

    BOOL krnSpinTryWrite(spinlock_t *)      one write attempt, acquire
    BOOL krnSpinTryRead(spinlock_t *)       one read attempt, acquire
    void krnSpinReleaseWrite(spinlock_t *)  release, wakes waiters
    void krnSpinReleaseRead(spinlock_t *)   drop one reader, wakes waiters
    void krnSpinPause(spinlock_t *, ULONG)  wait a little while any of
                                            the given bits are set

    Without it the spinlock functions do nothing.
*/

#include <aros/types/spinlock_s.h>

#endif /* KERNEL_SPINLOCK_H_ */
