#ifndef VPTHREAD_HAL_VITA_SCELIBC_H
#define VPTHREAD_HAL_VITA_SCELIBC_H
#include "hal_vita_common.h"

/* SceLibc has no per-thread reent to piggyback on, but the kernel allocates a
 * real TLS block per thread for the process image, so __thread works exactly
 * as advertised: one vp_thread_t slot per thread, no lookup needed. */
static _Thread_local void *vp_hal_tcb_current;
static inline void *vp_hal_current_tcb_get(void) { return vp_hal_tcb_current; }
static inline void vp_hal_current_tcb_set(void *tcb) { vp_hal_tcb_current = tcb; }
/* No reent to release: nothing to do on thread exit. */
static inline void vp_hal_thread_reent_cleanup_current(void) {}
static inline void vp_hal_thread_exit_current(void)
{
    sceKernelExitThread(0);
    __builtin_unreachable();
}
static inline void vp_hal_thread_exit_delete_current(void)
{
    sceKernelExitDeleteThread(0);
    __builtin_unreachable();
}
#endif
