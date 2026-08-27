#include <pthread.h>
#include <errno.h>
#include "test.h"
int main(void)
{
    pthread_attr_t a;
    CHECK0(pthread_attr_init(&a));
    int x;
    size_t z;
    void *p = (void *)0x10000, *q = NULL;
    struct sched_param sp = {.sched_priority = 42}, got = {0};
    CHECK0(pthread_attr_getdetachstate(&a, &x));
    EXPECT(x == PTHREAD_CREATE_JOINABLE);
    CHECK0(pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED));
    CHECK0(pthread_attr_getdetachstate(&a, &x));
    EXPECT(x == PTHREAD_CREATE_DETACHED);
    CHECK0(pthread_attr_setguardsize(&a, 4096));
    CHECK0(pthread_attr_getguardsize(&a, &z));
    EXPECT(z == 4096);
    CHECK0(pthread_attr_setstack(&a, p, 32768));
    CHECK0(pthread_attr_getstack(&a, &q, &z));
    EXPECT(q == p && z == 32768);
    CHECK0(pthread_attr_setschedparam(&a, &sp));
    CHECK0(pthread_attr_getschedparam(&a, &got));
    EXPECT(got.sched_priority == 42);
    CHECK0(pthread_attr_setinheritsched(&a, PTHREAD_INHERIT_SCHED));
    CHECK0(pthread_attr_getinheritsched(&a, &x));
    EXPECT(x == PTHREAD_INHERIT_SCHED);
    EXPECT(pthread_attr_setscope(&a, PTHREAD_SCOPE_PROCESS) == ENOTSUP);
    CHECK0(pthread_attr_destroy(&a));
    return 0;
}
