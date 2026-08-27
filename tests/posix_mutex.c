#include <pthread.h>
#include <errno.h>
#include "test.h"
static pthread_mutex_t recur=PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
static pthread_mutex_t err=PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
static void *bad_unlock(void *p){return (void*)(intptr_t)pthread_mutex_unlock((pthread_mutex_t*)p);}
int main(void){CHECK0(pthread_mutex_lock(&recur));CHECK0(pthread_mutex_lock(&recur));CHECK0(pthread_mutex_unlock(&recur));CHECK0(pthread_mutex_unlock(&recur));CHECK0(pthread_mutex_lock(&err));EXPECT(pthread_mutex_lock(&err)==EDEADLK);pthread_t t;void*r=NULL;CHECK0(pthread_create(&t,NULL,bad_unlock,&err));CHECK0(pthread_join(t,&r));EXPECT((intptr_t)r==EPERM);CHECK0(pthread_mutex_unlock(&err));CHECK0(pthread_mutex_destroy(&recur));CHECK0(pthread_mutex_destroy(&err));return 0;}
