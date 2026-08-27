#include <pthread.h>
#include <stdatomic.h>
#include "test.h"
static pthread_once_t once=PTHREAD_ONCE_INIT;static atomic_int entered,completed;
static void bad_init(void){atomic_store(&entered,1);CHECK0(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL));for(;;){}}
static void good_init(void){atomic_fetch_add(&completed,1);}static void *a(void*p){(void)p;(void)pthread_once(&once,bad_init);return NULL;}static void *b(void*p){(void)p;CHECK0(pthread_once(&once,good_init));return NULL;}
int main(void){pthread_t t;void*r=NULL;CHECK0(pthread_create(&t,NULL,a,NULL));while(!atomic_load(&entered)){}CHECK0(pthread_cancel(t));CHECK0(pthread_join(t,&r));EXPECT(r==PTHREAD_CANCELED);CHECK0(pthread_create(&t,NULL,b,NULL));CHECK0(pthread_join(t,NULL));EXPECT(atomic_load(&completed)==1);return 0;}
