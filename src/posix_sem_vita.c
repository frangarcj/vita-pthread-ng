#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <semaphore.h>
#include "internal.h"

#define VP_SEM_MAGIC 0x5650534du

struct sem_t_
{
    uint32_t magic;
    vp_mutex_t *mutex;
    vp_cond_t *cond;
    unsigned value;
};

static struct sem_t_ *sem_get(sem_t *sem)
{
    if (!sem || !*sem || (*sem)->magic != VP_SEM_MAGIC)
        return NULL;
    return *sem;
}

static void sem_unlock_cleanup(void *arg)
{
    (void)vp_mutex_unlock(arg);
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (!sem)
        return EINVAL;
    if (pshared)
        return ENOTSUP;

    struct sem_t_ *s = calloc(1, sizeof(*s));
    if (!s)
        return ENOMEM;
    int r = vp_mutex_init(&s->mutex, VP_MUTEX_NORMAL);
    if (!r)
        r = vp_cond_init(&s->cond);
    if (r)
    {
        if (s->cond)
            (void)vp_cond_destroy(s->cond);
        if (s->mutex)
            (void)vp_mutex_destroy(s->mutex);
        free(s);
        return r;
    }
    s->magic = VP_SEM_MAGIC;
    s->value = value;
    *sem = s;
    return 0;
}

int sem_destroy(sem_t *sem)
{
    struct sem_t_ *s = sem_get(sem);
    if (!s)
        return EINVAL;
    int r = vp_mutex_lock(s->mutex);
    if (r)
        return r;
    r = vp_mutex_unlock(s->mutex);
    if (r)
        return r;
    r = vp_cond_destroy(s->cond);
    if (r)
        return r;
    r = vp_mutex_destroy(s->mutex);
    if (r)
        return r;
    s->magic = 0;
    free(s);
    *sem = NULL;
    return 0;
}

int sem_trywait(sem_t *sem)
{
    struct sem_t_ *s = sem_get(sem);
    if (!s)
        return EINVAL;
    int r = vp_mutex_lock(s->mutex);
    if (r)
        return r;
    if (!s->value)
    {
        (void)vp_mutex_unlock(s->mutex);
        return EAGAIN;
    }
    s->value--;
    return vp_mutex_unlock(s->mutex);
}

static int sem_wait_impl(sem_t *sem, const struct timespec *abs)
{
    struct sem_t_ *s = sem_get(sem);
    if (!s)
        return EINVAL;
    /* sem_wait/sem_timedwait are POSIX cancellation points even when the
     * semaphore is immediately available; the blocking path below only
     * reaches one (vp_cond_wait) when it has to sleep. */
    vp_testcancel();
    int r = vp_mutex_lock(s->mutex);
    if (r)
        return r;

    vp_cleanup_t cleanup;
    vp_cleanup_push_impl(&cleanup, sem_unlock_cleanup, s->mutex);
    while (!s->value)
    {
        r = abs ? vp_cond_timedwait(s->cond, s->mutex, abs) : vp_cond_wait(s->cond, s->mutex);
        if (r)
            break;
    }
    if (!r)
        s->value--;
    vp_cleanup_pop_impl(&cleanup, r != 0);
    if (r)
        return r;
    return vp_mutex_unlock(s->mutex);
}

int sem_wait(sem_t *sem)
{
    return sem_wait_impl(sem, NULL);
}

int sem_timedwait(sem_t *sem, const struct timespec *abs)
{
    if (!abs)
        return EINVAL;
    return sem_wait_impl(sem, abs);
}

int sem_post(sem_t *sem)
{
    struct sem_t_ *s = sem_get(sem);
    if (!s)
        return EINVAL;
    int r = vp_mutex_lock(s->mutex);
    if (r)
        return r;
    if (s->value == UINT32_MAX)
    {
        (void)vp_mutex_unlock(s->mutex);
        return EOVERFLOW;
    }
    s->value++;
    r = vp_cond_signal(s->cond);
    int unlock_r = vp_mutex_unlock(s->mutex);
    return r ? r : unlock_r;
}

int sem_post_multiple(sem_t *sem, int count)
{
    if (count < 0)
        return EINVAL;
    struct sem_t_ *s = sem_get(sem);
    if (!s)
        return EINVAL;
    int r = vp_mutex_lock(s->mutex);
    if (r)
        return r;
    if ((uint32_t)count > UINT32_MAX - s->value)
    {
        (void)vp_mutex_unlock(s->mutex);
        return EOVERFLOW;
    }
    s->value += (unsigned)count;
    r = vp_cond_broadcast(s->cond);
    int unlock_r = vp_mutex_unlock(s->mutex);
    return r ? r : unlock_r;
}

int sem_getvalue(sem_t *sem, int *value)
{
    struct sem_t_ *s = sem_get(sem);
    if (!s || !value)
        return EINVAL;
    int r = vp_mutex_lock(s->mutex);
    if (r)
        return r;
    *value = s->value > INT32_MAX ? INT32_MAX : (int)s->value;
    return vp_mutex_unlock(s->mutex);
}