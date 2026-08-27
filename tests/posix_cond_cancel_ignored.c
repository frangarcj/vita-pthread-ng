#include <pthread.h>
#include <time.h>
#include "test.h"
static pthread_mutex_t mx=PTHREAD_MUTEX_INITIALIZER;static pthread_cond_t cv=PTHREAD_COND_INITIALIZER;static int waiting;
static void cleanup(void*p){(void)p;waiting=0;CHECK0(pthread_cond_signal(&cv));CHECK0(pthread_mutex_unlock(&mx));}
static void *waiter(void*p){(void)p;CHECK0(pthread_mutex_lock(&mx));waiting=1;CHECK0(pthread_cond_signal(&cv));pthread_cleanup_push(cleanup,NULL);while(waiting)CHECK0(pthread_cond_wait(&cv,&mx));pthread_cleanup_pop(1);return NULL;}
int main(void){pthread_t t;void*r=NULL;CHECK0(pthread_mutex_lock(&mx));CHECK0(pthread_create(&t,NULL,waiter,NULL));while(!waiting)CHECK0(pthread_cond_wait(&cv,&mx));CHECK0(pthread_cancel(t));struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);ts.tv_nsec+=30000000;if(ts.tv_nsec>=1000000000){ts.tv_sec++;ts.tv_nsec-=1000000000;}while(waiting&&pthread_cond_timedwait(&cv,&mx,&ts)==0){}waiting=0;CHECK0(pthread_cond_signal(&cv));CHECK0(pthread_mutex_unlock(&mx));CHECK0(pthread_join(t,&r));EXPECT(r==PTHREAD_CANCELED);CHECK0(pthread_cond_destroy(&cv));CHECK0(pthread_mutex_destroy(&mx));return 0;}
