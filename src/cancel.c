#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include "internal.h"

#define VP_CLEANUP_HEAP (-1)

static void vp_async_cancel_trampoline(void *opaque) __attribute__((noreturn));
static void vp_async_cancel_trampoline(void *opaque)
{
    vp_thread_t t = opaque;
    if (!t || vp_thread_self() != t)
        vp_hal_thread_exit_current();
    atomic_store_explicit(&t->async_inflight, 0, memory_order_release);
    vp_finish_current(VP_CANCELED, 1);
    int detached = atomic_load_explicit(&t->detached, memory_order_acquire);
    if (detached)
        vp_thread_unregister(t);
    /* Do not call code that can return to the overwritten context. */
    vp_thread_release_running(t); /* running reference */
    if (detached)
        vp_hal_thread_exit_delete_current();
    vp_hal_thread_exit_current();
    __builtin_unreachable();
}

int vp_setcancelstate(int state, int *old)
{
    if (state != VP_CANCEL_ENABLE && state != VP_CANCEL_DISABLE)
        return EINVAL;
    vp_thread_t t = vp_thread_self();
    if (!t)
        return ENOMEM;
    vp_async_guard_enter(t);
    if (old)
        *old = atomic_load_explicit(&t->cancel_state, memory_order_acquire);
    atomic_store_explicit(&t->cancel_state, state, memory_order_release);
    vp_async_guard_leave(t);
    /* Enabling deferred cancellation is not itself a cancellation point.
     * A pending request is delivered here only for ASYNCHRONOUS mode. */
    if (state == VP_CANCEL_ENABLE)
        vp_testcancel_async_only();
    return 0;
}

int vp_setcanceltype(int type, int *old)
{
    if (type != VP_CANCEL_DEFERRED && type != VP_CANCEL_ASYNCHRONOUS)
        return EINVAL;
    vp_thread_t t = vp_thread_self();
    if (!t)
        return ENOMEM;
    vp_async_guard_enter(t);
    if (old)
        *old = atomic_load_explicit(&t->cancel_type, memory_order_acquire);
    atomic_store_explicit(&t->cancel_type, type, memory_order_release);
    vp_async_guard_leave(t);
    if (type == VP_CANCEL_ASYNCHRONOUS)
        vp_testcancel();
    return 0;
}

int vp_cancel(vp_thread_t t)
{
    if (!vp_valid_thread(t))
        return ESRCH;
    atomic_store_explicit(&t->cancel_pending, 1, memory_order_release);

    int wait_kind = atomic_load_explicit(&t->wait_kind, memory_order_acquire);
    uintptr_t wait_obj = atomic_load_explicit(&t->wait_obj, memory_order_acquire);
    if (wait_kind == VP_WAIT_COND && wait_obj)
        (void)vp_cond_broadcast((vp_cond_t *)wait_obj);

    if (atomic_load_explicit(&t->cancel_state, memory_order_acquire) != VP_CANCEL_ENABLE ||
        atomic_load_explicit(&t->cancel_type, memory_order_acquire) != VP_CANCEL_ASYNCHRONOUS)
        return 0;
    if (t == vp_thread_self())
    {
        vp_testcancel_async_only();
        return 0;
    }

    /* Never rewrite a context while the target is inside a HAL wait.
     * Cond waits have an explicit wake; other HAL waits are bounded/polled and
     * deliver async cancellation immediately after returning to common code. */
    if (wait_kind != VP_WAIT_NONE)
        return 0;
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    if (t->async_guard != 0)
#else
    if (atomic_load_explicit(&t->async_guard, memory_order_acquire) != 0)
#endif
        return 0;
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&t->async_inflight, &expected, 1, memory_order_acq_rel, memory_order_acquire))
        return 0;
    int r = vp_hal_thread_inject_noreturn(t->os_thread, vp_async_cancel_trampoline, t, &t->async_guard, &t->async_inflight);
    if (r)
        atomic_store_explicit(&t->async_inflight, 0, memory_order_release);
    return r;
}

void vp_testcancel_async_only(void)
{
    vp_thread_t t = vp_thread_self();
    if (!t)
        return;
    if (atomic_load_explicit(&t->cancel_type, memory_order_acquire) != VP_CANCEL_ASYNCHRONOUS)
        return;
    vp_testcancel();
}

void vp_testcancel(void)
{
    vp_thread_t t = vp_thread_self();
    if (!t)
        return;
    if (atomic_load_explicit(&t->cancel_state, memory_order_acquire) != VP_CANCEL_ENABLE ||
        !atomic_load_explicit(&t->cancel_pending, memory_order_acquire))
        return;
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    if (t->async_guard != 0)
#else
    if (atomic_load_explicit(&t->async_guard, memory_order_acquire) != 0)
#endif
        return;
    vp_finish_current(VP_CANCELED, 1);
    int detached = atomic_load_explicit(&t->detached, memory_order_acquire);
    if (detached)
        vp_thread_unregister(t);
    vp_thread_release_running(t);
    if (detached)
        vp_hal_thread_exit_delete_current();
    vp_hal_thread_exit_current();
}

void vp_cleanup_link_raw(void *ctx)
{
    if (!ctx)
        return;
    vp_thread_t t = vp_thread_self();
    if (!t)
        return;
    vp_cleanup_t c;
    memcpy(&c, ctx, sizeof(c));
    c.prev = t->cleanup;
    memcpy(ctx, &c, sizeof(c));
    t->cleanup = ctx;
}

void vp_cleanup_pop_raw(void *ctx, int execute)
{
    if (!ctx)
        return;
    vp_cleanup_t c;
    memcpy(&c, ctx, sizeof(c));
    vp_thread_t t = vp_thread_self();
    if (c.canceltype == VP_CLEANUP_HEAP && c.prev)
    {
        vp_cleanup_t *node = c.prev;
        if (t && t->cleanup == node)
            t->cleanup = node->prev;
        if (execute && node->routine)
            node->routine(node->arg);
        free(node);
        return;
    }
    if (t && t->cleanup == ctx)
        t->cleanup = c.prev;
    if (execute && c.routine)
        c.routine(c.arg);
}
void vp_cleanup_push_impl(vp_cleanup_t *c, void (*routine)(void *), void *arg)
{
    if (!c)
        return;
    vp_thread_t t = vp_thread_self();
    vp_cleanup_t *node = t ? malloc(sizeof(*node)) : NULL;
    if (!node)
    {
        *c = (vp_cleanup_t){routine, arg, VP_CANCEL_DEFERRED, NULL};
        vp_cleanup_link_raw(c);
        return;
    }
    *node = (vp_cleanup_t){routine, arg, VP_CLEANUP_HEAP, t->cleanup};
    *c = (vp_cleanup_t){routine, arg, VP_CLEANUP_HEAP, node};
    t->cleanup = node;
}
void vp_cleanup_pop_impl(vp_cleanup_t *c, int execute) { vp_cleanup_pop_raw(c, execute); }

void vp_run_cleanup(vp_thread_t t)
{
    while (t && t->cleanup)
    {
        void *ctx = t->cleanup;
        vp_cleanup_t c;
        memcpy(&c, ctx, sizeof(c));
        t->cleanup = c.prev;
        if (c.routine)
            c.routine(c.arg);
        if (c.canceltype == VP_CLEANUP_HEAP)
            free(ctx);
    }
}

int vp_sleep_ms(unsigned ms)
{
    vp_thread_t t = vp_thread_self();
    for (unsigned i = 0; i < ms; i++)
    {
        vp_testcancel();
        vp_wait_enter(t, VP_WAIT_SLEEP, NULL);
        int r = vp_hal_sleep_us(1000);
        vp_wait_leave(t);
        vp_testcancel();
        if (r)
            return r;
    }
    vp_testcancel();
    return 0;
}
