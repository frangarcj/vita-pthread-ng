#ifndef VPTHREAD_H
#define VPTHREAD_H
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct vp_thread *vp_thread_t;
    typedef struct vp_mutex vp_mutex_t;
    typedef struct vp_cond vp_cond_t;
    typedef struct vp_rwlock vp_rwlock_t;
    typedef struct vp_barrier vp_barrier_t;
    typedef struct vp_once
    {
        _Atomic int state;
    } vp_once_t;
    typedef unsigned vp_key_t;
    typedef uint32_t vp_pthread_handle_t;

    typedef struct vp_thread_attr
    {
        size_t stack_size;
        int priority;
        int detached;
    } vp_thread_attr_t;
    typedef struct vp_cleanup
    {
        void (*routine)(void *);
        void *arg;
        int canceltype;
        void *prev;
    } vp_cleanup_t;

    enum
    {
        VP_THREAD_NEW = 0,
        VP_THREAD_RUNNING,
        VP_THREAD_EXITED,
        VP_THREAD_REAPED
    };
    enum
    {
        VP_MUTEX_NORMAL = 0,
        VP_MUTEX_RECURSIVE = 1,
        VP_MUTEX_ERRORCHECK = 2
    };
    enum
    {
        VP_MUTEX_STALLED = 0,
        VP_MUTEX_ROBUST = 1
    };
    enum
    {
        VP_PRIO_NONE = 0,
        VP_PRIO_INHERIT = 1,
        VP_PRIO_PROTECT = 2
    };
#define VP_BARRIER_SERIAL_THREAD (-1)
#define VP_ONCE_INIT {0}
    enum
    {
        VP_CANCEL_ENABLE = 0,
        VP_CANCEL_DISABLE = 1
    };
    enum
    {
        VP_CANCEL_DEFERRED = 0,
        VP_CANCEL_ASYNCHRONOUS = 1
    };
#define VP_CANCELED ((void *)(intptr_t)-1)

    int vp_thread_attr_init(vp_thread_attr_t *a);
    int vp_thread_create(vp_thread_t *out, const vp_thread_attr_t *a, void *(*fn)(void *), void *arg);
    int vp_thread_join(vp_thread_t t, void **ret);
    int vp_thread_detach(vp_thread_t t);
    void vp_thread_exit(void *retval);
    vp_thread_t vp_thread_self(void);
    int vp_thread_equal(vp_thread_t a, vp_thread_t b);

    int vp_thread_create_handle(vp_pthread_handle_t *out, const vp_thread_attr_t *a, void *(*fn)(void *), void *arg);
    vp_thread_t vp_thread_acquire_handle(vp_pthread_handle_t h);
    void vp_thread_release(vp_thread_t t);
    vp_pthread_handle_t vp_thread_handle(vp_thread_t t);

    int vp_mutex_init(vp_mutex_t **out, int type);
    int vp_mutex_init_ex(vp_mutex_t **out, int type, int robust, int protocol, int pshared);
    int vp_mutex_destroy(vp_mutex_t *m);
    int vp_mutex_lock(vp_mutex_t *m);
    int vp_mutex_trylock(vp_mutex_t *m);
    int vp_mutex_timedlock(vp_mutex_t *m, const struct timespec *abs);
    int vp_mutex_unlock(vp_mutex_t *m);
    int vp_mutex_consistent(vp_mutex_t *m);

    int vp_cond_init(vp_cond_t **out);
    int vp_cond_destroy(vp_cond_t *c);
    int vp_cond_wait(vp_cond_t *c, vp_mutex_t *m);
    int vp_cond_timedwait(vp_cond_t *c, vp_mutex_t *m, const struct timespec *abs);
    int vp_cond_signal(vp_cond_t *c);
    int vp_cond_broadcast(vp_cond_t *c);

    int vp_rwlock_init(vp_rwlock_t **out);
    int vp_rwlock_destroy(vp_rwlock_t *rw);
    int vp_rwlock_rdlock(vp_rwlock_t *rw);
    int vp_rwlock_tryrdlock(vp_rwlock_t *rw);
    int vp_rwlock_timedrdlock(vp_rwlock_t *rw, const struct timespec *abs);
    int vp_rwlock_wrlock(vp_rwlock_t *rw);
    int vp_rwlock_trywrlock(vp_rwlock_t *rw);
    int vp_rwlock_timedwrlock(vp_rwlock_t *rw, const struct timespec *abs);
    int vp_rwlock_unlock(vp_rwlock_t *rw);

    int vp_barrier_init(vp_barrier_t **out, unsigned count);
    int vp_barrier_destroy(vp_barrier_t *b);
    int vp_barrier_wait(vp_barrier_t *b);

    int vp_once(vp_once_t *once, void (*init)(void));

    int vp_key_create(vp_key_t *key, void (*dtor)(void *));
    int vp_key_delete(vp_key_t key);
    int vp_setspecific(vp_key_t key, const void *value);
    void *vp_getspecific(vp_key_t key);

    void vp_cleanup_push_impl(vp_cleanup_t *c, void (*routine)(void *), void *arg);
    void vp_cleanup_pop_impl(vp_cleanup_t *c, int execute);

    int vp_setcancelstate(int state, int *old);
    int vp_setcanceltype(int type, int *old);
    int vp_cancel(vp_thread_t t);
    void vp_testcancel(void);
    int vp_sleep_ms(unsigned ms);

    size_t vp_debug_tcb_size(void);
    size_t vp_debug_mutex_size(void);
    size_t vp_debug_live_hal_objects(void);
#ifdef __cplusplus
}
#endif
#endif
