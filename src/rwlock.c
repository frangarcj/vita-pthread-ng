#include <errno.h>
#include <stdlib.h>
#include "internal.h"

int vp_rwlock_init(vp_rwlock_t **out)
{
    if (!out)
        return EINVAL;
    vp_rwlock_t *rw = calloc(1, sizeof(*rw));
    if (!rw)
        return ENOMEM;
    rw->magic = VP_RWLOCK_MAGIC;
    int r = vp_mutex_init(&rw->mutex, VP_MUTEX_NORMAL);
    if (r)
    {
        free(rw);
        return r;
    }
    r = vp_cond_init(&rw->cond);
    if (r)
    {
        vp_mutex_destroy(rw->mutex);
        free(rw);
        return r;
    }
    *out = rw;
    return 0;
}

int vp_rwlock_destroy(vp_rwlock_t *rw)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    if (rw->readers || rw->writer || rw->waiting_writers)
    {
        vp_mutex_unlock(rw->mutex);
        return EBUSY;
    }
    vp_mutex_unlock(rw->mutex);
    vp_cond_destroy(rw->cond);
    vp_mutex_destroy(rw->mutex);
    rw->magic = 0;
    free(rw);
    return 0;
}

int vp_rwlock_rdlock(vp_rwlock_t *rw)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    while (rw->writer || rw->waiting_writers)
    {
        r = vp_cond_wait_nocancel(rw->cond, rw->mutex);
        if (r)
        {
            vp_mutex_unlock(rw->mutex);
            return r;
        }
    }
    rw->readers++;
    return vp_mutex_unlock(rw->mutex);
}

int vp_rwlock_tryrdlock(vp_rwlock_t *rw)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    if (rw->writer || rw->waiting_writers)
    {
        vp_mutex_unlock(rw->mutex);
        return EBUSY;
    }
    rw->readers++;
    return vp_mutex_unlock(rw->mutex);
}

int vp_rwlock_timedrdlock(vp_rwlock_t *rw, const struct timespec *abs)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC || !abs)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    while (rw->writer || rw->waiting_writers)
    {
        r = vp_cond_timedwait_nocancel(rw->cond, rw->mutex, abs);
        if (r)
        {
            vp_mutex_unlock(rw->mutex);
            return r;
        }
    }
    rw->readers++;
    return vp_mutex_unlock(rw->mutex);
}

int vp_rwlock_wrlock(vp_rwlock_t *rw)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    rw->waiting_writers++;
    while (rw->writer || rw->readers)
    {
        r = vp_cond_wait_nocancel(rw->cond, rw->mutex);
        if (r)
        {
            rw->waiting_writers--;
            vp_mutex_unlock(rw->mutex);
            return r;
        }
    }
    rw->waiting_writers--;
    rw->writer = vp_thread_self();
    return vp_mutex_unlock(rw->mutex);
}

int vp_rwlock_timedwrlock(vp_rwlock_t *rw, const struct timespec *abs)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC || !abs)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    rw->waiting_writers++;
    while (rw->writer || rw->readers)
    {
        r = vp_cond_timedwait_nocancel(rw->cond, rw->mutex, abs);
        if (r)
        {
            rw->waiting_writers--;
            vp_cond_broadcast(rw->cond);
            vp_mutex_unlock(rw->mutex);
            return r;
        }
    }
    rw->waiting_writers--;
    rw->writer = vp_thread_self();
    return vp_mutex_unlock(rw->mutex);
}

int vp_rwlock_trywrlock(vp_rwlock_t *rw)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    if (rw->writer || rw->readers)
    {
        vp_mutex_unlock(rw->mutex);
        return EBUSY;
    }
    rw->writer = vp_thread_self();
    return vp_mutex_unlock(rw->mutex);
}

int vp_rwlock_unlock(vp_rwlock_t *rw)
{
    if (!rw || rw->magic != VP_RWLOCK_MAGIC)
        return EINVAL;
    int r = vp_mutex_lock(rw->mutex);
    if (r)
        return r;
    if (rw->writer)
    {
        if (rw->writer != vp_thread_self())
        {
            vp_mutex_unlock(rw->mutex);
            return EPERM;
        }
        rw->writer = NULL;
    }
    else if (rw->readers)
    {
        rw->readers--;
    }
    else
    {
        vp_mutex_unlock(rw->mutex);
        return EPERM;
    }
    vp_cond_broadcast(rw->cond);
    return vp_mutex_unlock(rw->mutex);
}
