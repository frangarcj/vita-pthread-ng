#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "internal.h"

static void cond_lock(vp_cond_t *c)
{
    while (atomic_flag_test_and_set_explicit(&c->guard, memory_order_acquire))
        vp_hal_yield();
}

static void cond_unlock(vp_cond_t *c)
{
    atomic_flag_clear_explicit(&c->guard, memory_order_release);
}

static void enqueue(vp_cond_t *c, struct vp_cond_waiter *w)
{
    w->prev = c->tail;
    w->next = NULL;
    w->queued = 1;
    if (c->tail)
        c->tail->next = w;
    else
        c->head = w;
    c->tail = w;
    c->waiters++;
}

static void remove_waiter(vp_cond_t *c, struct vp_cond_waiter *w)
{
    if (!w->queued)
        return;
    if (w->prev)
        w->prev->next = w->next;
    else
        c->head = w->next;
    if (w->next)
        w->next->prev = w->prev;
    else
        c->tail = w->prev;
    w->queued = 0;
    c->waiters--;
}

int vp_cond_init(vp_cond_t **out)
{
    if (!out)
        return EINVAL;
    vp_cond_t *c = calloc(1, sizeof(*c));
    if (!c)
        return ENOMEM;
    c->magic = VP_COND_MAGIC;
    atomic_flag_clear(&c->guard);
    *out = c;
    return 0;
}

int vp_cond_destroy(vp_cond_t *c)
{
    if (!c || c->magic != VP_COND_MAGIC)
        return EINVAL;
    cond_lock(c);
    if (c->waiters)
    {
        cond_unlock(c);
        return EBUSY;
    }
    c->magic = 0;
    cond_unlock(c);
    free(c);
    return 0;
}

static int cond_wait_impl(vp_cond_t *c, vp_mutex_t *m,
                          const struct timespec *abs, int cancelable)
{
    if (!c || c->magic != VP_COND_MAGIC ||
        !m || m->magic != VP_MUTEX_MAGIC)
        return EINVAL;

    vp_thread_t t = vp_thread_self();
    if (!t)
        return ENOMEM;

    /* pthread_cond_wait is a cancellation point.  If cancellation was
     * already pending when we entered, act while the caller still owns the
     * mutex; this preserves the POSIX cleanup invariant that cancellation
     * from a condition wait observes the mutex locked. */
    if (cancelable)
        vp_testcancel();

    struct vp_cond_waiter w = {0};
    w.thread = t;

    vp_async_guard_enter(t);
    cond_lock(c);
    enqueue(c, &w);
    atomic_store_explicit(&t->wait_obj, (uintptr_t)c, memory_order_relaxed);
    atomic_store_explicit(&t->wait_kind, VP_WAIT_COND, memory_order_release);
    cond_unlock(c);

    int r = vp_mutex_unlock(m);
    vp_async_guard_leave_no_cancel(t);
    if (r)
    {
        cond_lock(c);
        remove_waiter(c, &w);
        cond_unlock(c);
        vp_wait_leave(t);
        if (cancelable)
            vp_testcancel();
        return r;
    }

    if (cancelable &&
        atomic_load_explicit(&t->cancel_pending, memory_order_acquire) &&
        atomic_load_explicit(&t->cancel_state, memory_order_acquire) == VP_CANCEL_ENABLE)
    {
        (void)vp_hal_park_trywait(&t->park);
        vp_async_guard_enter(t);
        cond_lock(c);
        remove_waiter(c, &w);
        cond_unlock(c);
        atomic_store_explicit(&t->wait_kind, VP_WAIT_NONE, memory_order_release);
        atomic_store_explicit(&t->wait_obj, 0, memory_order_relaxed);
        vp_async_guard_leave_no_cancel(t);
        r = vp_mutex_lock(m);
        if (r)
            return r;
        vp_testcancel();
        return 0;
    }

    if (abs)
    {
        int expired = 0;
        uint32_t us = vp_abs_to_us(abs, &expired);
        r = expired ? ETIMEDOUT : vp_hal_park_timedwait_us(&t->park, us);
    }
    else
    {
        r = vp_hal_park_wait(&t->park);
    }

    vp_async_guard_enter(t);
    cond_lock(c);
    remove_waiter(c, &w);
    cond_unlock(c);
    atomic_store_explicit(&t->wait_kind, VP_WAIT_NONE, memory_order_release);
    atomic_store_explicit(&t->wait_obj, 0, memory_order_relaxed);
    vp_async_guard_leave_no_cancel(t);

    int lr = vp_mutex_lock(m);
    if (lr)
        return lr;

    if (cancelable)
        vp_testcancel();
    return r == ETIMEDOUT ? ETIMEDOUT : 0;
}

int vp_cond_wait(vp_cond_t *c, vp_mutex_t *m)
{
    return cond_wait_impl(c, m, NULL, 1);
}

int vp_cond_timedwait(vp_cond_t *c, vp_mutex_t *m, const struct timespec *abs)
{
    if (!abs)
        return EINVAL;
    return cond_wait_impl(c, m, abs, 1);
}

int vp_cond_wait_nocancel(vp_cond_t *c, vp_mutex_t *m)
{
    return cond_wait_impl(c, m, NULL, 0);
}

int vp_cond_timedwait_nocancel(vp_cond_t *c, vp_mutex_t *m,
                               const struct timespec *abs)
{
    return cond_wait_impl(c, m, abs, 0);
}

int vp_cond_signal(vp_cond_t *c)
{
    if (!c || c->magic != VP_COND_MAGIC)
        return EINVAL;
    vp_thread_t t = NULL;
    cond_lock(c);
    if (c->head)
    {
        struct vp_cond_waiter *w = c->head;
        t = w->thread;
        remove_waiter(c, w);
    }
    cond_unlock(c);
    return t ? vp_hal_park_wake(&t->park) : 0;
}

int vp_cond_broadcast(vp_cond_t *c)
{
    if (!c || c->magic != VP_COND_MAGIC)
        return EINVAL;
    int first = 0;
    for (;;)
    {
        vp_thread_t t = NULL;
        cond_lock(c);
        if (c->head)
        {
            struct vp_cond_waiter *w = c->head;
            t = w->thread;
            remove_waiter(c, w);
        }
        cond_unlock(c);
        if (!t)
            break;
        int r = vp_hal_park_wake(&t->park);
        if (r && !first)
            first = r;
    }
    return first;
}
