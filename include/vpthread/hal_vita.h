#ifndef VPTHREAD_HAL_VITA_H
#define VPTHREAD_HAL_VITA_H
#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include <vitasdk/utils.h>

typedef SceUID vp_hal_thread_t;
typedef SceUID vp_hal_mutex_t;
typedef SceUID vp_hal_park_t;
typedef SceKernelThreadEntry vp_hal_thread_entry_t;
typedef void (*vp_hal_inject_entry_t)(void *);
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
typedef unsigned vp_hal_async_guard_t;
#else
typedef _Atomic unsigned vp_hal_async_guard_t;
#endif

static inline int vp_hal_map_error(int r)
{
    if (r >= 0)
        return 0;
    if (r == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT)
        return ETIMEDOUT;
    if (r == (int)SCE_KERNEL_ERROR_SEMA_ZERO || r == (int)SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN)
        return EBUSY;
    if (r == (int)SCE_KERNEL_ERROR_NO_MEMORY)
        return EAGAIN;
    if (r == (int)SCE_KERNEL_ERROR_UNKNOWN_UID || r == (int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
        return ESRCH;
    if (r == (int)SCE_KERNEL_ERROR_INVALID_ARGUMENT)
        return EINVAL;
    return EINVAL;
}
static inline void vp_hal_yield(void) { (void)sceKernelDelayThread(0); }
static inline int vp_hal_thread_create(vp_hal_thread_t *out, vp_hal_thread_entry_t e, void *arg, size_t arg_size, size_t stack, int pri)
{
    SceUID u = sceKernelCreateThread("vpthread", e, pri ? pri : 0x10000100, (SceSize)(stack ? stack : 0x20000u), 0, 0, NULL);
    if (u < 0)
        return vp_hal_map_error(u);
    int r = sceKernelStartThread(u, (SceSize)arg_size, arg);
    if (r < 0)
    {
        sceKernelDeleteThread(u);
        return vp_hal_map_error(r);
    }
    *out = u;
    return 0;
}
static inline int vp_hal_thread_join_timed(vp_hal_thread_t t, uint32_t us)
{
    int s = 0;
    SceUInt to = us;
    return vp_hal_map_error(sceKernelWaitThreadEnd(t, &s, &to));
}
static inline int vp_hal_thread_join(vp_hal_thread_t t)
{
    int s = 0;
    return vp_hal_map_error(sceKernelWaitThreadEnd(t, &s, NULL));
}
static inline int vp_hal_thread_delete(vp_hal_thread_t t) { return vp_hal_map_error(sceKernelDeleteThread(t)); }
static inline vp_hal_thread_t vp_hal_thread_self(void) { return sceKernelGetThreadId(); }
/* Per-thread storage for the current TCB pointer.  _Thread_local cannot be
 * used on this backend: libgcc emutls needs a gthread layer, and vpthread is
 * the thread library, so __gthread_active_p() is false and emutls degrades to
 * one process-global slot.  newlib reserves a per-thread pointer next to each
 * reent for exactly this purpose (vitasdk_get_pthread_data); thid 0 resolves
 * to the calling thread through its kernel TLS slot. */
static inline void **vp_hal_current_tcb_slot(void)
{
    void *slot = vitasdk_get_pthread_data(0);
    /* vitasdk_get_pthread_data returns reent - 4, so a failed reent
     * allocation surfaces as (void *)-4, not NULL. */
    if ((uintptr_t)slot == (uintptr_t)-4)
        return NULL;
    return (void **)slot;
}
static inline void *vp_hal_current_tcb_get(void)
{
    void **slot = vp_hal_current_tcb_slot();
    return slot ? *slot : NULL;
}
static inline void vp_hal_current_tcb_set(void *tcb)
{
    void **slot = vp_hal_current_tcb_slot();
    if (slot)
        *slot = tcb;
}
static inline int vp_hal_thread_setpriority(vp_hal_thread_t t, int p) { return vp_hal_map_error(sceKernelChangeThreadPriority(t, p)); }
static inline int vp_hal_thread_kill(vp_hal_thread_t t, int sig)
{
    (void)t;
    (void)sig;
    return ENOTSUP;
}
static inline int vp_hal_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    (void)prepare;
    (void)parent;
    (void)child;
    return ENOTSUP;
}
/* Newlib allocates one reent (plus the TLS/pthread-data slots in front of it)
 * per thread, from a fixed 256-entry table, and never frees it when a raw
 * kernel thread simply returns.  Every exit path must release it or long
 * create/join workloads exhaust the table and vitasdk_get_pthread_data
 * starts failing.  Must be the last newlib-dependent action on the thread. */
static inline void vp_hal_thread_reent_cleanup_current(void)
{
    (void)vitasdk_delete_thread_reent(sceKernelGetThreadId());
}
static inline void vp_hal_thread_exit_current(void)
{
    SceUID id = sceKernelGetThreadId();
    (void)vitasdk_delete_thread_reent(id);
    sceKernelExitThread(0);
    __builtin_unreachable();
}
static inline void vp_hal_thread_exit_delete_current(void)
{
    SceUID id = sceKernelGetThreadId();
    (void)vitasdk_delete_thread_reent(id);
    sceKernelExitDeleteThread(0);
    __builtin_unreachable();
}

/* Async cancellation injection over the user-side ForVM thread-context API
 * (SceLibKernel exports the context accessors; SceKernelThreadMgr exports
 * suspend/resume).  These need the VM capability on real hardware; without
 * it (or under emulators that stub the API) every failure path degrades to
 * ENOTSUP after resuming the target untouched.
 *
 * vitasdk 2026.08 ships the import stubs but no prototypes or types, so they
 * are declared here (layouts match the firmware ABI as implemented by
 * Vita3K's HLE).  Drop this block if vita-headers ever gains them. */
typedef struct SceKernelThreadCpuRegisterInfo
{
    SceSize size; /* 0x58 */
    SceUInt32 cpsr;
    SceUInt32 reg[16]; /* r0-r12, sp, lr, pc */
    SceUInt32 tpidrurw;
    SceUInt32 teehbr;
    SceUInt32 sb;
    SceUInt32 st;
} SceKernelThreadCpuRegisterInfo;
typedef struct SceKernelThreadVfpRegisterInfo
{
    SceSize size; /* 0x108 */
    SceUInt32 fpscr;
    float reg[64];
} SceKernelThreadVfpRegisterInfo;
int sceKernelSuspendThreadForVM(SceUID thid);
int sceKernelResumeThreadForVM(SceUID thid);
int sceKernelGetThreadContextForVM(SceUID thid, SceKernelThreadCpuRegisterInfo *cpu,
                                   SceKernelThreadVfpRegisterInfo *vfp);
int sceKernelSetThreadContextForVM(SceUID thid, const SceKernelThreadCpuRegisterInfo *cpu,
                                   const SceKernelThreadVfpRegisterInfo *vfp);

#define VP_HAL_CPSR_THUMB 0x20u

/* Operation: suspend target -> re-check *guard while suspended -> get context
 * -> set PC/R0 to entry/arg -> verify -> resume.  If guard != 0, resume
 * without injection and clear *inflight; the target delivers cancellation
 * itself when it leaves the guard.  The verify read-back is load-bearing:
 * Vita3K's SetThreadContextForVM is a silent no-op that returns success. */
static inline int vp_hal_thread_inject_noreturn(vp_hal_thread_t t, vp_hal_inject_entry_t entry,
                                                void *arg, const vp_hal_async_guard_t *guard,
                                                _Atomic int *inflight)
{
    int r = sceKernelSuspendThreadForVM(t);
    if (r < 0)
        return ENOTSUP;

#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    unsigned g = *guard;
#else
    unsigned g = atomic_load_explicit((_Atomic unsigned *)(uintptr_t)guard, memory_order_acquire);
#endif
    if (g != 0)
    {
        atomic_store_explicit(inflight, 0, memory_order_release);
        (void)sceKernelResumeThreadForVM(t);
        return 0;
    }

    SceKernelThreadCpuRegisterInfo cpu;
    SceKernelThreadVfpRegisterInfo vfp;
    cpu.size = sizeof(cpu);
    vfp.size = sizeof(vfp);
    r = sceKernelGetThreadContextForVM(t, &cpu, &vfp);
    if (r < 0)
        goto unsupported_resume;

    uintptr_t e = (uintptr_t)entry;
    cpu.reg[0] = (SceUInt32)(uintptr_t)arg;
    cpu.reg[15] = (SceUInt32)(e & ~(uintptr_t)1);
    if (e & 1u)
        cpu.cpsr |= VP_HAL_CPSR_THUMB;
    else
        cpu.cpsr &= ~VP_HAL_CPSR_THUMB;
    /* sp stays; the entry never returns, so lr is irrelevant. */
    r = sceKernelSetThreadContextForVM(t, &cpu, &vfp);
    if (r < 0)
        goto unsupported_resume;

    {
        SceKernelThreadCpuRegisterInfo chk;
        SceKernelThreadVfpRegisterInfo chk_vfp;
        chk.size = sizeof(chk);
        chk_vfp.size = sizeof(chk_vfp);
        r = sceKernelGetThreadContextForVM(t, &chk, &chk_vfp);
        if (r < 0 || chk.reg[15] != cpu.reg[15] || chk.reg[0] != cpu.reg[0])
            goto unsupported_resume;
    }

    r = sceKernelResumeThreadForVM(t);
    if (r < 0)
    {
        /* Context already rewritten; the target will run the entry if it
         * ever resumes.  Report the failure without undoing the injection. */
        atomic_store_explicit(inflight, 0, memory_order_release);
        return vp_hal_map_error(r);
    }
    return 0;

unsupported_resume:
    atomic_store_explicit(inflight, 0, memory_order_release);
    (void)sceKernelResumeThreadForVM(t);
    return ENOTSUP;
}
static inline int vp_hal_sleep_us(uint32_t us) { return vp_hal_map_error(sceKernelDelayThread((SceUInt)us)); }
static inline int vp_hal_mutex_init(vp_hal_mutex_t *m)
{
    SceUID u = sceKernelCreateMutex("vp-m", 0, 0, NULL);
    if (u < 0)
        return vp_hal_map_error(u);
    *m = u;
    return 0;
}
static inline int vp_hal_mutex_destroy(vp_hal_mutex_t *m)
{
    int r = sceKernelDeleteMutex(*m);
    if (r >= 0)
        *m = 0;
    return vp_hal_map_error(r);
}
static inline int vp_hal_mutex_lock(vp_hal_mutex_t *m) { return vp_hal_map_error(sceKernelLockMutex(*m, 1, NULL)); }
static inline int vp_hal_mutex_trylock(vp_hal_mutex_t *m) { return vp_hal_map_error(sceKernelTryLockMutex(*m, 1)); }
static inline int vp_hal_mutex_timedlock_us(vp_hal_mutex_t *m, uint32_t us)
{
    SceUInt t = us;
    return vp_hal_map_error(sceKernelLockMutex(*m, 1, &t));
}
static inline int vp_hal_mutex_unlock(vp_hal_mutex_t *m) { return vp_hal_map_error(sceKernelUnlockMutex(*m, 1)); }
static inline int vp_hal_park_init(vp_hal_park_t *p)
{
    SceUID u = sceKernelCreateSema("vp-p", 0, 0, 1, NULL);
    if (u < 0)
        return vp_hal_map_error(u);
    *p = u;
    return 0;
}
static inline int vp_hal_park_destroy(vp_hal_park_t *p)
{
    if (!*p)
        return 0;
    int r = sceKernelDeleteSema(*p);
    if (r >= 0)
        *p = 0;
    return vp_hal_map_error(r);
}
static inline int vp_hal_park_wait(vp_hal_park_t *p) { return vp_hal_map_error(sceKernelWaitSema(*p, 1, NULL)); }
static inline int vp_hal_park_timedwait_us(vp_hal_park_t *p, uint32_t us)
{
    SceUInt t = us;
    return vp_hal_map_error(sceKernelWaitSema(*p, 1, &t));
}
static inline int vp_hal_park_trywait(vp_hal_park_t *p) { return vp_hal_map_error(sceKernelPollSema(*p, 1)); }
static inline int vp_hal_park_wake(vp_hal_park_t *p)
{
    int r = sceKernelSignalSema(*p, 1);
    return r < 0 ? vp_hal_map_error(r) : 0;
}
static inline size_t vp_hal_debug_live_objects(void) { return 0; }
#endif
