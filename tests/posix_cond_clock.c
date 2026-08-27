#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "test.h"
int main(void){pthread_condattr_t a;pthread_cond_t c;pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;clockid_t clk;CHECK0(pthread_condattr_init(&a));CHECK0(pthread_condattr_getclock(&a,&clk));EXPECT(clk==CLOCK_REALTIME);CHECK0(pthread_condattr_setclock(&a,CLOCK_MONOTONIC));CHECK0(pthread_condattr_getclock(&a,&clk));EXPECT(clk==CLOCK_MONOTONIC);CHECK0(pthread_cond_init(&c,&a));CHECK0(pthread_mutex_lock(&m));struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);ts.tv_nsec+=10000000;if(ts.tv_nsec>=1000000000){ts.tv_sec++;ts.tv_nsec-=1000000000;}EXPECT(pthread_cond_timedwait(&c,&m,&ts)==ETIMEDOUT);CHECK0(pthread_mutex_unlock(&m));CHECK0(pthread_cond_destroy(&c));CHECK0(pthread_condattr_destroy(&a));CHECK0(pthread_mutex_destroy(&m));return 0;}
