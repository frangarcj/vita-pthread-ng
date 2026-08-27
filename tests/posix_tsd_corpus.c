#include <pthread.h>
#include "test.h"

static pthread_key_t k1, k2;
static void dtor(void *p) { *(int *)p = 1; }

static void *worker(void *arg)
{
    int *p = arg;
    CHECK0(pthread_setspecific(k1, p));
    CHECK0(pthread_setspecific(k2, p + 1));
    return NULL;
}

int main(void)
{
    int worker_values[2] = {0, 0};
    int main_values[2] = {0, 0};
    CHECK0(pthread_key_create(&k1, dtor));
    CHECK0(pthread_key_create(&k2, dtor));
    CHECK0(pthread_setspecific(k1, main_values));
    CHECK0(pthread_setspecific(k2, main_values + 1));
    pthread_t t;
    CHECK0(pthread_create(&t, NULL, worker, worker_values));
    CHECK0(pthread_join(t, NULL));
    EXPECT(worker_values[0] == 1 && worker_values[1] == 1);
    EXPECT(pthread_getspecific(k1) == main_values);
    EXPECT(pthread_getspecific(k2) == main_values + 1);
    CHECK0(pthread_setspecific(k1, NULL));
    CHECK0(pthread_setspecific(k2, NULL));
    CHECK0(pthread_key_delete(k1));
    CHECK0(pthread_key_delete(k2));
    return 0;
}