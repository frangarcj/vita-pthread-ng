#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "test.h"

static struct timespec deadline_ms(long ms)
{
    struct timespec t;
    EXPECT(clock_gettime(CLOCK_REALTIME, &t) == 0);
    t.tv_nsec += ms * 1000000L;
    t.tv_sec += t.tv_nsec / 1000000000L;
    t.tv_nsec %= 1000000000L;
    return t;
}

int main(void)
{
    pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
    EXPECT(pthread_rwlock_wrlock(&rw) == 0);
    struct timespec d = deadline_ms(5);
    EXPECT(pthread_rwlock_timedrdlock(&rw, &d) == ETIMEDOUT);
    EXPECT(pthread_rwlock_unlock(&rw) == 0);
    EXPECT(pthread_rwlock_rdlock(&rw) == 0);
    d = deadline_ms(5);
    EXPECT(pthread_rwlock_timedwrlock(&rw, &d) == ETIMEDOUT);
    EXPECT(pthread_rwlock_unlock(&rw) == 0);
    d = deadline_ms(50);
    EXPECT(pthread_rwlock_timedwrlock(&rw, &d) == 0);
    EXPECT(pthread_rwlock_unlock(&rw) == 0);
    EXPECT(pthread_rwlock_destroy(&rw) == 0);
    return 0;
}