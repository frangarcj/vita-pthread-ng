#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include "test.h"

int main(void)
{
    pthread_attr_t a;
    EXPECT(pthread_attr_init(&a) == 0);
    int v = -1;
    size_t z = 0;
    struct sched_param sp = {.sched_priority = -1};
    EXPECT(pthread_attr_getdetachstate(&a, &v) == 0 && v == PTHREAD_CREATE_JOINABLE);
    EXPECT(pthread_attr_getschedpolicy(&a, &v) == 0 && v == SCHED_OTHER);
    EXPECT(pthread_attr_getschedparam(&a, &sp) == 0 && sp.sched_priority == 0);
    EXPECT(pthread_attr_getinheritsched(&a, &v) == 0 && v == PTHREAD_INHERIT_SCHED);
    EXPECT(pthread_attr_getscope(&a, &v) == 0 && v == PTHREAD_SCOPE_SYSTEM);
    EXPECT(pthread_attr_getstacksize(&a, &z) == 0 && z >= 81920u);
    EXPECT(pthread_attr_getguardsize(&a, &z) == 0 && z == 8192u);
    EXPECT(pthread_attr_setguardsize(&a, 0) == 0);
    EXPECT(pthread_attr_setguardsize(&a, 8192u) == 0);
    EXPECT(pthread_attr_setguardsize(&a, SIZE_MAX) == EINVAL);
    EXPECT(pthread_attr_setscope(&a, PTHREAD_SCOPE_SYSTEM) == 0);
    EXPECT(pthread_attr_setscope(&a, PTHREAD_SCOPE_PROCESS) == ENOTSUP);
    EXPECT(pthread_attr_setscope(&a, 777) == EINVAL);

    size_t sz = PTHREAD_STACK_MIN;
    void *mem = malloc(sz);
    EXPECT(mem != NULL);
    EXPECT(pthread_attr_setstack(&a, mem, sz) == 0);
    void *got = NULL;
    size_t got_sz = 0;
    EXPECT(pthread_attr_getstack(&a, &got, &got_sz) == 0);
    EXPECT(got == mem && got_sz == sz);
    EXPECT(pthread_attr_setstack(&a, mem, PTHREAD_STACK_MIN - 1u) == EINVAL);
    EXPECT(pthread_attr_setstack(&a, mem, SIZE_MAX / 4u) == 0);
    EXPECT(pthread_attr_setstack(&a, mem, SIZE_MAX / 4u + PTHREAD_STACK_MIN + 1u) == EINVAL);
    free(mem);
    EXPECT(pthread_attr_destroy(&a) == 0);
    return 0;
}