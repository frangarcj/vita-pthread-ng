#ifndef VPTHREAD_HAL_VITA_H
#define VPTHREAD_HAL_VITA_H
#include <stdint.h>
#include <vitasdk/utils.h>
#include "hal_vita_common.h"

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
#endif
