#include <errno.h>
#include <stdatomic.h>
#include <time.h>
#include "vpthread/vpthread.h"
#include "test.h"

static void *lock_and_die(void *p)
{
    vp_mutex_t *m = p;
    int r = vp_mutex_lock(m);
    EXPECT(r == 0);
    return NULL;
}

static void check_variant(int pshared, int protocol)
{
    vp_mutex_t *m;
    CHECK0(vp_mutex_init_ex(&m, VP_MUTEX_NORMAL, VP_MUTEX_ROBUST, protocol, pshared));
    CHECK0(vp_mutex_lock(m));
    CHECK0(vp_mutex_unlock(m));

    vp_thread_t t;
    CHECK0(vp_thread_create(&t, NULL, lock_and_die, m));
    CHECK0(vp_thread_join(t, NULL));
    EXPECT(vp_mutex_lock(m) == EOWNERDEAD);
    CHECK0(vp_mutex_unlock(m));
    EXPECT(vp_mutex_lock(m) == ENOTRECOVERABLE);
    CHECK0(vp_mutex_destroy(m));

    CHECK0(vp_mutex_init_ex(&m, VP_MUTEX_NORMAL, VP_MUTEX_ROBUST, protocol, pshared));
    CHECK0(vp_thread_create(&t, NULL, lock_and_die, m));
    CHECK0(vp_thread_join(t, NULL));
    EXPECT(vp_mutex_lock(m) == EOWNERDEAD);
    CHECK0(vp_mutex_consistent(m));
    CHECK0(vp_mutex_unlock(m));
    CHECK0(vp_mutex_lock(m));
    CHECK0(vp_mutex_unlock(m));
    CHECK0(vp_mutex_destroy(m));
}

static vp_barrier_t *barrier;
static void *detached_owner(void *p)
{
    vp_mutex_t *m = p;
    CHECK0(vp_mutex_lock(m));
    int r = vp_barrier_wait(barrier);
    EXPECT(r == 0 || r == VP_BARRIER_SERIAL_THREAD);
    return NULL;
}

static void check_detached(void)
{
    vp_mutex_t *m;
    CHECK0(vp_mutex_init_ex(&m, VP_MUTEX_NORMAL, VP_MUTEX_ROBUST, VP_PRIO_NONE, 0));
    CHECK0(vp_barrier_init(&barrier, 2));
    vp_thread_t t;
    CHECK0(vp_thread_create(&t, NULL, detached_owner, m));
    CHECK0(vp_thread_detach(t));
    int r = vp_barrier_wait(barrier);
    EXPECT(r == 0 || r == VP_BARRIER_SERIAL_THREAD);
    CHECK0(vp_barrier_destroy(barrier));

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 200000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_nsec -= 1000000000; ts.tv_sec++; }
    EXPECT(vp_mutex_timedlock(m, &ts) == EOWNERDEAD);
    CHECK0(vp_mutex_consistent(m));
    CHECK0(vp_mutex_unlock(m));
    CHECK0(vp_mutex_destroy(m));
}

int main(void)
{
    for (int p = 0; p <= 1; p++) {
        check_variant(p, VP_PRIO_NONE);
        check_variant(p, VP_PRIO_INHERIT);
    }
    check_detached();
    return 0;
}
