#include <errno.h>
#include <stdlib.h>
#include "internal.h"

int vp_barrier_init(vp_barrier_t **out, unsigned count)
{
    if (!out || !count) return EINVAL;
    vp_barrier_t *b = calloc(1, sizeof(*b));
    if (!b) return ENOMEM;
    b->magic = VP_BARRIER_MAGIC;
    b->threshold = count;
    int r = vp_mutex_init(&b->mutex, VP_MUTEX_NORMAL);
    if (r) { free(b); return r; }
    r = vp_cond_init(&b->cond);
    if (r) { vp_mutex_destroy(b->mutex); free(b); return r; }
    *out = b;
    return 0;
}

int vp_barrier_destroy(vp_barrier_t *b)
{
    if (!b || b->magic != VP_BARRIER_MAGIC) return EINVAL;
    int r = vp_mutex_lock(b->mutex);
    if (r) return r;
    if (b->waiting) {
        vp_mutex_unlock(b->mutex);
        return EBUSY;
    }
    b->destroying = 1;
    while (b->active) {
        r = vp_cond_wait_nocancel(b->cond, b->mutex);
        if (r) {
            b->destroying = 0;
            vp_mutex_unlock(b->mutex);
            return r;
        }
    }
    vp_mutex_unlock(b->mutex);
    vp_cond_destroy(b->cond);
    vp_mutex_destroy(b->mutex);
    b->magic = 0;
    free(b);
    return 0;
}

int vp_barrier_wait(vp_barrier_t *b)
{
    if (!b || b->magic != VP_BARRIER_MAGIC) return EINVAL;
    int r = vp_mutex_lock(b->mutex);
    if (r) return r;
    if (b->destroying) {
        vp_mutex_unlock(b->mutex);
        return EINVAL;
    }

    b->active++;
    unsigned generation = b->generation;
    b->waiting++;
    int serial = 0;
    if (b->waiting == b->threshold) {
        b->waiting = 0;
        b->generation++;
        serial = 1;
        vp_cond_broadcast(b->cond);
    } else {
        while (generation == b->generation) {
            r = vp_cond_wait_nocancel(b->cond, b->mutex);
            if (r) {
                b->active--;
                vp_mutex_unlock(b->mutex);
                return r;
            }
        }
    }

    b->active--;
    if (!b->active && b->destroying) vp_cond_broadcast(b->cond);
    r = vp_mutex_unlock(b->mutex);
    if (r) return r;
    return serial ? VP_BARRIER_SERIAL_THREAD : 0;
}
