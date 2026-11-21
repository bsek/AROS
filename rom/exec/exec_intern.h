/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Private data belonging to exec.library
*/
#ifndef __EXEC_INTERN_H__
#define __EXEC_INTERN_H__

/* This is a short file that contains a few things every Exec function needs */

#include <aros/debug.h> /* Needed for aros_print_not_implemented macro */
#include <aros/system.h>
#include <aros/libcall.h>
#include <exec/execbase.h>
#include <proto/exec.h>

#include <exec_platform.h>

/*
 * Store-store barrier, for publishing a freshly built structure to a reader
 * that walks it lock-free. SMP arches define a real one in exec_platform.h;
 * on single-CPU builds there is no other observer, so a no-op is correct.
 */
#ifndef EXEC_MEMORY_BARRIER
#define EXEC_MEMORY_BARRIER()   do { } while (0)
#endif

#if defined(__AROSEXEC_SMP__)
#include <aros/types/spinlock_s.h>
#endif

#ifndef __KERNEL_NOLIBBASE__
#define __KERNEL_NOLIBBASE__
#endif
#include <proto/kernel.h>

#define ALERT_BUFFER_SIZE 2048

#if !defined(HAVE_ExecDoInitResident)
#define HAVE_ExecDoInitResident
#define ExecDoInitResident(a,b,c) \
({ \
        a = AROS_UFC3(struct Library *, (b), \
        AROS_UFCA(struct Library *,  0L, D0), \
        AROS_UFCA(BPTR,              (c), A0), \
        AROS_UFCA(struct ExecBase *, SysBase, A6) \
    ); \
})
#endif


/*
   Internals of this structure are host-specific, we don't know them here
 */
struct HostInterface;

struct SupervisorAlertTask
{
    struct Task                 *sat_Task;                      /* Task that tries to display supervisor-level alerts           */
    BOOL                        sat_IsAvailable;
    IPTR                        sat_Params[2];
};

/*
   AROS specific private portion of ExecBase
 */
/*
 * Private soft interrupt node states (cause.c). NT_SOFTINT means queued;
 * these two mean "a CPU is running it" and "running, run it again when
 * done". Cause() treats anything else as free.
 */
#define NT_SOFTINT_RUNNING  (NT_USER - 2)
#define NT_SOFTINT_PENDING  (NT_USER - 3)

struct IntExecBase
{
    struct ExecBase             pub;
    struct List                 ResetHandlers;                  /* Reset handlers list                                          */
    struct Interrupt            ColdResetHandler;               /* Reset handler that causes cold reboot                        */
    struct Interrupt            WarmResetHandler;               /* Reset handler that causes warm reboot                        */
    struct Interrupt            ShutdownHandler;                /* Reset handler that halts CPU                                 */
    struct MinList              AllocMemList;                   /* Mungwall allocations list                                    */
    struct SignalSemaphore      LowMemSem;                      /* Lock for single-threading low memory handlers                */
    APTR                        KernelBase;                     /* kernel.resource base                                         */
    struct Library              *DebugBase;                     /* debug.library base                                           */
    ULONG                       PageSize;                       /* Memory page size                                             */
    ULONG                       IntFlags;                       /* Internal flags, see below                                    */
    struct MsgPort              *ServicePort;                   /* Message port for service task                                */
    struct List                 AllocatorCtxList;               /* List of allocator contexts for system mem headers            */
    struct Exec_PlatformData    PlatformData;                   /* Platform-specific stuff                                      */
    struct SupervisorAlertTask  SAT;
    ULONG                       SupervisorDeadEndCnt;           /* Counter of reaching AT_DeadEnd under Supervisor mode         */
    char                        AlertBuffer[ALERT_BUFFER_SIZE]; /* Buffer for alert text                                        */
    void                       *ExecLogBase;
    struct Task                *ResetCallbackWaiter;           /* Launcher awaiting the running reset handler                 */
    ULONG                       ResetCallbackSignal;           /* .. and the signal it waits on                               */
    struct Task                *ResetCallbackTask;             /* The handler task itself, while it is still inside the call  */
#if defined(__AROSEXEC_BROKENMEMLOCK__)
    struct SignalSemaphore      MemListSem;                     /* Memory list protection semaphore                             */
#elif defined(__AROSEXEC_SMP__)
    void                       *ExecLockBase;
    cpumask_t                   *CPUMask;                       /* bitmap of online core                                        */
    spinlock_t                  MemListSpinLock;
    spinlock_t                  AllocMemListSpinLock;           /* mungwall AllocMemList (SMP-safe replacement for Forbid)       */
    spinlock_t                  AllocatorCtxListSpinLock;       /* AllocatorCtxList lazy-create serialization                   */
    /* First the locks for arbitration of public resources ... */
    spinlock_t                  ResourceListSpinLock;
    spinlock_t                  DeviceListSpinLock;
    spinlock_t                  IntrListSpinLock;
    spinlock_t                  LibListSpinLock;
    spinlock_t                  PortListSpinLock;
    spinlock_t                  SemListSpinLock;

    /* .. and then scheduling related locks ... */
    spinlock_t                  TaskRunningSpinLock;
    struct List                 TaskRunning;                    /* Tasks that are currently running on available CPUs           */
    spinlock_t                  TaskSpinningLock;
    struct List                 TaskSpinning;                   /* Tasks that are spinning waiting for a lock                   */
    spinlock_t                  TaskReadySpinLock;
    spinlock_t                  TaskWaitSpinLock;
#endif
};

#define PrivExecBase(base)      ((struct IntExecBase *)(base))
#define PD(base)                PrivExecBase(base)->PlatformData
#ifndef __AROS_KERNEL__
#define KernelBase              PrivExecBase(SysBase)->KernelBase
#else
#define __kernelBase            PrivExecBase(SysBase)->KernelBase
#endif
#if !defined(DEBUG_NOPRIVATEINLINE)
#define DebugBase               PrivExecBase(SysBase)->DebugBase
#endif

/* IntFlags */
#define EXECB_MungWall          0                                /* This flag can't be changed at runtime                        */
#define EXECF_MungWall          (1 << EXECB_MungWall)
#define EXECB_StackSnoop        1
#define EXECF_StackSnoop        (1 << EXECB_StackSnoop)
#define EXECB_CPUAffinity       2                                /* Set once the CPU affinity masks should be used               */
#define EXECF_CPUAffinity       (1 << EXECB_CPUAffinity)

/* Additional private task states */
#define TS_SERVICE              128

#if UseLVOs
extern void __AROS_InitExecBase (void);
#endif

struct ExecBase *PrepareExecBase(struct MemHeader *mh, struct TagItem *tags);
void InitExecBase(struct ExecBase *SysBase, ULONG negsize, struct TagItem *msg);
struct ExecBase *PrepareExecBaseMove(struct ExecBase *oldSysBase);
BOOL Exec_PreparePlatform(struct Exec_PlatformData *pdata, struct TagItem *tags);

void InitKickTags(struct ExecBase *SysBase);
UWORD GetSysBaseChkSum(struct ExecBase *sysbase);
void SetSysBaseChkSum(void);
BOOL IsSysBaseValid(struct ExecBase *sysbase);

IPTR cpu_SuperState();

void ServiceTask(struct ExecBase *SysBase);

#if defined(__AROSEXEC_SMP__)

/*
 * Run list moves, kernel context only, with interrupts masked by the
 * caller. tc_State and list membership change together, under the locks
 * of both lists, taken in index order. Anyone else reads tc_State, locks
 * that state's list and checks tc_State again before trusting it.
 */
#define EXEC_TASKLIST_NONE      (-1)
#define EXEC_TASKLIST_RUNNING   0
#define EXEC_TASKLIST_READY     1
#define EXEC_TASKLIST_WAIT      2
#define EXEC_TASKLIST_SPINNING  3

static inline int exec_TaskListOf(ULONG state)
{
    switch (state)
    {
        case TS_RUN:    return EXEC_TASKLIST_RUNNING;
        case TS_READY:  return EXEC_TASKLIST_READY;
        case TS_WAIT:   return EXEC_TASKLIST_WAIT;
        case TS_SPIN:   return EXEC_TASKLIST_SPINNING;
    }
    return EXEC_TASKLIST_NONE;
}

static inline spinlock_t *exec_TaskListLock(int list)
{
    switch (list)
    {
        case EXEC_TASKLIST_RUNNING: return &PrivExecBase(SysBase)->TaskRunningSpinLock;
        case EXEC_TASKLIST_READY:   return &PrivExecBase(SysBase)->TaskReadySpinLock;
        case EXEC_TASKLIST_WAIT:    return &PrivExecBase(SysBase)->TaskWaitSpinLock;
    }
    return &PrivExecBase(SysBase)->TaskSpinningLock;
}

static inline struct List *exec_TaskList(int list)
{
    switch (list)
    {
        case EXEC_TASKLIST_RUNNING: return &PrivExecBase(SysBase)->TaskRunning;
        case EXEC_TASKLIST_READY:   return &SysBase->TaskReady;
        case EXEC_TASKLIST_WAIT:    return &SysBase->TaskWait;
    }
    return &PrivExecBase(SysBase)->TaskSpinning;
}

static inline void exec_TaskListsLock(int a, int b)
{
    if (a > b)
    {
        int t = a; a = b; b = t;
    }
    if (a != EXEC_TASKLIST_NONE)
        EXEC_SPINLOCK_LOCK(exec_TaskListLock(a), NULL, SPINLOCK_MODE_WRITE);
    if ((b != EXEC_TASKLIST_NONE) && (b != a))
        EXEC_SPINLOCK_LOCK(exec_TaskListLock(b), NULL, SPINLOCK_MODE_WRITE);
}

static inline void exec_TaskListsUnlock(int a, int b)
{
    if ((b != EXEC_TASKLIST_NONE) && (b != a))
        EXEC_SPINLOCK_UNLOCK(exec_TaskListLock(b));
    if (a != EXEC_TASKLIST_NONE)
        EXEC_SPINLOCK_UNLOCK(exec_TaskListLock(a));
}

/*
 * Move a task from list 'from' to list 'to' and set newState, provided
 * tc_State is still 'expect'. FALSE if it was not.
 */
static inline BOOL exec_TaskRelist(struct Task *task, int from, int to,
                                   ULONG expect, ULONG newState)
{
    exec_TaskListsLock(from, to);
    if (task->tc_State != expect)
    {
        exec_TaskListsUnlock(from, to);
        return FALSE;
    }
    if (from != EXEC_TASKLIST_NONE)
        Remove(&task->tc_Node);
    task->tc_State = newState;
    if (to == EXEC_TASKLIST_RUNNING)
        AddHead(exec_TaskList(to), &task->tc_Node);
    else if (to != EXEC_TASKLIST_NONE)
        Enqueue(exec_TaskList(to), &task->tc_Node);
    exec_TaskListsUnlock(from, to);
    return TRUE;
}

/* Move a task between the lists of two states */
static inline BOOL exec_TaskMove(struct Task *task, ULONG from, ULONG to)
{
    return exec_TaskRelist(task, exec_TaskListOf(from), exec_TaskListOf(to),
                           from, to);
}

/* Change a task's state and lists, from whatever it is now */
static inline void exec_TaskSetState(struct Task *task, ULONG newState)
{
    while (!exec_TaskMove(task, task->tc_State, newState))
        ;
}

#if defined(_ETASK_H)
/*
 * Make spinners whose lock has no writer ready again. They retry the
 * lock when run, and the fail hook parks them again if it is still busy.
 */
static inline void exec_TaskUnspin(void)
{
    struct Task *task, *next;

    if (IsListEmpty(&PrivExecBase(SysBase)->TaskSpinning))
        return;

    exec_TaskListsLock(EXEC_TASKLIST_READY, EXEC_TASKLIST_SPINNING);
    ForeachNodeSafe(&PrivExecBase(SysBase)->TaskSpinning, task, next)
    {
        struct IntETask *iet = GetIntETask(task);
        spinlock_t *lock = iet ? iet->iet_SpinLock : NULL;

        if (!lock || !(lock->lock & SPINLOCKF_WRITE))
        {
            if (iet)
                iet->iet_SpinLock = NULL;
            Remove(&task->tc_Node);
            task->tc_State = TS_READY;
            Enqueue(&SysBase->TaskReady, &task->tc_Node);
        }
    }
    exec_TaskListsUnlock(EXEC_TASKLIST_READY, EXEC_TASKLIST_SPINNING);
}
#endif

#endif /* __AROSEXEC_SMP__ */

#endif /* __EXEC_INTERN_H__ */
