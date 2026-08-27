#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "test.h"

enum
{
    PHASES = 10,
    THREADS = 10
};
static pthread_t tids[THREADS];
static pthread_mutex_t locks[PHASES];
static pthread_cond_t clients = PTHREAD_COND_INITIALIZER;
static pthread_cond_t main_cv = PTHREAD_COND_INITIALIZER;
static unsigned inside[PHASES];
static volatile unsigned phase;

static struct timespec deadline(void)
{
    struct timespec t;
    EXPECT(clock_gettime(CLOCK_REALTIME, &t) == 0);
#if defined(__vita__)
    /* Emulated Vita (Vita3K) schedules HLE waits an order of magnitude
     * slower than a host; a 500ms rendezvous bound flakes there. */
    t.tv_sec += 5;
#else
    t.tv_nsec += 500000000L;
    if (t.tv_nsec >= 1000000000L)
    {
        t.tv_nsec -= 1000000000L;
        t.tv_sec++;
    }
#endif
    return t;
}

static void *worker(void *arg)
{
    (void)arg;
    for (unsigned i = 0; i < PHASES; i++)
    {
        EXPECT(pthread_mutex_lock(&locks[i]) == 0);
        inside[i]++;
        if (inside[i] == THREADS)
            EXPECT(pthread_cond_signal(&main_cv) == 0);
        while (i == phase)
        {
            struct timespec d = deadline();
            int r = pthread_cond_timedwait(&clients, &locks[i], &d);
            EXPECT(r == 0);
        }
        EXPECT(pthread_mutex_unlock(&locks[i]) == 0);
    }
    return NULL;
}

int main(void)
{
    EXPECT(pthread_cond_init(&clients, NULL) == 0);
    EXPECT(pthread_cond_init(&main_cv, NULL) == 0);
    for (unsigned i = 0; i < PHASES; i++)
        EXPECT(pthread_mutex_init(&locks[i], NULL) == 0);
    EXPECT(pthread_mutex_lock(&locks[0]) == 0);
    for (unsigned i = 0; i < THREADS; i++)
        EXPECT(pthread_create(&tids[i], NULL, worker, NULL) == 0);
    while (phase < PHASES)
    {
        while (inside[phase] < THREADS)
        {
            struct timespec d = deadline();
            EXPECT(pthread_cond_timedwait(&main_cv, &locks[phase], &d) == 0);
        }
        if (phase + 1 < PHASES)
            EXPECT(pthread_mutex_lock(&locks[phase + 1]) == 0);
        EXPECT(pthread_cond_broadcast(&clients) == 0);
        phase++;
        EXPECT(pthread_mutex_unlock(&locks[phase - 1]) == 0);
    }
    for (unsigned i = 0; i < THREADS; i++)
        EXPECT(pthread_join(tids[i], NULL) == 0);
    for (unsigned i = 0; i < PHASES; i++)
        EXPECT(pthread_mutex_destroy(&locks[i]) == 0);
    EXPECT(pthread_cond_destroy(&main_cv) == 0);
    EXPECT(pthread_cond_destroy(&clients) == 0);
    return 0;
}