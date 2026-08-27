#include <pthread.h>
#include <errno.h>
#include "test.h"
static void *die_locked(void*p){CHECK0(pthread_mutex_lock((pthread_mutex_t*)p));return NULL;}
int main(void){for(int pi=0;pi<2;pi++){pthread_mutexattr_t a;pthread_mutex_t m;pthread_t t;CHECK0(pthread_mutexattr_init(&a));CHECK0(pthread_mutexattr_setrobust(&a,PTHREAD_MUTEX_ROBUST));CHECK0(pthread_mutexattr_setpshared(&a,PTHREAD_PROCESS_SHARED));if(pi)CHECK0(pthread_mutexattr_setprotocol(&a,PTHREAD_PRIO_INHERIT));CHECK0(pthread_mutex_init(&m,&a));CHECK0(pthread_create(&t,NULL,die_locked,&m));CHECK0(pthread_join(t,NULL));EXPECT(pthread_mutex_lock(&m)==EOWNERDEAD);CHECK0(pthread_mutex_consistent(&m));CHECK0(pthread_mutex_unlock(&m));CHECK0(pthread_mutex_lock(&m));CHECK0(pthread_mutex_unlock(&m));CHECK0(pthread_mutex_destroy(&m));CHECK0(pthread_mutexattr_destroy(&a));}return 0;}
