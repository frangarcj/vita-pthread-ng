#include <pthread.h>
#include <stddef.h>
#include <time.h>

#define CHECK_SIG(name, ret, args) \
    do                             \
    {                              \
        ret(*p) args = name;       \
        (void)p;                   \
    } while (0)

static void inventory(void)
{
    CHECK_SIG(pthread_atfork, int, (void (*)(void), void (*)(void), void (*)(void)));
    CHECK_SIG(pthread_attr_destroy, int, (pthread_attr_t *));
    CHECK_SIG(pthread_attr_getdetachstate, int, (const pthread_attr_t *, int *));
    CHECK_SIG(pthread_attr_getguardsize, int, (const pthread_attr_t *, size_t *));
    CHECK_SIG(pthread_attr_getinheritsched, int, (const pthread_attr_t *, int *));
    CHECK_SIG(pthread_attr_getschedparam, int, (const pthread_attr_t *, struct sched_param *));
    CHECK_SIG(pthread_attr_getschedpolicy, int, (const pthread_attr_t *, int *));
    CHECK_SIG(pthread_attr_getscope, int, (const pthread_attr_t *, int *));
    CHECK_SIG(pthread_attr_getstack, int, (const pthread_attr_t *, void **, size_t *));
    CHECK_SIG(pthread_attr_getstacksize, int, (const pthread_attr_t *, size_t *));
    CHECK_SIG(pthread_attr_init, int, (pthread_attr_t *));
    CHECK_SIG(pthread_attr_setdetachstate, int, (pthread_attr_t *, int));
    CHECK_SIG(pthread_attr_setguardsize, int, (pthread_attr_t *, size_t));
    CHECK_SIG(pthread_attr_setinheritsched, int, (pthread_attr_t *, int));
    CHECK_SIG(pthread_attr_setschedparam, int, (pthread_attr_t *, const struct sched_param *));
    CHECK_SIG(pthread_attr_setschedpolicy, int, (pthread_attr_t *, int));
    CHECK_SIG(pthread_attr_setscope, int, (pthread_attr_t *, int));
    CHECK_SIG(pthread_attr_setstack, int, (pthread_attr_t *, void *, size_t));
    CHECK_SIG(pthread_attr_setstacksize, int, (pthread_attr_t *, size_t));

    CHECK_SIG(pthread_barrier_destroy, int, (pthread_barrier_t *));
    CHECK_SIG(pthread_barrier_init, int, (pthread_barrier_t *, const pthread_barrierattr_t *, unsigned));
    CHECK_SIG(pthread_barrier_wait, int, (pthread_barrier_t *));
    CHECK_SIG(pthread_barrierattr_destroy, int, (pthread_barrierattr_t *));
    CHECK_SIG(pthread_barrierattr_getpshared, int, (const pthread_barrierattr_t *, int *));
    CHECK_SIG(pthread_barrierattr_init, int, (pthread_barrierattr_t *));
    CHECK_SIG(pthread_barrierattr_setpshared, int, (pthread_barrierattr_t *, int));

    CHECK_SIG(pthread_cancel, int, (pthread_t));
    CHECK_SIG(pthread_cond_broadcast, int, (pthread_cond_t *));
    CHECK_SIG(pthread_cond_destroy, int, (pthread_cond_t *));
    CHECK_SIG(pthread_cond_init, int, (pthread_cond_t *, const pthread_condattr_t *));
    CHECK_SIG(pthread_cond_signal, int, (pthread_cond_t *));
    CHECK_SIG(pthread_cond_timedwait, int, (pthread_cond_t *, pthread_mutex_t *, const struct timespec *));
    CHECK_SIG(pthread_cond_wait, int, (pthread_cond_t *, pthread_mutex_t *));
    CHECK_SIG(pthread_condattr_destroy, int, (pthread_condattr_t *));
    CHECK_SIG(pthread_condattr_getclock, int, (const pthread_condattr_t *, clockid_t *));
    CHECK_SIG(pthread_condattr_getpshared, int, (const pthread_condattr_t *, int *));
    CHECK_SIG(pthread_condattr_init, int, (pthread_condattr_t *));
    CHECK_SIG(pthread_condattr_setclock, int, (pthread_condattr_t *, clockid_t));
    CHECK_SIG(pthread_condattr_setpshared, int, (pthread_condattr_t *, int));

    CHECK_SIG(pthread_create, int, (pthread_t *, const pthread_attr_t *, void *(*)(void *), void *));
    CHECK_SIG(pthread_detach, int, (pthread_t));
    CHECK_SIG(pthread_equal, int, (pthread_t, pthread_t));
    CHECK_SIG(pthread_exit, void, (void *));
    CHECK_SIG(pthread_getcpuclockid, int, (pthread_t, clockid_t *));
    CHECK_SIG(pthread_getschedparam, int, (pthread_t, int *, struct sched_param *));
    CHECK_SIG(pthread_getspecific, void *, (pthread_key_t));
    CHECK_SIG(pthread_join, int, (pthread_t, void **));
    CHECK_SIG(pthread_key_create, int, (pthread_key_t *, void (*)(void *)));
    CHECK_SIG(pthread_key_delete, int, (pthread_key_t));
    CHECK_SIG(pthread_kill, int, (pthread_t, int));

    CHECK_SIG(pthread_mutex_consistent, int, (pthread_mutex_t *));
    CHECK_SIG(pthread_mutex_destroy, int, (pthread_mutex_t *));
    CHECK_SIG(pthread_mutex_init, int, (pthread_mutex_t *, const pthread_mutexattr_t *));
    CHECK_SIG(pthread_mutex_lock, int, (pthread_mutex_t *));
    CHECK_SIG(pthread_mutex_timedlock, int, (pthread_mutex_t *, const struct timespec *));
    CHECK_SIG(pthread_mutex_trylock, int, (pthread_mutex_t *));
    CHECK_SIG(pthread_mutex_unlock, int, (pthread_mutex_t *));
    CHECK_SIG(pthread_mutexattr_destroy, int, (pthread_mutexattr_t *));
    CHECK_SIG(pthread_mutexattr_getprioceiling, int, (const pthread_mutexattr_t *, int *));
    CHECK_SIG(pthread_mutexattr_getprotocol, int, (const pthread_mutexattr_t *, int *));
    CHECK_SIG(pthread_mutexattr_getpshared, int, (const pthread_mutexattr_t *, int *));
    CHECK_SIG(pthread_mutexattr_getrobust, int, (const pthread_mutexattr_t *, int *));
    CHECK_SIG(pthread_mutexattr_gettype, int, (const pthread_mutexattr_t *, int *));
    CHECK_SIG(pthread_mutexattr_init, int, (pthread_mutexattr_t *));
    CHECK_SIG(pthread_mutexattr_setprioceiling, int, (pthread_mutexattr_t *, int));
    CHECK_SIG(pthread_mutexattr_setprotocol, int, (pthread_mutexattr_t *, int));
    CHECK_SIG(pthread_mutexattr_setpshared, int, (pthread_mutexattr_t *, int));
    CHECK_SIG(pthread_mutexattr_setrobust, int, (pthread_mutexattr_t *, int));
    CHECK_SIG(pthread_mutexattr_settype, int, (pthread_mutexattr_t *, int));

    CHECK_SIG(pthread_once, int, (pthread_once_t *, void (*)(void)));
    CHECK_SIG(pthread_rwlock_destroy, int, (pthread_rwlock_t *));
    CHECK_SIG(pthread_rwlock_init, int, (pthread_rwlock_t *, const pthread_rwlockattr_t *));
    CHECK_SIG(pthread_rwlock_rdlock, int, (pthread_rwlock_t *));
    CHECK_SIG(pthread_rwlock_timedrdlock, int, (pthread_rwlock_t *, const struct timespec *));
    CHECK_SIG(pthread_rwlock_timedwrlock, int, (pthread_rwlock_t *, const struct timespec *));
    CHECK_SIG(pthread_rwlock_tryrdlock, int, (pthread_rwlock_t *));
    CHECK_SIG(pthread_rwlock_trywrlock, int, (pthread_rwlock_t *));
    CHECK_SIG(pthread_rwlock_unlock, int, (pthread_rwlock_t *));
    CHECK_SIG(pthread_rwlock_wrlock, int, (pthread_rwlock_t *));
    CHECK_SIG(pthread_rwlockattr_destroy, int, (pthread_rwlockattr_t *));
    CHECK_SIG(pthread_rwlockattr_getpshared, int, (const pthread_rwlockattr_t *, int *));
    CHECK_SIG(pthread_rwlockattr_init, int, (pthread_rwlockattr_t *));
    CHECK_SIG(pthread_rwlockattr_setpshared, int, (pthread_rwlockattr_t *, int));

    CHECK_SIG(pthread_self, pthread_t, (void));
    CHECK_SIG(pthread_setcancelstate, int, (int, int *));
    CHECK_SIG(pthread_setcanceltype, int, (int, int *));
    CHECK_SIG(pthread_setschedparam, int, (pthread_t, int, const struct sched_param *));
    CHECK_SIG(pthread_setschedprio, int, (pthread_t, int));
    CHECK_SIG(pthread_setspecific, int, (pthread_key_t, const void *));
    CHECK_SIG(pthread_sigmask, int, (int, const sigset_t *, sigset_t *));
    CHECK_SIG(pthread_spin_destroy, int, (pthread_spinlock_t *));
    CHECK_SIG(pthread_spin_init, int, (pthread_spinlock_t *, int));
    CHECK_SIG(pthread_spin_lock, int, (pthread_spinlock_t *));
    CHECK_SIG(pthread_spin_trylock, int, (pthread_spinlock_t *));
    CHECK_SIG(pthread_spin_unlock, int, (pthread_spinlock_t *));
    CHECK_SIG(pthread_testcancel, void, (void));

    pthread_cond_t c = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
    pthread_once_t once = PTHREAD_ONCE_INIT;
    void *cancelled = PTHREAD_CANCELED;
    (void)c;
    (void)m;
    (void)rw;
    (void)once;
    (void)cancelled;
}

int main(void)
{
    inventory();
    return 0;
}