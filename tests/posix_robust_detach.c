#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "test.h"
static pthread_barrier_t b;static void *owner(void*p){CHECK0(pthread_mutex_lock((pthread_mutex_t*)p));int r=pthread_barrier_wait(&b);EXPECT(r==0||r==PTHREAD_BARRIER_SERIAL_THREAD);return NULL;}
int main(void){for(int ps=0;ps<2;ps++){pthread_mutexattr_t a;pthread_mutex_t m;CHECK0(pthread_barrier_init(&b,NULL,2));CHECK0(pthread_mutexattr_init(&a));CHECK0(pthread_mutexattr_setrobust(&a,PTHREAD_MUTEX_ROBUST));if(ps)CHECK0(pthread_mutexattr_setpshared(&a,PTHREAD_PROCESS_SHARED));CHECK0(pthread_mutex_init(&m,&a));pthread_t t;CHECK0(pthread_create(&t,NULL,owner,&m));CHECK0(pthread_detach(t));int r=pthread_barrier_wait(&b);EXPECT(r==0||r==PTHREAD_BARRIER_SERIAL_THREAD);CHECK0(pthread_barrier_destroy(&b));struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);ts.tv_nsec+=100000000;if(ts.tv_nsec>=1000000000){ts.tv_sec++;ts.tv_nsec-=1000000000;}EXPECT(pthread_mutex_timedlock(&m,&ts)==EOWNERDEAD);CHECK0(pthread_mutex_consistent(&m));CHECK0(pthread_mutex_unlock(&m));CHECK0(pthread_mutex_destroy(&m));CHECK0(pthread_mutexattr_destroy(&a));}return 0;}
