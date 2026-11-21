/*
    Copyright (C) 2013, The AROS Development Team. All rights reserved.
*/

#include <aros/kernel.h>

#include <kernel_base.h>
#include <kernel_syscall.h>

#include <proto/kernel.h>

/* See rom/kernel/cli.c for documentation */

AROS_LH0I(void, KrnCli,
    struct KernelBase *, KernelBase, 9, Kernel)
{
    AROS_LIBFUNC_INIT

    unsigned int cpsr;

    /* Privileged modes mask directly, a nested swi would clobber lr_svc */
    asm volatile("mrs %0, cpsr" : "=r"(cpsr));
    if ((cpsr & 0x1f) != 0x10)
        asm volatile("cpsid if" ::: "memory");
    else
        krnSysCall(SC_CLI);

    AROS_LIBFUNC_EXIT
}
