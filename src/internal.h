#ifndef VPTHREAD_INTERNAL_H
#define VPTHREAD_INTERNAL_H
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "vpthread/vpthread.h"
#include "vpthread/hal.h"

#define VP_THREAD_MAGIC 0x56505448u
#define VP_MUTEX_MAGIC 0x56504d58u
#define VP_COND_MAGIC 0x56504344u
#define VP_KEYS_MAX 128u
#define VP_HANDLE_SLOTS 1024u
#define VP_HANDLE_SLOT_BITS 10u
#define VP_HANDLE_SLOT_MASK ((1u << VP_HANDLE_SLOT_BITS) - 1u)
#define VP_DTOR_ITERATIONS 4

struct vp_tsd_entry
{
    vp_key_t key;
    void *value;
    struct vp_tsd_entry *next;
};

struct vp_thread
{
    uint32_t magic;
    uint32_t public_handle;
    vp_hal_thread_t os_thread;
    vp_hal_park_t park;
    _Atomic unsigned refs;
    _Atomic int running_ref_released;
    _Atomic int state;
    _Atomic int detached;
    _Atomic int joined;
    _Atomic int cancel_pending;
    _Atomic int cancel_state;
    _Atomic int cancel_type;
    vp_hal_async_guard_t async_guard;
    _Atomic int async_inflight;
    _Atomic int wait_kind;
    _Atomic(uintptr_t) wait_obj;
    void *retval;
    void *(*start)(void *);
    void *arg;
    void *cleanup;
    struct vp_tsd_entry *tsd;
    struct vp_mutex *owned_robust;
};

struct vp_mutex
{
    uint32_t magic;
    vp_hal_mutex_t os_mutex;
    int type;
    int robust;
    int protocol;
    int pshared;
    _Atomic int robust_state;
    _Atomic(uintptr_t) owner;
    unsigned recursion;
    struct vp_mutex *owned_next;
};

enum
{
    VP_ROBUST_CONSISTENT = 0,
    VP_ROBUST_OWNER_DEAD = 1,
    VP_ROBUST_NOT_RECOVERABLE = 2
};
enum
{
    VP_WAIT_NONE = 0,
    VP_WAIT_COND,
    VP_WAIT_JOIN,
    VP_WAIT_MUTEX,
    VP_WAIT_SLEEP
};

struct vp_cond_waiter
{
    struct vp_cond_waiter *prev;
    struct vp_cond_waiter *next;
    vp_thread_t thread;
    int queued;
};

struct vp_rwlock
{
    uint32_t magic;
    vp_mutex_t *mutex;
    vp_cond_t *cond;
    unsigned readers;
    unsigned waiting_writers;
    vp_thread_t writer;
};

struct vp_barrier
{
    uint32_t magic;
    vp_mutex_t *mutex;
    vp_cond_t *cond;
    unsigned threshold;
    unsigned waiting;
    unsigned active;
    unsigned generation;
    int destroying;
};

#define VP_RWLOCK_MAGIC 0x56505257u
#define VP_BARRIER_MAGIC 0x56504252u

struct vp_cond
{
    uint32_t magic;
    atomic_flag guard;
    struct vp_cond_waiter *head;
    struct vp_cond_waiter *tail;
    unsigned waiters;
};

static inline int vp_valid_thread(vp_thread_t t) { return t && t->magic == VP_THREAD_MAGIC; }
static inline void vp_thread_get(vp_thread_t t)
{
    if (t)
        atomic_fetch_add_explicit(&t->refs, 1, memory_order_relaxed);
}
void vp_thread_put(vp_thread_t t);
void vp_thread_release_running(vp_thread_t t);

void vp_async_guard_enter(vp_thread_t t);
void vp_async_guard_leave(vp_thread_t t);
void vp_async_guard_leave_no_cancel(vp_thread_t t);
void vp_wait_enter(vp_thread_t t, int kind, void *obj);
void vp_wait_leave(vp_thread_t t);
void vp_testcancel_async_only(void);
void vp_finish_current(void *retval, int canceled);
void vp_run_cleanup(vp_thread_t t);
void vp_run_tsd_destructors(vp_thread_t t);
void vp_abandon_owned_mutexes(vp_thread_t t);
void vp_cleanup_link_raw(void *ctx);
void vp_cleanup_pop_raw(void *ctx, int execute);
int vp_cond_wait_nocancel(vp_cond_t *c, vp_mutex_t *m);
int vp_cond_timedwait_nocancel(vp_cond_t *c, vp_mutex_t *m, const struct timespec *abs);
uint32_t vp_abs_to_us(const struct timespec *abs, int *expired);
void vp_thread_unregister(vp_thread_t t);

#endif
