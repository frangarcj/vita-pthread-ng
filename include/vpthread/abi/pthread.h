#ifndef VPTHREAD_ABI_PTHREAD_H
#define VPTHREAD_ABI_PTHREAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ATOMIC_VAR_INIT
#define ATOMIC_VAR_INIT(value) (value)
#endif
#if defined(VPTHREAD_HAL_VITA_SCELIBC)
/* SceLibc world: the opaque handle typedefs are fixed by vita-libc's
 * bits/pthread_types.h, not redefined here. */
#include <bits/pthread_types.h>
#else
    /* VitaSDK-compatible public object model.  pthread_t is deliberately a
     * 32-bit generated handle; the synchronization objects remain opaque pointer
     * handles so the historical -1/-2/-3 static initializers keep their ABI. */
    typedef uint32_t pthread_t;
    typedef struct pthread_attr_t_ *pthread_attr_t;
    typedef struct pthread_mutex_t_ *pthread_mutex_t;
    typedef struct pthread_mutexattr_t_ *pthread_mutexattr_t;
    typedef struct pthread_cond_t_ *pthread_cond_t;
    typedef struct pthread_condattr_t_ *pthread_condattr_t;
    typedef struct pthread_key_t_ *pthread_key_t;
    typedef struct pthread_barrier_t_ *pthread_barrier_t;
    typedef struct pthread_barrierattr_t_ *pthread_barrierattr_t;
    typedef struct pthread_rwlock_t_ *pthread_rwlock_t;
    typedef struct pthread_rwlockattr_t_ *pthread_rwlockattr_t;
    typedef struct pthread_spinlock_t_ *pthread_spinlock_t;
#endif

/* glibc's signal/time headers normally pull in the host pthread ABI.  In the
 * Linux SCE simulator we intentionally provide Vita's ABI instead. */
#if defined(__linux__)
#ifndef _BITS_PTHREADTYPES_COMMON_H
#define _BITS_PTHREADTYPES_COMMON_H 1
#endif
#ifndef __have_pthread_attr_t
#define __have_pthread_attr_t 1
#endif
#endif
#if defined(VPTHREAD_HAL_VITA)
#ifndef _SYS__PTHREADTYPES_H_
#define _SYS__PTHREADTYPES_H_ 1
#endif
#endif
#include <sched.h>
#include <time.h>
#include <signal.h>

#ifndef CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_PROCESS_CPUTIME_ID 2
#endif
#ifndef CLOCK_THREAD_CPUTIME_ID
#define CLOCK_THREAD_CPUTIME_ID 3
#endif

#if !defined(VPTHREAD_HAL_VITA_SCELIBC)
    /* SceLibc world: pthread_once_t already came from bits/pthread_types.h
     * above (state + semaphore); pthread_once() below only ever touches
     * state, so the shorter layout is enough. */
    struct pthread_once_t_
    {
        int state;
        void *semaphore;
        int numSemaphoreUsers;
        int done;
    };
    typedef struct pthread_once_t_ pthread_once_t;
#endif

#define PTHREAD_SCOPE_PROCESS 0
#define PTHREAD_SCOPE_SYSTEM 1
#define PTHREAD_INHERIT_SCHED 1
#define PTHREAD_EXPLICIT_SCHED 2
#define PTHREAD_CREATE_DETACHED 0
#define PTHREAD_CREATE_JOINABLE 1
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_PRIO_NONE 0
#define PTHREAD_PRIO_INHERIT 1
#define PTHREAD_PRIO_PROTECT 2

    enum
    {
        PTHREAD_MUTEX_FAST_NP = 0,
        PTHREAD_MUTEX_RECURSIVE_NP = 1,
        PTHREAD_MUTEX_ERRORCHECK_NP = 2,
        PTHREAD_MUTEX_TIMED_NP = PTHREAD_MUTEX_FAST_NP,
        PTHREAD_MUTEX_ADAPTIVE_NP = PTHREAD_MUTEX_FAST_NP,
        PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_FAST_NP,
        PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
        PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
        PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
    };

#define PTHREAD_MUTEX_STALLED 0
#define PTHREAD_MUTEX_ROBUST 1
#define PTHREAD_MUTEX_STALLED_NP PTHREAD_MUTEX_STALLED
#define PTHREAD_MUTEX_ROBUST_NP PTHREAD_MUTEX_ROBUST

/* Preserve pthread-embedded's public numeric values. */
#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_ASYNCHRONOUS 0
#define PTHREAD_CANCEL_DEFERRED 1
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)
#define PTHREAD_CANCELED ((void *)(intptr_t)-1)

#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t)(intptr_t)-1)
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER ((pthread_mutex_t)(intptr_t)-2)
#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER ((pthread_mutex_t)(intptr_t)-3)
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP PTHREAD_ERRORCHECK_MUTEX_INITIALIZER
#define PTHREAD_COND_INITIALIZER ((pthread_cond_t)(intptr_t)-1)
#define PTHREAD_RWLOCK_INITIALIZER ((pthread_rwlock_t)(intptr_t)-1)
#define PTHREAD_SPINLOCK_INITIALIZER ((pthread_spinlock_t)(intptr_t)-1)
#if defined(VPTHREAD_HAL_VITA_SCELIBC)
#define PTHREAD_ONCE_INIT {0, 0}
#else
#define PTHREAD_ONCE_INIT {0, 0, 0, 0}
#endif

#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384u
#endif

/* The Linux conformance build must not export pthread_* symbols because the
 * SCE simulator itself uses the host libpthread.  Tests include this header
 * with VPTHREAD_POSIX_SHIM and transparently call namespaced entry points. */
#ifdef VPTHREAD_POSIX_SHIM
#define pthread_attr_init vp_posix_pthread_attr_init
#define pthread_attr_destroy vp_posix_pthread_attr_destroy
#define pthread_attr_getdetachstate vp_posix_pthread_attr_getdetachstate
#define pthread_attr_setdetachstate vp_posix_pthread_attr_setdetachstate
#define pthread_attr_getstacksize vp_posix_pthread_attr_getstacksize
#define pthread_attr_setstacksize vp_posix_pthread_attr_setstacksize
#define pthread_attr_getstackaddr vp_posix_pthread_attr_getstackaddr
#define pthread_attr_setstackaddr vp_posix_pthread_attr_setstackaddr
#define pthread_attr_getstack vp_posix_pthread_attr_getstack
#define pthread_attr_setstack vp_posix_pthread_attr_setstack
#define pthread_attr_getguardsize vp_posix_pthread_attr_getguardsize
#define pthread_attr_setguardsize vp_posix_pthread_attr_setguardsize
#define pthread_attr_getschedparam vp_posix_pthread_attr_getschedparam
#define pthread_attr_setschedparam vp_posix_pthread_attr_setschedparam
#define pthread_attr_getschedpolicy vp_posix_pthread_attr_getschedpolicy
#define pthread_attr_setschedpolicy vp_posix_pthread_attr_setschedpolicy
#define pthread_attr_getinheritsched vp_posix_pthread_attr_getinheritsched
#define pthread_attr_setinheritsched vp_posix_pthread_attr_setinheritsched
#define pthread_attr_getscope vp_posix_pthread_attr_getscope
#define pthread_attr_setscope vp_posix_pthread_attr_setscope
#define pthread_atfork vp_posix_pthread_atfork
#define pthread_create vp_posix_pthread_create
#define pthread_detach vp_posix_pthread_detach
#define pthread_equal vp_posix_pthread_equal
#define pthread_exit vp_posix_pthread_exit
#define pthread_join vp_posix_pthread_join
#define pthread_self vp_posix_pthread_self
#define pthread_getschedparam vp_posix_pthread_getschedparam
#define pthread_setschedparam vp_posix_pthread_setschedparam
#define pthread_setschedprio vp_posix_pthread_setschedprio
#define pthread_cancel vp_posix_pthread_cancel
#define pthread_setcancelstate vp_posix_pthread_setcancelstate
#define pthread_setcanceltype vp_posix_pthread_setcanceltype
#define pthread_testcancel vp_posix_pthread_testcancel
#define pthread_once vp_posix_pthread_once
#define pthread_key_create vp_posix_pthread_key_create
#define pthread_key_delete vp_posix_pthread_key_delete
#define pthread_setspecific vp_posix_pthread_setspecific
#define pthread_getspecific vp_posix_pthread_getspecific
#define pthread_mutexattr_init vp_posix_pthread_mutexattr_init
#define pthread_mutexattr_destroy vp_posix_pthread_mutexattr_destroy
#define pthread_mutexattr_gettype vp_posix_pthread_mutexattr_gettype
#define pthread_mutexattr_settype vp_posix_pthread_mutexattr_settype
#define pthread_mutexattr_getpshared vp_posix_pthread_mutexattr_getpshared
#define pthread_mutexattr_setpshared vp_posix_pthread_mutexattr_setpshared
#define pthread_mutexattr_getprotocol vp_posix_pthread_mutexattr_getprotocol
#define pthread_mutexattr_setprotocol vp_posix_pthread_mutexattr_setprotocol
#define pthread_mutexattr_getrobust vp_posix_pthread_mutexattr_getrobust
#define pthread_mutexattr_setrobust vp_posix_pthread_mutexattr_setrobust
#define pthread_mutexattr_getrobust_np vp_posix_pthread_mutexattr_getrobust
#define pthread_mutexattr_setrobust_np vp_posix_pthread_mutexattr_setrobust
#define pthread_mutexattr_getprioceiling vp_posix_pthread_mutexattr_getprioceiling
#define pthread_mutexattr_setprioceiling vp_posix_pthread_mutexattr_setprioceiling
#define pthread_mutex_init vp_posix_pthread_mutex_init
#define pthread_mutex_destroy vp_posix_pthread_mutex_destroy
#define pthread_mutex_lock vp_posix_pthread_mutex_lock
#define pthread_mutex_trylock vp_posix_pthread_mutex_trylock
#define pthread_mutex_timedlock vp_posix_pthread_mutex_timedlock
#define pthread_mutex_unlock vp_posix_pthread_mutex_unlock
#define pthread_mutex_consistent vp_posix_pthread_mutex_consistent
#define pthread_mutex_consistent_np vp_posix_pthread_mutex_consistent
#define pthread_condattr_init vp_posix_pthread_condattr_init
#define pthread_condattr_destroy vp_posix_pthread_condattr_destroy
#define pthread_condattr_getpshared vp_posix_pthread_condattr_getpshared
#define pthread_condattr_setpshared vp_posix_pthread_condattr_setpshared
#define pthread_condattr_getclock vp_posix_pthread_condattr_getclock
#define pthread_condattr_setclock vp_posix_pthread_condattr_setclock
#define pthread_cond_init vp_posix_pthread_cond_init
#define pthread_cond_destroy vp_posix_pthread_cond_destroy
#define pthread_cond_wait vp_posix_pthread_cond_wait
#define pthread_cond_timedwait vp_posix_pthread_cond_timedwait
#define pthread_cond_signal vp_posix_pthread_cond_signal
#define pthread_cond_broadcast vp_posix_pthread_cond_broadcast
#define pthread_rwlockattr_init vp_posix_pthread_rwlockattr_init
#define pthread_rwlockattr_destroy vp_posix_pthread_rwlockattr_destroy
#define pthread_rwlockattr_getpshared vp_posix_pthread_rwlockattr_getpshared
#define pthread_rwlockattr_setpshared vp_posix_pthread_rwlockattr_setpshared
#define pthread_rwlock_init vp_posix_pthread_rwlock_init
#define pthread_rwlock_destroy vp_posix_pthread_rwlock_destroy
#define pthread_rwlock_rdlock vp_posix_pthread_rwlock_rdlock
#define pthread_rwlock_tryrdlock vp_posix_pthread_rwlock_tryrdlock
#define pthread_rwlock_timedrdlock vp_posix_pthread_rwlock_timedrdlock
#define pthread_rwlock_wrlock vp_posix_pthread_rwlock_wrlock
#define pthread_rwlock_trywrlock vp_posix_pthread_rwlock_trywrlock
#define pthread_rwlock_timedwrlock vp_posix_pthread_rwlock_timedwrlock
#define pthread_rwlock_unlock vp_posix_pthread_rwlock_unlock
#define pthread_barrierattr_init vp_posix_pthread_barrierattr_init
#define pthread_barrierattr_destroy vp_posix_pthread_barrierattr_destroy
#define pthread_barrierattr_getpshared vp_posix_pthread_barrierattr_getpshared
#define pthread_barrierattr_setpshared vp_posix_pthread_barrierattr_setpshared
#define pthread_barrier_init vp_posix_pthread_barrier_init
#define pthread_barrier_destroy vp_posix_pthread_barrier_destroy
#define pthread_barrier_wait vp_posix_pthread_barrier_wait
#define pthread_spin_init vp_posix_pthread_spin_init
#define pthread_spin_destroy vp_posix_pthread_spin_destroy
#define pthread_spin_lock vp_posix_pthread_spin_lock
#define pthread_spin_trylock vp_posix_pthread_spin_trylock
#define pthread_spin_unlock vp_posix_pthread_spin_unlock
#define pthread_kill vp_posix_pthread_kill
#define pthread_sigmask vp_posix_pthread_sigmask
#define pthread_getcpuclockid vp_posix_pthread_getcpuclockid
#define sem_wait vp_posix_sem_wait
#define sem_timedwait vp_posix_sem_timedwait
#define sleep vp_posix_sleep
#endif

    int pthread_attr_init(pthread_attr_t *);
    int pthread_attr_destroy(pthread_attr_t *);
    int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
    int pthread_attr_setdetachstate(pthread_attr_t *, int);
    int pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
    int pthread_attr_setstacksize(pthread_attr_t *, size_t);
    int pthread_attr_getstackaddr(const pthread_attr_t *, void **);
    int pthread_attr_setstackaddr(pthread_attr_t *, void *);
    int pthread_attr_getstack(const pthread_attr_t *, void **, size_t *);
    int pthread_attr_setstack(pthread_attr_t *, void *, size_t);
    int pthread_attr_getguardsize(const pthread_attr_t *, size_t *);
    int pthread_attr_setguardsize(pthread_attr_t *, size_t);
    int pthread_attr_getschedparam(const pthread_attr_t *, struct sched_param *);
    int pthread_attr_setschedparam(pthread_attr_t *, const struct sched_param *);
    int pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
    int pthread_attr_setschedpolicy(pthread_attr_t *, int);
    int pthread_attr_getinheritsched(const pthread_attr_t *, int *);
    int pthread_attr_setinheritsched(pthread_attr_t *, int);
    int pthread_attr_getscope(const pthread_attr_t *, int *);
    int pthread_attr_setscope(pthread_attr_t *, int);

    int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
    int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
    int pthread_detach(pthread_t);
    int pthread_equal(pthread_t, pthread_t);
    void pthread_exit(void *) __attribute__((noreturn));
    int pthread_join(pthread_t, void **);
    pthread_t pthread_self(void);
    int pthread_getschedparam(pthread_t, int *, struct sched_param *);
    int pthread_setschedparam(pthread_t, int, const struct sched_param *);
    int pthread_setschedprio(pthread_t, int);
    int pthread_cancel(pthread_t);
    int pthread_setcancelstate(int, int *);
    int pthread_setcanceltype(int, int *);
    void pthread_testcancel(void);
    int pthread_once(pthread_once_t *, void (*)(void));

    int pthread_key_create(pthread_key_t *, void (*)(void *));
    int pthread_key_delete(pthread_key_t);
    int pthread_setspecific(pthread_key_t, const void *);
    void *pthread_getspecific(pthread_key_t);

    int pthread_mutexattr_init(pthread_mutexattr_t *);
    int pthread_mutexattr_destroy(pthread_mutexattr_t *);
    int pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);
    int pthread_mutexattr_settype(pthread_mutexattr_t *, int);
    int pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);
    int pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
    int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
    int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
    int pthread_mutexattr_getrobust(const pthread_mutexattr_t *, int *);
    int pthread_mutexattr_setrobust(pthread_mutexattr_t *, int);
    int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *, int *);
    int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);

    int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
    int pthread_mutex_destroy(pthread_mutex_t *);
    int pthread_mutex_lock(pthread_mutex_t *);
    int pthread_mutex_trylock(pthread_mutex_t *);
    int pthread_mutex_timedlock(pthread_mutex_t *, const struct timespec *);
    int pthread_mutex_unlock(pthread_mutex_t *);
    int pthread_mutex_consistent(pthread_mutex_t *);

    int pthread_condattr_init(pthread_condattr_t *);
    int pthread_condattr_destroy(pthread_condattr_t *);
    int pthread_condattr_getpshared(const pthread_condattr_t *, int *);
    int pthread_condattr_setpshared(pthread_condattr_t *, int);
    int pthread_condattr_getclock(const pthread_condattr_t *, clockid_t *);
    int pthread_condattr_setclock(pthread_condattr_t *, clockid_t);
    int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
    int pthread_cond_destroy(pthread_cond_t *);
    int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
    int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
    int pthread_cond_signal(pthread_cond_t *);
    int pthread_cond_broadcast(pthread_cond_t *);

    int pthread_rwlockattr_init(pthread_rwlockattr_t *);
    int pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
    int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *, int *);
    int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
    int pthread_rwlock_init(pthread_rwlock_t *, const pthread_rwlockattr_t *);
    int pthread_rwlock_destroy(pthread_rwlock_t *);
    int pthread_rwlock_rdlock(pthread_rwlock_t *);
    int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
    int pthread_rwlock_timedrdlock(pthread_rwlock_t *, const struct timespec *);
    int pthread_rwlock_wrlock(pthread_rwlock_t *);
    int pthread_rwlock_trywrlock(pthread_rwlock_t *);
    int pthread_rwlock_timedwrlock(pthread_rwlock_t *, const struct timespec *);
    int pthread_rwlock_unlock(pthread_rwlock_t *);

    int pthread_barrierattr_init(pthread_barrierattr_t *);
    int pthread_barrierattr_destroy(pthread_barrierattr_t *);
    int pthread_barrierattr_getpshared(const pthread_barrierattr_t *, int *);
    int pthread_barrierattr_setpshared(pthread_barrierattr_t *, int);
    int pthread_barrier_init(pthread_barrier_t *, const pthread_barrierattr_t *, unsigned);
    int pthread_barrier_destroy(pthread_barrier_t *);
    int pthread_barrier_wait(pthread_barrier_t *);

    int pthread_spin_init(pthread_spinlock_t *, int);
    int pthread_spin_destroy(pthread_spinlock_t *);
    int pthread_spin_lock(pthread_spinlock_t *);
    int pthread_spin_trylock(pthread_spinlock_t *);
    int pthread_spin_unlock(pthread_spinlock_t *);
    int pthread_kill(pthread_t, int);
    int pthread_sigmask(int, const sigset_t *, sigset_t *);
    int pthread_getcpuclockid(pthread_t, clockid_t *);

#ifdef VPTHREAD_POSIX_SHIM
    unsigned int vp_posix_sleep(unsigned int);
#endif

    /* Cleanup scopes intentionally mirror pthread-embedded's source-level ABI. */
    typedef struct vp_posix_cleanup_t
    {
        void (*routine)(void *);
        void *arg;
        int canceltype;
        void *prev;
    } vp_posix_cleanup_t;
    void vp_posix_cleanup_push(vp_posix_cleanup_t *, void (*)(void *), void *);
    void vp_posix_cleanup_pop(vp_posix_cleanup_t *, int);
#define pthread_cleanup_push(routine, arg) \
    {                                      \
        vp_posix_cleanup_t __vp_cleanup;   \
        vp_posix_cleanup_push(&__vp_cleanup, (routine), (arg));
#define pthread_cleanup_pop(execute)                \
    vp_posix_cleanup_pop(&__vp_cleanup, (execute)); \
    }

#ifdef __cplusplus
}
#endif
#endif
