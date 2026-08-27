#include <pthread.h>
#include <stdatomic.h>
#include "test.h"
static atomic_int cleaned,destroyed;static pthread_key_t key;static void cl(void*p){(void)p;atomic_store(&cleaned,1);}static void dt(void*p){(void)p;atomic_store(&destroyed,1);}static void *w(void*p){(void)p;CHECK0(pthread_setspecific(key,(void*)1));pthread_cleanup_push(cl,NULL);pthread_exit((void*)0x55);pthread_cleanup_pop(0);return NULL;}
int main(void){CHECK0(pthread_key_create(&key,dt));pthread_t t;void*r=NULL;CHECK0(pthread_create(&t,NULL,w,NULL));CHECK0(pthread_join(t,&r));EXPECT(r==(void*)0x55);EXPECT(atomic_load(&cleaned)==1);EXPECT(atomic_load(&destroyed)==1);CHECK0(pthread_key_delete(key));return 0;}
