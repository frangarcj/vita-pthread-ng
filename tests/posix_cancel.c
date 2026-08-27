#include <pthread.h>
#include <stdatomic.h>
#include "test.h"
static atomic_int ready,cleaned,destroyed;static pthread_key_t key;
static void cleanup(void*p){(void)p;atomic_fetch_add(&cleaned,1);}static void dtor(void*p){(void)p;atomic_fetch_add(&destroyed,1);}
static void *w(void*p){(void)p;CHECK0(pthread_setspecific(key,(void*)1));pthread_cleanup_push(cleanup,NULL);pthread_cleanup_push(cleanup,NULL);CHECK0(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL));atomic_store(&ready,1);for(;;){}pthread_cleanup_pop(0);pthread_cleanup_pop(0);return NULL;}
int main(void){CHECK0(pthread_key_create(&key,dtor));pthread_t t;void*r=NULL;CHECK0(pthread_create(&t,NULL,w,NULL));while(!atomic_load(&ready)){}CHECK0(pthread_cancel(t));CHECK0(pthread_join(t,&r));EXPECT(r==PTHREAD_CANCELED);EXPECT(atomic_load(&cleaned)==2);EXPECT(atomic_load(&destroyed)==1);CHECK0(pthread_key_delete(key));return 0;}
