#ifndef VPTHREAD_HAL_SCE_SIM_H
#define VPTHREAD_HAL_SCE_SIM_H
#include <errno.h>
#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include "sce_sim.h"

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
    if (r == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
        return ETIMEDOUT;
    if (r == SCE_KERNEL_ERROR_SEMA_ZERO || r == SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN)
        return EBUSY;
    if (r == SCE_KERNEL_ERROR_NO_MEMORY)
        return EAGAIN;
    if (r == SCE_KERNEL_ERROR_UNKNOWN_UID)
        return ESRCH;
    if (r == SCE_KERNEL_ERROR_INVALID_ARGUMENT)
        return EINVAL;
    return EINVAL;
}
static inline void vp_hal_yield(void) { sched_yield(); }

static inline int vp_hal_thread_create(vp_hal_thread_t *out, vp_hal_thread_entry_t entry,
                                       void *arg, size_t arg_size,
                                       size_t stack_size, int priority)
{
    SceUID uid = sceKernelCreateThread("vpthread", entry,
                                       priority ? priority : 0x10000100, (SceSize)(stack_size ? stack_size : 0x20000u),
                                       0, 0, NULL);
    if (uid < 0)
        return vp_hal_map_error(uid);
    int r = sceKernelStartThread(uid, (SceSize)arg_size, arg);
    if (r < 0)
    {
        (void)sceKernelDeleteThread(uid);
        return vp_hal_map_error(r);
    }
    *out = uid;
    return 0;
}

static inline int vp_hal_thread_join_timed(vp_hal_thread_t t, uint32_t usec)
{
    int status = 0;
    SceUInt timeout = usec;
    return vp_hal_map_error(sceKernelWaitThreadEnd(t, &status, &timeout));
}
static inline int vp_hal_thread_join(vp_hal_thread_t t)
{
    int status = 0;
    return vp_hal_map_error(sceKernelWaitThreadEnd(t, &status, NULL));
}
static inline int vp_hal_thread_delete(vp_hal_thread_t t) { return vp_hal_map_error(sceKernelDeleteThread(t)); }
static inline vp_hal_thread_t vp_hal_thread_self(void) { return sceKernelGetThreadId(); }
static inline int vp_hal_thread_setpriority(vp_hal_thread_t t, int p) { return vp_hal_map_error(sceKernelChangeThreadPriority(t, p)); }
static inline int vp_hal_thread_kill(vp_hal_thread_t t, int sig) { return vp_hal_map_error(sceSimThreadKill(t, sig)); }
static inline int vp_hal_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) { return vp_hal_map_error(sceSimAtfork(prepare, parent, child)); }
/* No newlib per-thread reent on the host backend; see hal_vita.h. */
static inline void vp_hal_thread_reent_cleanup_current(void) {}
static inline void vp_hal_thread_exit_current(void)
{
    (void)sceKernelExitThread(0);
    __builtin_unreachable();
}
static inline void vp_hal_thread_exit_delete_current(void)
{
    (void)sceKernelExitDeleteThread(0);
    __builtin_unreachable();
}
static inline int vp_hal_thread_inject_noreturn(vp_hal_thread_t t, vp_hal_inject_entry_t entry,
                                                void *arg, const vp_hal_async_guard_t *guard,
                                                _Atomic int *inflight)
{
    return vp_hal_map_error(sceSimInjectThreadNoreturn(t, entry, arg, guard, inflight));
}
static inline int vp_hal_sleep_us(uint32_t us) { return vp_hal_map_error(sceKernelDelayThread(us)); }

static inline int vp_hal_mutex_init(vp_hal_mutex_t *m)
{
    SceUID x = sceKernelCreateMutex("vp-m", 0, 0, NULL);
    if (x < 0)
        return vp_hal_map_error(x);
    *m = x;
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
    SceUID x = sceKernelCreateSema("vp-p", 0, 0, 1, NULL);
    if (x < 0)
        return vp_hal_map_error(x);
    *p = x;
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
    return r == SCE_KERNEL_ERROR_INVALID_ARGUMENT ? 0 : vp_hal_map_error(r);
}
static inline size_t vp_hal_debug_live_objects(void) { return sceSimLiveObjectCount(); }
#endif
