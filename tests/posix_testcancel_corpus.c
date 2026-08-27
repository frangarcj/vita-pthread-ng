#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include "test.h"

enum
{
    STATUS_NONE = 0,
    STATUS_CANCELED = 1,
    STATUS_FINISHED = 2
};
struct scenario
{
    int mode;
    sem_t gate;
    atomic_int status;
};

static void mark_canceled(void *arg)
{
    struct scenario *s = arg;
    atomic_store_explicit(&s->status, STATUS_CANCELED, memory_order_release);
}

static void *worker(void *arg)
{
    struct scenario *s = arg;
    if (s->mode != 3)
        CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL));
    pthread_cleanup_push(mark_canceled, s);
    EXPECT(sem_wait(&s->gate) == 0);
    if (s->mode == 0)
    {
        CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
        pthread_testcancel();
    }
    else if (s->mode == 1)
    {
        pthread_testcancel();
    }
    else if (s->mode == 2)
    {
        CHECK0(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL));
        CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
        pthread_testcancel();
    }
    else
    {
        for (;;)
            pthread_testcancel();
    }
    atomic_store_explicit(&s->status, STATUS_FINISHED, memory_order_release);
    pthread_cleanup_pop(0);
    return NULL;
}

int main(void)
{
    const int expected[4] = {STATUS_CANCELED, STATUS_FINISHED, STATUS_CANCELED, STATUS_CANCELED};
    for (int mode = 0; mode < 4; ++mode)
    {
        struct scenario s = {.mode = mode, .status = ATOMIC_VAR_INIT(STATUS_NONE)};
        EXPECT(sem_init(&s.gate, 0, 0) == 0);
        pthread_t t;
        void *ret = NULL;
        CHECK0(pthread_create(&t, NULL, worker, &s));
        CHECK0(pthread_cancel(t));
        EXPECT(sem_post(&s.gate) == 0);
        CHECK0(pthread_join(t, &ret));
        if (expected[mode] == STATUS_CANCELED)
            EXPECT(ret == PTHREAD_CANCELED);
        else
            EXPECT(ret != PTHREAD_CANCELED);
        EXPECT(atomic_load_explicit(&s.status, memory_order_acquire) == expected[mode]);
        EXPECT(sem_destroy(&s.gate) == 0);
    }
    return 0;
}