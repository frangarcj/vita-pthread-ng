#include <pthread.h>
#include <stdatomic.h>
#include "test.h"
static pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;static atomic_int ready;
static void *w(void*p){(void)p;CHECK0(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL));atomic_store(&ready,1);CHECK0(pthread_mutex_lock(&m));return (void*)7;}
int main(void){CHECK0(pthread_mutex_lock(&m));pthread_t t;void*r=NULL;CHECK0(pthread_create(&t,NULL,w,NULL));while(!atomic_load(&ready)){}CHECK0(pthread_cancel(t));CHECK0(pthread_join(t,&r));EXPECT(r==PTHREAD_CANCELED);CHECK0(pthread_mutex_unlock(&m));CHECK0(pthread_mutex_destroy(&m));return 0;}
