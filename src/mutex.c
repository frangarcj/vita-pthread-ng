#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "internal.h"

static void robust_link(vp_thread_t t, vp_mutex_t *m)
{
    if (!m->robust || !t)
        return;
    vp_async_guard_enter(t);
    m->owned_next = t->owned_robust;
    t->owned_robust = m;
    vp_async_guard_leave(t);
}

static void robust_unlink(vp_thread_t t, vp_mutex_t *m)
{
    if (!m->robust || !t)
        return;
    vp_async_guard_enter(t);
    vp_mutex_t **pp = &t->owned_robust;
    while (*pp)
    {
        if (*pp == m)
        {
            *pp = m->owned_next;
            m->owned_next = NULL;
            break;
        }
        pp = &(*pp)->owned_next;
    }
    vp_async_guard_leave(t);
}

int vp_mutex_init_ex(vp_mutex_t **out, int type, int robust,
                     int protocol, int pshared)
{
    if (!out)
        return EINVAL;
    if (type != VP_MUTEX_NORMAL && type != VP_MUTEX_RECURSIVE &&
        type != VP_MUTEX_ERRORCHECK)
        return EINVAL;
    if (robust != VP_MUTEX_STALLED && robust != VP_MUTEX_ROBUST)
        return EINVAL;
    if (protocol != VP_PRIO_NONE && protocol != VP_PRIO_INHERIT &&
        protocol != VP_PRIO_PROTECT)
        return EINVAL;
    if (pshared != 0 && pshared != 1)
        return EINVAL;

    vp_mutex_t *m = calloc(1, sizeof(*m));
    if (!m)
        return ENOMEM;
    m->magic = VP_MUTEX_MAGIC;
    m->type = type;
    m->robust = robust;
    m->protocol = protocol;
    m->pshared = pshared;
    atomic_init(&m->owner, 0);
    atomic_init(&m->robust_state, VP_ROBUST_CONSISTENT);

    int r = vp_hal_mutex_init(&m->os_mutex);
    if (r)
    {
        free(m);
        return r;
    }
    *out = m;
    return 0;
}

int vp_mutex_init(vp_mutex_t **out, int type)
{
    return vp_mutex_init_ex(out, type, VP_MUTEX_STALLED, VP_PRIO_NONE, 0);
}

int vp_mutex_destroy(vp_mutex_t *m)
{
    if (!m || m->magic != VP_MUTEX_MAGIC)
        return EINVAL;
    uintptr_t owner = atomic_load_explicit(&m->owner, memory_order_acquire);
    if (owner)
    {
        vp_thread_t self = vp_thread_self();
        if (!m->robust || owner != (uintptr_t)self)
            return EBUSY;
        robust_unlink(self, m);
        m->recursion = 0;
        atomic_store_explicit(&m->owner, 0, memory_order_release);
        int unlock_r = vp_hal_mutex_unlock(&m->os_mutex);
        if (unlock_r)
        {
            m->recursion = 1;
            atomic_store_explicit(&m->owner, owner, memory_order_release);
            robust_link(self, m);
            return unlock_r;
        }
    }
    int r = vp_hal_mutex_destroy(&m->os_mutex);
    if (r)
        return r;
    m->magic = 0;
    free(m);
    return 0;
}

static int mutex_after_acquire(vp_mutex_t *m, vp_thread_t self)
{
    atomic_store_explicit(&m->owner, (uintptr_t)self, memory_order_release);
    m->recursion = 1;
    robust_link(self, m);

    if (!m->robust)
        return 0;
    int state = atomic_load_explicit(&m->robust_state, memory_order_acquire);
    if (state == VP_ROBUST_OWNER_DEAD)
        return EOWNERDEAD;
    if (state == VP_ROBUST_NOT_RECOVERABLE)
    {
        robust_unlink(self, m);
        m->recursion = 0;
        atomic_store_explicit(&m->owner, 0, memory_order_release);
        (void)vp_hal_mutex_unlock(&m->os_mutex);
        return ENOTRECOVERABLE;
    }
    return 0;
}

static int mutex_precheck(vp_mutex_t *m, vp_thread_t self, int try_only)
{
    uintptr_t me = (uintptr_t)self;
    uintptr_t owner = atomic_load_explicit(&m->owner, memory_order_acquire);
    if (owner != me)
        return -1;
    if (m->type == VP_MUTEX_RECURSIVE)
    {
        m->recursion++;
        return 0;
    }
    if (m->type == VP_MUTEX_ERRORCHECK)
        return try_only ? EBUSY : EDEADLK;
    return try_only ? EBUSY : -1;
}

int vp_mutex_lock(vp_mutex_t *m)
{
    if (!m || m->magic != VP_MUTEX_MAGIC)
        return EINVAL;
    vp_thread_t self = vp_thread_self();
    int pre = mutex_precheck(m, self, 0);
    if (pre >= 0)
        return pre;
    /* Uncontended: nothing to block on, so nothing to stay cancellable for. */
    if (!vp_hal_mutex_trylock(&m->os_mutex))
        return mutex_after_acquire(m, self);
    for (;;)
    {
        vp_wait_enter(self, VP_WAIT_MUTEX, m);
        int r = vp_hal_mutex_timedlock_us(&m->os_mutex, 2000);
        vp_wait_leave(self);
        vp_testcancel_async_only();
        if (r == ETIMEDOUT)
            continue;
        if (r)
            return r;
        return mutex_after_acquire(m, self);
    }
}

int vp_mutex_trylock(vp_mutex_t *m)
{
    if (!m || m->magic != VP_MUTEX_MAGIC)
        return EINVAL;
    vp_thread_t self = vp_thread_self();
    int pre = mutex_precheck(m, self, 1);
    if (pre >= 0)
        return pre;
    int r = vp_hal_mutex_trylock(&m->os_mutex);
    if (r)
        return r;
    return mutex_after_acquire(m, self);
}

int vp_mutex_timedlock(vp_mutex_t *m, const struct timespec *abs)
{
    if (!m || m->magic != VP_MUTEX_MAGIC || !abs)
        return EINVAL;
    vp_thread_t self = vp_thread_self();
    int pre = mutex_precheck(m, self, 0);
    if (pre >= 0)
        return pre;
    for (;;)
    {
        int expired = 0;
        uint32_t us = vp_abs_to_us(abs, &expired);
        if (expired)
            return ETIMEDOUT;
        uint32_t slice = us > 2000u ? 2000u : us;
        vp_wait_enter(self, VP_WAIT_MUTEX, m);
        int r = vp_hal_mutex_timedlock_us(&m->os_mutex, slice);
        vp_wait_leave(self);
        vp_testcancel_async_only();
        if (r == ETIMEDOUT)
        {
            if (slice < us)
                continue;
            return ETIMEDOUT;
        }
        if (r)
            return r;
        return mutex_after_acquire(m, self);
    }
}

int vp_mutex_consistent(vp_mutex_t *m)
{
    if (!m || m->magic != VP_MUTEX_MAGIC || !m->robust)
        return EINVAL;
    if (atomic_load_explicit(&m->owner, memory_order_acquire) !=
        (uintptr_t)vp_thread_self())
        return EPERM;
    int expected = VP_ROBUST_OWNER_DEAD;
    if (!atomic_compare_exchange_strong_explicit(&m->robust_state, &expected,
                                                 VP_ROBUST_CONSISTENT, memory_order_acq_rel, memory_order_acquire))
        return EINVAL;
    return 0;
}

int vp_mutex_unlock(vp_mutex_t *m)
{
    if (!m || m->magic != VP_MUTEX_MAGIC)
        return EINVAL;
    vp_thread_t self = vp_thread_self();
    uintptr_t me = (uintptr_t)self;
    if (atomic_load_explicit(&m->owner, memory_order_acquire) != me)
        return EPERM;

    if (m->type == VP_MUTEX_RECURSIVE && m->recursion > 1)
    {
        m->recursion--;
        return 0;
    }

    if (m->robust && atomic_load_explicit(&m->robust_state, memory_order_acquire) == VP_ROBUST_OWNER_DEAD)
        atomic_store_explicit(&m->robust_state, VP_ROBUST_NOT_RECOVERABLE,
                              memory_order_release);

    robust_unlink(self, m);
    m->recursion = 0;
    atomic_store_explicit(&m->owner, 0, memory_order_release);
    return vp_hal_mutex_unlock(&m->os_mutex);
}

void vp_abandon_owned_mutexes(vp_thread_t t)
{
    if (!t)
        return;
    while (t->owned_robust)
    {
        vp_mutex_t *m = t->owned_robust;
        t->owned_robust = m->owned_next;
        m->owned_next = NULL;
        m->recursion = 0;
        atomic_store_explicit(&m->robust_state, VP_ROBUST_OWNER_DEAD,
                              memory_order_release);
        atomic_store_explicit(&m->owner, 0, memory_order_release);
        (void)vp_hal_mutex_unlock(&m->os_mutex);
    }
}

size_t vp_debug_mutex_size(void) { return sizeof(struct vp_mutex); }
