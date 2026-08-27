#include <pthread.h>
#include <stdatomic.h>
#include "test.h"
static pthread_cond_t c=PTHREAD_COND_INITIALIZER;static pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;static atomic_int ready;static int go;
static void *w(void*p){(void)p;CHECK0(pthread_mutex_lock(&m));atomic_store(&ready,1);while(!go)CHECK0(pthread_cond_wait(&c,&m));CHECK0(pthread_mutex_unlock(&m));return NULL;}
int main(void){pthread_t t;CHECK0(pthread_create(&t,NULL,w,NULL));while(!atomic_load(&ready)){}CHECK0(pthread_mutex_lock(&m));go=1;CHECK0(pthread_cond_signal(&c));CHECK0(pthread_mutex_unlock(&m));CHECK0(pthread_join(t,NULL));CHECK0(pthread_cond_destroy(&c));CHECK0(pthread_mutex_destroy(&m));return 0;}
