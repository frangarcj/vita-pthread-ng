#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <time.h>
#include "test.h"

static pthread_once_t once = PTHREAD_ONCE_INIT;
static atomic_int once_count;
static sem_t started[3];
static sem_t finished[3];

static void once_init(void)
{
    atomic_fetch_add_explicit(&once_count, 1, memory_order_relaxed);
}

static void *once_worker(void *arg)
{
    intptr_t i = (intptr_t)arg;
    EXPECT(sem_post(&started[i]) == 0);
    CHECK0(pthread_once(&once, once_init));
    EXPECT(sem_post(&finished[i]) == 0);
    return NULL;
}

static void test_once_no_deadlock(void)
{
    pthread_t t[3];
    for (int i = 0; i < 3; ++i)
    {
        EXPECT(sem_init(&started[i], 0, 0) == 0);
        EXPECT(sem_init(&finished[i], 0, 0) == 0);
        CHECK0(pthread_create(&t[i], NULL, once_worker, (void *)(intptr_t)i));
    }
    for (int i = 0; i < 3; ++i)
    {
        EXPECT(sem_wait(&started[i]) == 0);
        struct timespec ts;
        EXPECT(clock_gettime(CLOCK_REALTIME, &ts) == 0);
        ts.tv_nsec += 100000000L;
        if (ts.tv_nsec >= 1000000000L)
        {
            ts.tv_nsec -= 1000000000L;
            ++ts.tv_sec;
        }
        EXPECT(sem_timedwait(&finished[i], &ts) == 0);
        CHECK0(pthread_join(t[i], NULL));
        EXPECT(sem_destroy(&started[i]) == 0);
        EXPECT(sem_destroy(&finished[i]) == 0);
    }
    EXPECT(atomic_load_explicit(&once_count, memory_order_relaxed) == 1);
}

static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static void *try_reader(void *arg)
{
    (void)arg;
    return (void *)(intptr_t)pthread_rwlock_tryrdlock(&rw);
}
static void *try_writer(void *arg)
{
    (void)arg;
    return (void *)(intptr_t)pthread_rwlock_trywrlock(&rw);
}

static void test_rwlock_ebusy(void)
{
    pthread_t t;
    void *ret = NULL;
    CHECK0(pthread_rwlock_rdlock(&rw));
    CHECK0(pthread_create(&t, NULL, try_writer, NULL));
    CHECK0(pthread_join(t, &ret));
    EXPECT((intptr_t)ret == EBUSY);
    CHECK0(pthread_rwlock_unlock(&rw));
    CHECK0(pthread_rwlock_wrlock(&rw));
    CHECK0(pthread_create(&t, NULL, try_reader, NULL));
    CHECK0(pthread_join(t, &ret));
    EXPECT((intptr_t)ret == EBUSY);
    CHECK0(pthread_rwlock_unlock(&rw));
    CHECK0(pthread_rwlock_destroy(&rw));
}

static void test_cond_clock_validation(void)
{
    pthread_condattr_t a;
    clockid_t clk = (clockid_t)-1;
    CHECK0(pthread_condattr_init(&a));
    EXPECT(pthread_condattr_setclock(&a, CLOCK_PROCESS_CPUTIME_ID) == EINVAL);
    EXPECT(pthread_condattr_setclock(&a, CLOCK_THREAD_CPUTIME_ID) == EINVAL);
    CHECK0(pthread_condattr_getclock(&a, &clk));
    EXPECT(clk == CLOCK_REALTIME);
    CHECK0(pthread_condattr_destroy(&a));
}

int main(void)
{
    test_once_no_deadlock();
    test_rwlock_ebusy();
    test_cond_clock_validation();
    return 0;
}