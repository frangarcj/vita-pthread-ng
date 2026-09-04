#include "vpthread/abi/pthread.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "internal.h"

#if defined(VPTHREAD_HAL_VITA) || defined(VPTHREAD_HAL_VITA_SCELIBC)
#define P(name) name
/* Referenced from thread.c so that linking any part of vpthread extracts this
 * object.  libgcc's gthread layer (emutls behind _Thread_local, libstdc++
 * locking) binds to the pthread_* symbols defined here through WEAK
 * references, and weak references never extract archive members on their own;
 * without this anchor an app that uses only the vp_* API would link with
 * __gthread_active_p() == 0 and emutls would silently degrade to a single
 * process-global slot shared by every thread. */
const char vp_posix_gthread_anchor = 0;
#else
#define P(name) vp_posix_##name
#endif

#if defined(VPTHREAD_HAL_SCE_SIM)
extern int pthread_sigmask(int, const sigset_t *, sigset_t *);
#endif

#define PTR_SENTINEL(p, n) ((intptr_t)(p) == -(n))
/* A lazily-initialized slot holds NULL, a small negative sentinel, or a real
 * object pointer.  Never classify by pointer sign: on Vita the newlib heap
 * lives at 0x88000000+, so valid pointers are negative as intptr_t. */
#define PTR_IS_OBJ(p) ((p) != NULL && (uintptr_t)(p) < (uintptr_t)-3)

struct pthread_attr_t_
{
    size_t stack_size;
    void *stack_addr;
    size_t guard_size;
    int detach_state;
    int sched_policy;
    struct sched_param sched_param;
    int inherit_sched;
    int scope;
};
struct pthread_mutexattr_t_
{
    int type, pshared, protocol, robust, prioceiling;
};
struct pthread_condattr_t_
{
    int pshared;
    clockid_t clock_id;
};
struct pthread_rwlockattr_t_
{
    int pshared;
};
struct pthread_barrierattr_t_
{
    int pshared;
};
struct pthread_cond_t_
{
    vp_cond_t *core;
    clockid_t clock_id;
};
struct pthread_spinlock_t_
{
    atomic_flag flag;
};

static int valid_pshared(int v) { return v == PTHREAD_PROCESS_PRIVATE || v == PTHREAD_PROCESS_SHARED; }
static int valid_mutex_type(int v) { return v == PTHREAD_MUTEX_NORMAL || v == PTHREAD_MUTEX_RECURSIVE || v == PTHREAD_MUTEX_ERRORCHECK; }

int P(pthread_attr_init)(pthread_attr_t *out)
{
    if (!out)
        return EINVAL;
    struct pthread_attr_t_ *a = calloc(1, sizeof(*a));
    if (!a)
        return ENOMEM;
    a->stack_size = 0x20000u;
    a->guard_size = 8192u;
    a->detach_state = PTHREAD_CREATE_JOINABLE;
    a->sched_policy = SCHED_OTHER;
    a->sched_param.sched_priority = 0;
    a->inherit_sched = PTHREAD_INHERIT_SCHED;
    a->scope = PTHREAD_SCOPE_SYSTEM;
    *out = a;
    return 0;
}
int P(pthread_attr_destroy)(pthread_attr_t *a)
{
    if (!a || !*a)
        return EINVAL;
    free(*a);
    *a = NULL;
    return 0;
}
int P(pthread_attr_getdetachstate)(const pthread_attr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->detach_state;
    return 0;
}
int P(pthread_attr_setdetachstate)(pthread_attr_t *a, int v)
{
    if (!a || !*a || (v != PTHREAD_CREATE_JOINABLE && v != PTHREAD_CREATE_DETACHED))
        return EINVAL;
    (*a)->detach_state = v;
    return 0;
}
int P(pthread_attr_getstacksize)(const pthread_attr_t *a, size_t *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->stack_size;
    return 0;
}
int P(pthread_attr_setstacksize)(pthread_attr_t *a, size_t v)
{
    if (!a || !*a || v < PTHREAD_STACK_MIN)
        return EINVAL;
    (*a)->stack_size = v;
    return 0;
}
int P(pthread_attr_getstackaddr)(const pthread_attr_t *a, void **v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->stack_addr;
    return 0;
}
int P(pthread_attr_setstackaddr)(pthread_attr_t *a, void *v)
{
    if (!a || !*a)
        return EINVAL;
    (*a)->stack_addr = v;
    return 0;
}
int P(pthread_attr_getstack)(const pthread_attr_t *a, void **addr, size_t *sz)
{
    if (!a || !*a || !addr || !sz)
        return EINVAL;
    *addr = (*a)->stack_addr;
    *sz = (*a)->stack_size;
    return 0;
}
int P(pthread_attr_setstack)(pthread_attr_t *a, void *addr, size_t sz)
{
    if (!a || !*a || !addr || sz < PTHREAD_STACK_MIN || sz > SIZE_MAX / 4u)
        return EINVAL;
    (*a)->stack_addr = addr;
    (*a)->stack_size = sz;
    return 0;
}
int P(pthread_attr_getguardsize)(const pthread_attr_t *a, size_t *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->guard_size;
    return 0;
}
int P(pthread_attr_setguardsize)(pthread_attr_t *a, size_t v)
{
    if (!a || !*a || v > SIZE_MAX - 4095u)
        return EINVAL;
    (*a)->guard_size = v;
    return 0;
}
int P(pthread_attr_getschedparam)(const pthread_attr_t *a, struct sched_param *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->sched_param;
    return 0;
}
int P(pthread_attr_setschedparam)(pthread_attr_t *a, const struct sched_param *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    (*a)->sched_param = *v;
    return 0;
}
int P(pthread_attr_getschedpolicy)(const pthread_attr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->sched_policy;
    return 0;
}
int P(pthread_attr_setschedpolicy)(pthread_attr_t *a, int v)
{
    if (!a || !*a || (v != SCHED_OTHER && v != SCHED_FIFO && v != SCHED_RR))
        return EINVAL;
    (*a)->sched_policy = v;
    return 0;
}
int P(pthread_attr_getinheritsched)(const pthread_attr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->inherit_sched;
    return 0;
}
int P(pthread_attr_setinheritsched)(pthread_attr_t *a, int v)
{
    if (!a || !*a || (v != PTHREAD_INHERIT_SCHED && v != PTHREAD_EXPLICIT_SCHED))
        return EINVAL;
    (*a)->inherit_sched = v;
    return 0;
}
int P(pthread_attr_getscope)(const pthread_attr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->scope;
    return 0;
}
int P(pthread_attr_setscope)(pthread_attr_t *a, int v)
{
    if (!a || !*a)
        return EINVAL;
    if (v == PTHREAD_SCOPE_PROCESS)
        return ENOTSUP;
    if (v != PTHREAD_SCOPE_SYSTEM)
        return EINVAL;
    (*a)->scope = v;
    return 0;
}

static void attr_to_core(const pthread_attr_t *a, vp_thread_attr_t *v)
{
    vp_thread_attr_init(v);
    if (!a || !*a)
        return;
    v->stack_size = (*a)->stack_size;
    v->detached = ((*a)->detach_state == PTHREAD_CREATE_DETACHED);
    if ((*a)->inherit_sched == PTHREAD_EXPLICIT_SCHED)
        v->priority = (*a)->sched_param.sched_priority;
}
int P(pthread_atfork)(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    return vp_hal_atfork(prepare, parent, child);
}
int P(pthread_create)(pthread_t *out, const pthread_attr_t *a, void *(*fn)(void *), void *arg)
{
    if (!out)
        return EINVAL;
    vp_thread_attr_t va;
    attr_to_core(a, &va);
    return vp_thread_create_handle(out, &va, fn, arg);
}
static vp_thread_t acquire(pthread_t h) { return vp_thread_acquire_handle(h); }
static void release_thread_cleanup(void *arg) { vp_thread_release((vp_thread_t)arg); }
int P(pthread_join)(pthread_t h, void **ret)
{
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    vp_cleanup_t cleanup;
    vp_cleanup_push_impl(&cleanup, release_thread_cleanup, t);
    int r = vp_thread_join(t, ret);
    vp_cleanup_pop_impl(&cleanup, 0);
    vp_thread_release(t);
    return r;
}
int P(pthread_detach)(pthread_t h)
{
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    int r = vp_thread_detach(t);
    vp_thread_release(t);
    return r;
}
int P(pthread_equal)(pthread_t a, pthread_t b) { return a == b; }
pthread_t P(pthread_self)(void)
{
    vp_thread_t t = vp_thread_self();
    return t ? vp_thread_handle(t) : 0;
}
int P(pthread_getschedparam)(pthread_t h, int *policy, struct sched_param *param)
{
    if (!policy || !param)
        return EINVAL;
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    *policy = SCHED_OTHER;
    param->sched_priority = 0;
    vp_thread_release(t);
    return 0;
}
int P(pthread_setschedparam)(pthread_t h, int policy, const struct sched_param *param)
{
    if (!param || (policy != SCHED_OTHER && policy != SCHED_FIFO && policy != SCHED_RR))
        return EINVAL;
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    int r = vp_hal_thread_setpriority(t->os_thread, param->sched_priority);
    vp_thread_release(t);
    return r;
}
int P(pthread_setschedprio)(pthread_t h, int priority)
{
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    int r = vp_hal_thread_setpriority(t->os_thread, priority);
    vp_thread_release(t);
    return r;
}
void P(pthread_exit)(void *ret)
{
    vp_thread_exit(ret);
    __builtin_unreachable();
}
int P(pthread_cancel)(pthread_t h)
{
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    int r = vp_cancel(t);
    vp_thread_release(t);
    return r;
}
int P(pthread_setcancelstate)(int s, int *old)
{
    int vs = s == PTHREAD_CANCEL_ENABLE ? VP_CANCEL_ENABLE : s == PTHREAD_CANCEL_DISABLE ? VP_CANCEL_DISABLE
                                                                                         : -1;
    int vo = 0;
    if (vs < 0)
        return EINVAL;
    int r = vp_setcancelstate(vs, old ? &vo : NULL);
    if (!r && old)
        *old = vo == VP_CANCEL_ENABLE ? PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE;
    return r;
}
int P(pthread_setcanceltype)(int s, int *old)
{
    int vs = s == PTHREAD_CANCEL_ASYNCHRONOUS ? VP_CANCEL_ASYNCHRONOUS : s == PTHREAD_CANCEL_DEFERRED ? VP_CANCEL_DEFERRED
                                                                                                      : -1;
    int vo = 0;
    if (vs < 0)
        return EINVAL;
    int r = vp_setcanceltype(vs, old ? &vo : NULL);
    if (!r && old)
        *old = vo == VP_CANCEL_ASYNCHRONOUS ? PTHREAD_CANCEL_ASYNCHRONOUS : PTHREAD_CANCEL_DEFERRED;
    return r;
}
void P(pthread_testcancel)(void) { vp_testcancel(); }

struct once_reset_ctx
{
    pthread_once_t *once;
};
static void once_reset(void *p)
{
    struct once_reset_ctx *c = p;
    __atomic_store_n(&c->once->state, 0, __ATOMIC_RELEASE);
    free(c);
}
int P(pthread_once)(pthread_once_t *o, void (*init)(void))
{
    if (!o || !init)
        return EINVAL;
    for (;;)
    {
        int s = __atomic_load_n(&o->state, __ATOMIC_ACQUIRE);
        if (s == 2)
            return 0;
        if (s == 0)
        {
            int expected = 0;
            if (__atomic_compare_exchange_n(&o->state, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            {
                struct once_reset_ctx *rc = malloc(sizeof(*rc));
                if (!rc)
                {
                    __atomic_store_n(&o->state, 0, __ATOMIC_RELEASE);
                    return ENOMEM;
                }
                rc->once = o;
                vp_cleanup_t cleanup;
                vp_cleanup_push_impl(&cleanup, once_reset, rc);
                init();
                vp_cleanup_pop_impl(&cleanup, 0);
                free(rc);
                __atomic_store_n(&o->state, 2, __ATOMIC_RELEASE);
#if !defined(VPTHREAD_HAL_VITA_SCELIBC)
                o->done = 1;
#endif
                return 0;
            }
        }
        vp_hal_yield();
    }
}

int P(pthread_key_create)(pthread_key_t *out, void (*dtor)(void *))
{
    if (!out)
        return EINVAL;
    vp_key_t k;
    int r = vp_key_create(&k, dtor);
    if (!r)
        *out = (pthread_key_t)(uintptr_t)k;
    return r;
}
int P(pthread_key_delete)(pthread_key_t k) { return vp_key_delete((vp_key_t)(uintptr_t)k); }
int P(pthread_setspecific)(pthread_key_t k, const void *v) { return vp_setspecific((vp_key_t)(uintptr_t)k, v); }
void *P(pthread_getspecific)(pthread_key_t k) { return vp_getspecific((vp_key_t)(uintptr_t)k); }

int P(pthread_mutexattr_init)(pthread_mutexattr_t *out)
{
    if (!out)
        return EINVAL;
    struct pthread_mutexattr_t_ *a = calloc(1, sizeof(*a));
    if (!a)
        return ENOMEM;
    a->type = PTHREAD_MUTEX_DEFAULT;
    a->pshared = PTHREAD_PROCESS_PRIVATE;
    a->protocol = PTHREAD_PRIO_NONE;
    a->robust = PTHREAD_MUTEX_STALLED;
    *out = a;
    return 0;
}
int P(pthread_mutexattr_destroy)(pthread_mutexattr_t *a)
{
    if (!a || !*a)
        return EINVAL;
    free(*a);
    *a = NULL;
    return 0;
}
int P(pthread_mutexattr_gettype)(const pthread_mutexattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->type;
    return 0;
}
int P(pthread_mutexattr_settype)(pthread_mutexattr_t *a, int v)
{
    if (!a || !*a || !valid_mutex_type(v))
        return EINVAL;
    (*a)->type = v;
    return 0;
}
int P(pthread_mutexattr_getpshared)(const pthread_mutexattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->pshared;
    return 0;
}
int P(pthread_mutexattr_setpshared)(pthread_mutexattr_t *a, int v)
{
    if (!a || !*a || !valid_pshared(v))
        return EINVAL;
    (*a)->pshared = v;
    return 0;
}
int P(pthread_mutexattr_getprotocol)(const pthread_mutexattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->protocol;
    return 0;
}
int P(pthread_mutexattr_setprotocol)(pthread_mutexattr_t *a, int v)
{
    if (!a || !*a || (v != PTHREAD_PRIO_NONE && v != PTHREAD_PRIO_INHERIT && v != PTHREAD_PRIO_PROTECT))
        return EINVAL;
    (*a)->protocol = v;
    return 0;
}
int P(pthread_mutexattr_getrobust)(const pthread_mutexattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->robust;
    return 0;
}
int P(pthread_mutexattr_setrobust)(pthread_mutexattr_t *a, int v)
{
    if (!a || !*a || (v != PTHREAD_MUTEX_STALLED && v != PTHREAD_MUTEX_ROBUST))
        return EINVAL;
    (*a)->robust = v;
    return 0;
}
int P(pthread_mutexattr_getprioceiling)(const pthread_mutexattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->prioceiling;
    return 0;
}
int P(pthread_mutexattr_setprioceiling)(pthread_mutexattr_t *a, int v)
{
    if (!a || !*a)
        return EINVAL;
    (*a)->prioceiling = v;
    return 0;
}

static int mutex_type_from_sentinel(pthread_mutex_t m)
{
    if (PTR_SENTINEL(m, 2))
        return VP_MUTEX_RECURSIVE;
    if (PTR_SENTINEL(m, 3))
        return VP_MUTEX_ERRORCHECK;
    return VP_MUTEX_NORMAL;
}
static vp_mutex_t *mutex_get(pthread_mutex_t *slot, int create)
{
    if (!slot)
        return NULL;
    pthread_mutex_t p = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    if (PTR_IS_OBJ(p))
        return (vp_mutex_t *)p;
    if (!create || (!PTR_SENTINEL(p, 1) && !PTR_SENTINEL(p, 2) && !PTR_SENTINEL(p, 3)))
        return NULL;
    vp_mutex_t *fresh = NULL;
    if (vp_mutex_init(&fresh, mutex_type_from_sentinel(p)))
        return NULL;
    pthread_mutex_t wanted = (pthread_mutex_t)fresh, expected = p;
    if (!__atomic_compare_exchange_n(slot, &expected, wanted, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
    {
        vp_mutex_destroy(fresh);
        p = expected;
    }
    else
        p = wanted;
    return (vp_mutex_t *)p;
}
int P(pthread_mutex_init)(pthread_mutex_t *slot, const pthread_mutexattr_t *a)
{
    if (!slot)
        return EINVAL;
    vp_mutex_t *m = NULL;
    int type = VP_MUTEX_NORMAL, rob = VP_MUTEX_STALLED, proto = VP_PRIO_NONE, ps = 0;
    if (a && *a)
    {
        type = (*a)->type;
        rob = (*a)->robust;
        proto = (*a)->protocol;
        ps = (*a)->pshared;
    }
    int r = vp_mutex_init_ex(&m, type, rob, proto, ps);
    if (r)
        return r;
    *slot = (pthread_mutex_t)m;
    return 0;
}
int P(pthread_mutex_destroy)(pthread_mutex_t *slot)
{
    if (!slot)
        return EINVAL;
    pthread_mutex_t p = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    if (PTR_SENTINEL(p, 1) || PTR_SENTINEL(p, 2) || PTR_SENTINEL(p, 3))
    {
        *slot = NULL;
        return 0;
    }
    if (!PTR_IS_OBJ(p))
        return EINVAL;
    int r = vp_mutex_destroy((vp_mutex_t *)p);
    if (!r)
        *slot = NULL;
    return r;
}
int P(pthread_mutex_lock)(pthread_mutex_t *s)
{
    vp_mutex_t *m = mutex_get(s, 1);
    return m ? vp_mutex_lock(m) : EINVAL;
}
int P(pthread_mutex_trylock)(pthread_mutex_t *s)
{
    vp_mutex_t *m = mutex_get(s, 1);
    return m ? vp_mutex_trylock(m) : EINVAL;
}
int P(pthread_mutex_timedlock)(pthread_mutex_t *s, const struct timespec *a)
{
    vp_mutex_t *m = mutex_get(s, 1);
    return m ? vp_mutex_timedlock(m, a) : EINVAL;
}
int P(pthread_mutex_unlock)(pthread_mutex_t *s)
{
    vp_mutex_t *m = mutex_get(s, 1);
    return m ? vp_mutex_unlock(m) : EINVAL;
}
int P(pthread_mutex_consistent)(pthread_mutex_t *s)
{
    vp_mutex_t *m = mutex_get(s, 0);
    return m ? vp_mutex_consistent(m) : EINVAL;
}

int P(pthread_condattr_init)(pthread_condattr_t *out)
{
    if (!out)
        return EINVAL;
    struct pthread_condattr_t_ *a = calloc(1, sizeof(*a));
    if (!a)
        return ENOMEM;
    a->pshared = PTHREAD_PROCESS_PRIVATE;
    a->clock_id = CLOCK_REALTIME;
    *out = a;
    return 0;
}
int P(pthread_condattr_destroy)(pthread_condattr_t *a)
{
    if (!a || !*a)
        return EINVAL;
    free(*a);
    *a = NULL;
    return 0;
}
int P(pthread_condattr_getpshared)(const pthread_condattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->pshared;
    return 0;
}
int P(pthread_condattr_setpshared)(pthread_condattr_t *a, int v)
{
    if (!a || !*a || !valid_pshared(v))
        return EINVAL;
    (*a)->pshared = v;
    return 0;
}
int P(pthread_condattr_getclock)(const pthread_condattr_t *a, clockid_t *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->clock_id;
    return 0;
}
int P(pthread_condattr_setclock)(pthread_condattr_t *a, clockid_t v)
{
    if (!a || !*a || (v != CLOCK_REALTIME && v != CLOCK_MONOTONIC))
        return EINVAL;
    (*a)->clock_id = v;
    return 0;
}
static struct pthread_cond_t_ *cond_alloc(clockid_t clk)
{
    struct pthread_cond_t_ *c = calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    if (vp_cond_init(&c->core))
    {
        free(c);
        return NULL;
    }
    c->clock_id = clk;
    return c;
}
static struct pthread_cond_t_ *cond_get(pthread_cond_t *slot, int create)
{
    if (!slot)
        return NULL;
    pthread_cond_t p = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    if (PTR_IS_OBJ(p))
        return p;
    if (!create || !PTR_SENTINEL(p, 1))
        return NULL;
    struct pthread_cond_t_ *fresh = cond_alloc(CLOCK_REALTIME);
    if (!fresh)
        return NULL;
    pthread_cond_t expected = p;
    if (!__atomic_compare_exchange_n(slot, &expected, fresh, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
    {
        vp_cond_destroy(fresh->core);
        free(fresh);
        p = expected;
    }
    else
        p = fresh;
    return p;
}
int P(pthread_cond_init)(pthread_cond_t *s, const pthread_condattr_t *a)
{
    if (!s)
        return EINVAL;
    clockid_t clk = (a && *a) ? (*a)->clock_id : CLOCK_REALTIME;
    struct pthread_cond_t_ *c = cond_alloc(clk);
    if (!c)
        return ENOMEM;
    *s = c;
    return 0;
}
int P(pthread_cond_destroy)(pthread_cond_t *s)
{
    if (!s)
        return EINVAL;
    pthread_cond_t p = __atomic_load_n(s, __ATOMIC_ACQUIRE);
    if (PTR_SENTINEL(p, 1))
    {
        *s = NULL;
        return 0;
    }
    if (!PTR_IS_OBJ(p))
        return EINVAL;
    int r = vp_cond_destroy(p->core);
    if (!r)
    {
        free(p);
        *s = NULL;
    }
    return r;
}
int P(pthread_cond_wait)(pthread_cond_t *c, pthread_mutex_t *m)
{
    struct pthread_cond_t_ *pc = cond_get(c, 1);
    vp_mutex_t *pm = mutex_get(m, 1);
    return pc && pm ? vp_cond_wait(pc->core, pm) : EINVAL;
}
static int clock_abs_to_realtime(clockid_t clk, const struct timespec *in, struct timespec *out)
{
    if (clk == CLOCK_REALTIME)
    {
        *out = *in;
        return 0;
    }
    struct timespec now_src, now_rt;
    if (clock_gettime(clk, &now_src) || clock_gettime(CLOCK_REALTIME, &now_rt))
        return EINVAL;
    int64_t ns = (int64_t)(in->tv_sec - now_src.tv_sec) * 1000000000ll + (int64_t)in->tv_nsec - now_src.tv_nsec;
    if (ns <= 0)
    {
        *out = now_rt;
        return 0;
    }
    int64_t rt_ns = (int64_t)now_rt.tv_nsec + ns;
    out->tv_sec = now_rt.tv_sec + (time_t)(rt_ns / 1000000000ll);
    out->tv_nsec = (long)(rt_ns % 1000000000ll);
    return 0;
}
int P(pthread_cond_timedwait)(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *a)
{
    if (!a)
        return EINVAL;
    struct pthread_cond_t_ *pc = cond_get(c, 1);
    vp_mutex_t *pm = mutex_get(m, 1);
    if (!pc || !pm)
        return EINVAL;
    struct timespec rt;
    int r = clock_abs_to_realtime(pc->clock_id, a, &rt);
    return r ? r : vp_cond_timedwait(pc->core, pm, &rt);
}
int P(pthread_cond_signal)(pthread_cond_t *c)
{
    struct pthread_cond_t_ *pc = cond_get(c, 1);
    return pc ? vp_cond_signal(pc->core) : EINVAL;
}
int P(pthread_cond_broadcast)(pthread_cond_t *c)
{
    struct pthread_cond_t_ *pc = cond_get(c, 1);
    return pc ? vp_cond_broadcast(pc->core) : EINVAL;
}

int P(pthread_rwlockattr_init)(pthread_rwlockattr_t *out)
{
    if (!out)
        return EINVAL;
    struct pthread_rwlockattr_t_ *a = calloc(1, sizeof(*a));
    if (!a)
        return ENOMEM;
    a->pshared = PTHREAD_PROCESS_PRIVATE;
    *out = a;
    return 0;
}
int P(pthread_rwlockattr_destroy)(pthread_rwlockattr_t *a)
{
    if (!a || !*a)
        return EINVAL;
    free(*a);
    *a = NULL;
    return 0;
}
int P(pthread_rwlockattr_getpshared)(const pthread_rwlockattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->pshared;
    return 0;
}
int P(pthread_rwlockattr_setpshared)(pthread_rwlockattr_t *a, int v)
{
    if (!a || !*a || !valid_pshared(v))
        return EINVAL;
    (*a)->pshared = v;
    return 0;
}
static vp_rwlock_t *rw_get(pthread_rwlock_t *s, int create)
{
    if (!s)
        return NULL;
    pthread_rwlock_t p = __atomic_load_n(s, __ATOMIC_ACQUIRE);
    if (PTR_IS_OBJ(p))
        return (vp_rwlock_t *)p;
    if (!create || !PTR_SENTINEL(p, 1))
        return NULL;
    vp_rwlock_t *f = NULL;
    if (vp_rwlock_init(&f))
        return NULL;
    pthread_rwlock_t expected = p, wanted = (pthread_rwlock_t)f;
    if (!__atomic_compare_exchange_n(s, &expected, wanted, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
    {
        vp_rwlock_destroy(f);
        p = expected;
    }
    else
        p = wanted;
    return (vp_rwlock_t *)p;
}
int P(pthread_rwlock_init)(pthread_rwlock_t *s, const pthread_rwlockattr_t *a)
{
    (void)a;
    if (!s)
        return EINVAL;
    vp_rwlock_t *r = NULL;
    int e = vp_rwlock_init(&r);
    if (!e)
        *s = (pthread_rwlock_t)r;
    return e;
}
int P(pthread_rwlock_destroy)(pthread_rwlock_t *s)
{
    if (!s)
        return EINVAL;
    pthread_rwlock_t p = __atomic_load_n(s, __ATOMIC_ACQUIRE);
    if (PTR_SENTINEL(p, 1))
    {
        *s = NULL;
        return 0;
    }
    if (!PTR_IS_OBJ(p))
        return EINVAL;
    int r = vp_rwlock_destroy((vp_rwlock_t *)p);
    if (!r)
        *s = NULL;
    return r;
}
int P(pthread_rwlock_rdlock)(pthread_rwlock_t *s)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_rdlock(r) : EINVAL;
}
int P(pthread_rwlock_tryrdlock)(pthread_rwlock_t *s)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_tryrdlock(r) : EINVAL;
}
int P(pthread_rwlock_timedrdlock)(pthread_rwlock_t *s, const struct timespec *a)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_timedrdlock(r, a) : EINVAL;
}
int P(pthread_rwlock_wrlock)(pthread_rwlock_t *s)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_wrlock(r) : EINVAL;
}
int P(pthread_rwlock_trywrlock)(pthread_rwlock_t *s)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_trywrlock(r) : EINVAL;
}
int P(pthread_rwlock_timedwrlock)(pthread_rwlock_t *s, const struct timespec *a)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_timedwrlock(r, a) : EINVAL;
}
int P(pthread_rwlock_unlock)(pthread_rwlock_t *s)
{
    vp_rwlock_t *r = rw_get(s, 1);
    return r ? vp_rwlock_unlock(r) : EINVAL;
}

int P(pthread_barrierattr_init)(pthread_barrierattr_t *out)
{
    if (!out)
        return EINVAL;
    struct pthread_barrierattr_t_ *a = calloc(1, sizeof(*a));
    if (!a)
        return ENOMEM;
    a->pshared = PTHREAD_PROCESS_PRIVATE;
    *out = a;
    return 0;
}
int P(pthread_barrierattr_destroy)(pthread_barrierattr_t *a)
{
    if (!a || !*a)
        return EINVAL;
    free(*a);
    *a = NULL;
    return 0;
}
int P(pthread_barrierattr_getpshared)(const pthread_barrierattr_t *a, int *v)
{
    if (!a || !*a || !v)
        return EINVAL;
    *v = (*a)->pshared;
    return 0;
}
int P(pthread_barrierattr_setpshared)(pthread_barrierattr_t *a, int v)
{
    if (!a || !*a || !valid_pshared(v))
        return EINVAL;
    (*a)->pshared = v;
    return 0;
}
int P(pthread_barrier_init)(pthread_barrier_t *s, const pthread_barrierattr_t *a, unsigned n)
{
    (void)a;
    if (!s)
        return EINVAL;
    vp_barrier_t *b = NULL;
    int r = vp_barrier_init(&b, n);
    if (!r)
        *s = (pthread_barrier_t)b;
    return r;
}
int P(pthread_barrier_destroy)(pthread_barrier_t *s)
{
    if (!s || !*s)
        return EINVAL;
    int r = vp_barrier_destroy((vp_barrier_t *)*s);
    if (!r)
        *s = NULL;
    return r;
}
int P(pthread_barrier_wait)(pthread_barrier_t *s) { return s && *s ? vp_barrier_wait((vp_barrier_t *)*s) : EINVAL; }

static struct pthread_spinlock_t_ *spin_get(pthread_spinlock_t *s, int create)
{
    if (!s)
        return NULL;
    pthread_spinlock_t p = __atomic_load_n(s, __ATOMIC_ACQUIRE);
    if (PTR_IS_OBJ(p))
        return p;
    if (!create || !PTR_SENTINEL(p, 1))
        return NULL;
    struct pthread_spinlock_t_ *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    atomic_flag_clear(&f->flag);
    pthread_spinlock_t expected = p;
    if (!__atomic_compare_exchange_n(s, &expected, f, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
    {
        free(f);
        p = expected;
    }
    else
        p = f;
    return p;
}
int P(pthread_spin_init)(pthread_spinlock_t *s, int ps)
{
    if (!s || !valid_pshared(ps))
        return EINVAL;
    struct pthread_spinlock_t_ *p = calloc(1, sizeof(*p));
    if (!p)
        return ENOMEM;
    atomic_flag_clear(&p->flag);
    *s = p;
    return 0;
}
int P(pthread_spin_destroy)(pthread_spinlock_t *s)
{
    if (!s)
        return EINVAL;
    pthread_spinlock_t p = __atomic_load_n(s, __ATOMIC_ACQUIRE);
    if (PTR_SENTINEL(p, 1))
    {
        *s = NULL;
        return 0;
    }
    if (!PTR_IS_OBJ(p))
        return EINVAL;
    free(p);
    *s = NULL;
    return 0;
}
int P(pthread_spin_lock)(pthread_spinlock_t *s)
{
    struct pthread_spinlock_t_ *p = spin_get(s, 1);
    if (!p)
        return EINVAL;
    while (atomic_flag_test_and_set_explicit(&p->flag, memory_order_acquire))
        vp_hal_yield();
    return 0;
}
int P(pthread_spin_trylock)(pthread_spinlock_t *s)
{
    struct pthread_spinlock_t_ *p = spin_get(s, 1);
    if (!p)
        return EINVAL;
    return atomic_flag_test_and_set_explicit(&p->flag, memory_order_acquire) ? EBUSY : 0;
}
int P(pthread_spin_unlock)(pthread_spinlock_t *s)
{
    struct pthread_spinlock_t_ *p = spin_get(s, 1);
    if (!p)
        return EINVAL;
    atomic_flag_clear_explicit(&p->flag, memory_order_release);
    return 0;
}

int P(pthread_kill)(pthread_t h, int sig)
{
    if (sig < 0)
        return EINVAL;
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    int r = vp_hal_thread_kill(t->os_thread, sig);
    vp_thread_release(t);
    return r;
}
int P(pthread_sigmask)(int how, const sigset_t *set, sigset_t *old)
{
#if defined(VPTHREAD_HAL_SCE_SIM)
    return pthread_sigmask(how, set, old);
#elif defined(VPTHREAD_HAL_VITA) || defined(VPTHREAD_HAL_VITA_SCELIBC)
    (void)how;
    (void)set;
    (void)old;
    return ENOTSUP;
#else
    return sigprocmask(how, set, old) == 0 ? 0 : errno;
#endif
}
int P(pthread_getcpuclockid)(pthread_t h, clockid_t *clk)
{
    if (!clk)
        return EINVAL;
    vp_thread_t t = acquire(h);
    if (!t)
        return ESRCH;
    vp_thread_release(t); /* Vita has no portable per-thread CPU clock in the HAL yet. */
    *clk = CLOCK_MONOTONIC;
    return 0;
}

void vp_posix_cleanup_push(vp_posix_cleanup_t *c, void (*routine)(void *), void *arg) { vp_cleanup_push_impl((vp_cleanup_t *)c, routine, arg); }
void vp_posix_cleanup_pop(vp_posix_cleanup_t *c, int execute) { vp_cleanup_pop_impl((vp_cleanup_t *)c, execute); }
