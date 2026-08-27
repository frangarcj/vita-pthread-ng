#include <pthread.h>
#include <errno.h>
#include "vpthread/sce_sim.h"
#include "test.h"

static void *worker(void *arg)
{
    return arg;
}

int main(void)
{
    pthread_t t = 0;
    sceSimFailNextThreadCreate(SCE_KERNEL_ERROR_NO_MEMORY);
    EXPECT(pthread_create(&t, NULL, worker, NULL) == EAGAIN);
    CHECK0(pthread_create(&t, NULL, worker, (void *)0x1234));
    void *ret = NULL;
    CHECK0(pthread_join(t, &ret));
    EXPECT(ret == (void *)0x1234);
    return 0;
}