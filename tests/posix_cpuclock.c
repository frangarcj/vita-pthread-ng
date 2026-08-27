#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdatomic.h>
#include "test.h"

static void *spin(void *p)
{
    atomic_int *ready = p;
    atomic_store_explicit(ready, 1, memory_order_release);
    for (;;)
    {
        pthread_testcancel();
    }
    return NULL;
}

int main(void)
{
    atomic_int ready = ATOMIC_VAR_INIT(0);
    pthread_t td;
    EXPECT(pthread_create(&td, NULL, spin, (void *)&ready) == 0);
    while (!atomic_load_explicit(&ready, memory_order_acquire))
    {
    }
    clockid_t clk;
    EXPECT(pthread_getcpuclockid(td, &clk) == 0);
    struct timespec ts;
    EXPECT(clock_gettime(clk, &ts) == 0);
    EXPECT(pthread_cancel(td) == 0);
    void *ret = NULL;
    EXPECT(pthread_join(td, &ret) == 0);
    EXPECT(ret == PTHREAD_CANCELED);
    EXPECT(pthread_getcpuclockid(td, &clk) == ESRCH);
    return 0;
}