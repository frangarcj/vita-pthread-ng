#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "internal.h"

struct handle_slot
{
    vp_thread_t t;
    uint32_t gen;
};
static struct handle_slot g_handles[VP_HANDLE_SLOTS];
static atomic_flag g_handle_guard = ATOMIC_FLAG_INIT;
static _Thread_local vp_thread_t g_current;
#define vp_current_get() (g_current)
#define vp_current_set(t) ((void)(g_current = (t)))
#if defined(VPTHREAD_HAL_VITA)
/* g_current is only per-thread if the toolchain emits real TLS. A newlib that
 * keeps its own reent there is the published evidence that it does; without
 * it, emutls would give every thread the same g_current. */
#include <reent.h>
_Static_assert(
#ifdef _REENT_THREAD_LOCAL
    1,
#else
    0,
#endif
    "vpthread needs a native-TLS toolchain: build newlib with "
    "--enable-newlib-reent-thread-local and gcc with --enable-tls");
/* Force extraction of posix.o whenever vpthread is linked: libstdc++ and
 * libgcc bind their gthread layer to the strong pthread_* it defines. */
extern const char vp_posix_gthread_anchor;
__attribute__((used)) static const void *const vp_gthread_anchor_ref =
    &vp_posix_gthread_anchor;
#endif

static void handle_lock(void)
{
    while (atomic_flag_test_and_set_explicit(&g_handle_guard, memory_order_acquire))
        vp_hal_yield();
}
static void handle_unlock(void) { atomic_flag_clear_explicit(&g_handle_guard, memory_order_release); }

void vp_async_guard_enter(vp_thread_t t)
{
    if (!t)
        return;
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    t->async_guard++;
#else
    atomic_fetch_add_explicit(&t->async_guard, 1, memory_order_acq_rel);
#endif
}

void vp_async_guard_leave_no_cancel(vp_thread_t t)
{
    if (!t)
        return;
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    t->async_guard--;
#else
    (void)atomic_fetch_sub_explicit(&t->async_guard, 1, memory_order_acq_rel);
#endif
}

void vp_async_guard_leave(vp_thread_t t)
{
    if (!t)
        return;
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    unsigned prev = t->async_guard--;
#else
    unsigned prev = atomic_fetch_sub_explicit(&t->async_guard, 1, memory_order_acq_rel);
#endif
    if (prev == 1 && atomic_load_explicit(&t->cancel_pending, memory_order_acquire) &&
        atomic_load_explicit(&t->cancel_state, memory_order_acquire) == VP_CANCEL_ENABLE &&
        atomic_load_explicit(&t->cancel_type, memory_order_acquire) == VP_CANCEL_ASYNCHRONOUS)
        vp_testcancel_async_only();
}

void vp_wait_enter(vp_thread_t t, int kind, void *obj)
{
    if (!t)
        return;
    vp_async_guard_enter(t);
    atomic_store_explicit(&t->wait_obj, (uintptr_t)obj, memory_order_relaxed);
    atomic_store_explicit(&t->wait_kind, kind, memory_order_release);
    vp_async_guard_leave_no_cancel(t);
}

void vp_wait_leave(vp_thread_t t)
{
    if (!t)
        return;
    vp_async_guard_enter(t);
    atomic_store_explicit(&t->wait_kind, VP_WAIT_NONE, memory_order_release);
    atomic_store_explicit(&t->wait_obj, 0, memory_order_relaxed);
    vp_async_guard_leave_no_cancel(t);
}

void vp_thread_put(vp_thread_t t)
{
    if (!t)
        return;
    unsigned previous = atomic_fetch_sub_explicit(&t->refs, 1, memory_order_acq_rel);
    if (previous == 1)
    {
        struct vp_tsd_entry *e = t->tsd;
        while (e)
        {
            struct vp_tsd_entry *n = e->next;
            free(e);
            e = n;
        }
        (void)vp_hal_park_destroy(&t->park);
        t->magic = 0;
        free(t);
    }
}

static int register_handle(vp_thread_t t)
{
    vp_thread_t current = vp_current_get();
    if (current)
        vp_async_guard_enter(current);
    handle_lock();
    for (uint32_t i = 1; i < VP_HANDLE_SLOTS; i++)
    {
        if (!g_handles[i].t)
        {
            uint32_t gen = (g_handles[i].gen + 1u) & ((1u << (32u - VP_HANDLE_SLOT_BITS)) - 1u);
            if (!gen)
                gen = 1;
            g_handles[i].gen = gen;
            g_handles[i].t = t;
            t->public_handle = (gen << VP_HANDLE_SLOT_BITS) | i;
            vp_thread_get(t); /* table reference */
            handle_unlock();
            if (current)
                vp_async_guard_leave(current);
            return 0;
        }
    }
    handle_unlock();
    if (current)
        vp_async_guard_leave(current);
    return EAGAIN;
}

void vp_thread_unregister(vp_thread_t t)
{
    if (!t || !t->public_handle)
        return;
    vp_thread_t current = vp_current_get();
    if (current)
        vp_async_guard_enter(current);
    uint32_t h = t->public_handle, idx = h & VP_HANDLE_SLOT_MASK;
    handle_lock();
    if (idx < VP_HANDLE_SLOTS && g_handles[idx].t == t)
    {
        g_handles[idx].t = NULL;
        t->public_handle = 0;
        handle_unlock();
        vp_thread_put(t); /* table reference */
        if (current)
            vp_async_guard_leave(current);
        return;
    }
    handle_unlock();
    if (current)
        vp_async_guard_leave(current);
}

vp_thread_t vp_thread_acquire_handle(vp_pthread_handle_t h)
{
    uint32_t idx = h & VP_HANDLE_SLOT_MASK, gen = h >> VP_HANDLE_SLOT_BITS;
    if (!idx || idx >= VP_HANDLE_SLOTS || !gen)
        return NULL;
    vp_thread_t current = vp_current_get();
    if (current)
        vp_async_guard_enter(current);
    handle_lock();
    vp_thread_t t = g_handles[idx].t;
    if (!t || g_handles[idx].gen != gen || t->magic != VP_THREAD_MAGIC)
        t = NULL;
    else
        vp_thread_get(t);
    handle_unlock();
    if (current)
        vp_async_guard_leave(current);
    return t;
}
void vp_thread_release(vp_thread_t t) { vp_thread_put(t); }
void vp_thread_release_running(vp_thread_t t)
{
    if (!t)
        return;
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&t->running_ref_released, &expected, 1,
                                                memory_order_acq_rel, memory_order_acquire))
    {
        if (vp_current_get() == t)
            vp_current_set(NULL);
        vp_thread_put(t);
    }
}
vp_pthread_handle_t vp_thread_handle(vp_thread_t t) { return vp_valid_thread(t) ? t->public_handle : 0; }

static void init_tcb(vp_thread_t t)
{
    t->magic = VP_THREAD_MAGIC;
    atomic_init(&t->refs, 2); /* running/external + creator temporary */
    atomic_init(&t->running_ref_released, 0);
    atomic_init(&t->state, VP_THREAD_NEW);
    atomic_init(&t->detached, 0);
    atomic_init(&t->joined, 0);
    atomic_init(&t->cancel_pending, 0);
    atomic_init(&t->cancel_state, VP_CANCEL_ENABLE);
    atomic_init(&t->cancel_type, VP_CANCEL_DEFERRED);
#if defined(VPTHREAD_DIAGNOSTIC_NO_ASYNC_GUARD_ATOMIC)
    t->async_guard = 0;
#else
    atomic_init(&t->async_guard, 0);
#endif
    atomic_init(&t->async_inflight, 0);
    atomic_init(&t->wait_kind, VP_WAIT_NONE);
    atomic_init(&t->wait_obj, 0);
}

#if defined(__GNUC__)
__attribute__((no_sanitize_thread))
#endif
int vp_sim_host_cancel_cleanup(void *opaque)
{
    vp_thread_t t = opaque ? *(vp_thread_t *)opaque : NULL;
    if (!t || vp_current_get() != t)
        return 0;
    atomic_store_explicit(&t->async_inflight, 0, memory_order_release);
    vp_finish_current(VP_CANCELED, 1);
    int detached = atomic_load_explicit(&t->detached, memory_order_acquire);
    if (detached)
        vp_thread_unregister(t);
    vp_current_set(NULL);
    return 1;
}

#if defined(__GNUC__)
__attribute__((no_sanitize_thread))
#endif
void vp_sim_host_cancel_release(void *opaque)
{
    vp_thread_t t = opaque ? *(vp_thread_t *)opaque : NULL;
    if (t)
        vp_thread_release_running(t);
}

static vp_thread_t ensure_external_current(void)
{
    vp_thread_t cur = vp_current_get();
    if (cur)
        return cur;
    vp_thread_t t = calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    init_tcb(t);
    atomic_store_explicit(&t->state, VP_THREAD_RUNNING, memory_order_release);
    t->os_thread = vp_hal_thread_self();
    if (vp_hal_park_init(&t->park) != 0 || register_handle(t) != 0)
    {
        if (t->park)
            (void)vp_hal_park_destroy(&t->park);
        free(t);
        return NULL;
    }
    vp_current_set(t);
    vp_thread_put(t); /* creator temporary; table + external-running remain */
    return t;
}

int vp_thread_attr_init(vp_thread_attr_t *a)
{
    if (!a)
        return EINVAL;
    a->stack_size = 0;
    a->priority = 0;
    a->detached = 0;
    return 0;
}

static int vp_start_trampoline(SceSize argc, void *argv)
{
    if (argc != sizeof(vp_thread_t) || !argv)
        return EINVAL;
    vp_thread_t t = *(vp_thread_t *)argv;
    vp_current_set(t);
    atomic_store_explicit(&t->state, VP_THREAD_RUNNING, memory_order_release);
    void *ret = t->start(t->arg);
    vp_finish_current(ret, 0);
    vp_current_set(NULL);
    if (atomic_load_explicit(&t->detached, memory_order_acquire))
    {
        vp_thread_unregister(t);
        vp_thread_release_running(t);
        vp_hal_thread_exit_delete_current();
    }
    vp_thread_release_running(t); /* running ref; table keeps joinable TCB alive */
    vp_hal_thread_reent_cleanup_current();
    return 0;
}

int vp_thread_create(vp_thread_t *out, const vp_thread_attr_t *attr, void *(*fn)(void *), void *arg)
{
    if (!out || !fn)
        return EINVAL;
    vp_thread_attr_t local;
    if (!attr)
    {
        vp_thread_attr_init(&local);
        attr = &local;
    }
    vp_thread_t t = calloc(1, sizeof(*t));
    if (!t)
        return EAGAIN;
    init_tcb(t);
    atomic_store_explicit(&t->detached, !!attr->detached, memory_order_relaxed);
    t->start = fn;
    t->arg = arg;
    int r = vp_hal_park_init(&t->park);
    if (r)
    {
        free(t);
        return r;
    }
    r = register_handle(t);
    if (r)
    {
        vp_hal_park_destroy(&t->park);
        free(t);
        return r;
    }
    vp_thread_t arg_t = t;
    r = vp_hal_thread_create(&t->os_thread, vp_start_trampoline, &arg_t, sizeof(arg_t), attr->stack_size, attr->priority);
    if (r)
    {
        vp_thread_unregister(t);
        vp_thread_release_running(t); /* running reference never started */
        vp_thread_put(t);             /* creator temporary */
        return r;
    }
    *out = t;
    vp_thread_put(t); /* creator temporary */
    return 0;
}

int vp_thread_create_handle(vp_pthread_handle_t *out, const vp_thread_attr_t *a,
                            void *(*fn)(void *), void *arg)
{
    if (!out)
        return EINVAL;
    vp_thread_t t;
    int r = vp_thread_create(&t, a, fn, arg);
    if (r)
        return r;
    *out = t->public_handle;
    return 0;
}

vp_thread_t vp_thread_self(void) { return ensure_external_current(); }
int vp_thread_equal(vp_thread_t a, vp_thread_t b) { return a == b; }

static void join_claim_cleanup(void *arg)
{
    vp_thread_t t = arg;
    if (t)
        atomic_store_explicit(&t->joined, 0, memory_order_release);
}

int vp_thread_join(vp_thread_t t, void **ret)
{
    if (!vp_valid_thread(t))
        return ESRCH;
    if (t == vp_thread_self())
        return EDEADLK;
    if (atomic_load_explicit(&t->detached, memory_order_acquire))
        return EINVAL;
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&t->joined, &expected, 1, memory_order_acq_rel, memory_order_acquire))
        return EINVAL;
    vp_cleanup_t join_cleanup;
    vp_cleanup_push_impl(&join_cleanup, join_claim_cleanup, t);
    for (;;)
    {
        vp_testcancel();
        vp_thread_t self = vp_thread_self();
        vp_wait_enter(self, VP_WAIT_JOIN, t);
        /* Poll the TCB state instead of relying on a timed HAL join: the
         * 2ms cadence keeps join a cancellation point, and some hosts
         * (Vita3K) ignore the sceKernelWaitThreadEnd timeout entirely.  The
         * blocking HAL join only runs once the target published EXITED, so
         * it reaps an already-finished OS thread in bounded time. */
        int r;
        if (atomic_load_explicit(&t->state, memory_order_acquire) >= VP_THREAD_EXITED)
            r = vp_hal_thread_join(t->os_thread);
        else
        {
            (void)vp_hal_sleep_us(2000);
            r = ETIMEDOUT;
        }
        vp_wait_leave(self);
        vp_testcancel();
        if (r == ETIMEDOUT)
            continue;
        if (r)
        {
            vp_cleanup_pop_impl(&join_cleanup, 1);
            return r;
        }
        break;
    }
    vp_cleanup_pop_impl(&join_cleanup, 0);
    if (ret)
        *ret = t->retval;
    (void)vp_hal_thread_delete(t->os_thread);
    atomic_store_explicit(&t->state, VP_THREAD_REAPED, memory_order_release);
    vp_thread_unregister(t);
    return 0;
}

int vp_thread_detach(vp_thread_t t)
{
    if (!vp_valid_thread(t))
        return ESRCH;
    if (atomic_load_explicit(&t->joined, memory_order_acquire))
        return EINVAL;
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&t->detached, &expected, 1, memory_order_acq_rel, memory_order_acquire))
        return EINVAL;
    if (atomic_load_explicit(&t->state, memory_order_acquire) >= VP_THREAD_EXITED)
    {
        vp_thread_unregister(t);
        (void)vp_hal_thread_delete(t->os_thread);
    }
    return 0;
}

void vp_thread_exit(void *retval)
{
    vp_thread_t t = vp_thread_self();
    if (!t)
        vp_hal_thread_exit_current();
    vp_finish_current(retval, 0);
    int detached = atomic_load_explicit(&t->detached, memory_order_acquire);
    if (detached)
        vp_thread_unregister(t);
    vp_thread_release_running(t);
    vp_current_set(NULL);
    if (detached)
        vp_hal_thread_exit_delete_current();
    vp_hal_thread_exit_current();
}

void vp_finish_current(void *retval, int canceled)
{
    vp_thread_t t = vp_current_get();
    if (!t)
        return;

    /* Close the race between a normal return and a remote async injection.
     * Once cancel_state is disabled, leaving the guard cannot self-cancel. */
    vp_async_guard_enter(t);
    atomic_store_explicit(&t->cancel_state, VP_CANCEL_DISABLE, memory_order_release);
    if (canceled)
        retval = VP_CANCELED;
    vp_run_cleanup(t);
    vp_run_tsd_destructors(t);
    vp_abandon_owned_mutexes(t);
    t->retval = retval;
    atomic_store_explicit(&t->state, VP_THREAD_EXITED, memory_order_release);
    (void)vp_hal_park_wake(&t->park);
    vp_async_guard_leave(t);
}

uint32_t vp_abs_to_us(const struct timespec *abs, int *expired)
{
    if (expired)
        *expired = 0;
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
    {
        if (expired)
            *expired = 1;
        return 0;
    }
    int64_t sec = (int64_t)abs->tv_sec - (int64_t)now.tv_sec;
    int64_t ns = (int64_t)abs->tv_nsec - (int64_t)now.tv_nsec;
    int64_t total = sec * 1000000000ll + ns;
    if (total <= 0)
    {
        if (expired)
            *expired = 1;
        return 0;
    }
    uint64_t us = (uint64_t)(total + 999) / 1000u;
    return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
}

size_t vp_debug_tcb_size(void) { return sizeof(struct vp_thread); }
size_t vp_debug_live_hal_objects(void) { return vp_hal_debug_live_objects(); }
