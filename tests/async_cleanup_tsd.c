#include <stdatomic.h>
#include <stdint.h>
#include "vpthread/vpthread.h"
#include "test.h"
static atomic_int ready,cleanup_count,dtor_count; static vp_key_t key;
static void cleanup(void*p){atomic_fetch_add(&cleanup_count,(int)(intptr_t)p);}
static void dtor(void*p){if(p)atomic_fetch_add(&dtor_count,1);}
static void *worker(void*p){(void)p;vp_cleanup_t a,b;CHECK0(vp_setspecific(key,(void*)1));vp_cleanup_push_impl(&a,cleanup,(void*)1);vp_cleanup_push_impl(&b,cleanup,(void*)10);CHECK0(vp_setcanceltype(VP_CANCEL_ASYNCHRONOUS,NULL));atomic_store(&ready,1);for(;;){}return NULL;}
int main(void){CHECK0(vp_key_create(&key,dtor));vp_thread_t t;CHECK0(vp_thread_create(&t,NULL,worker,NULL));while(!atomic_load(&ready)){}CHECK0(vp_cancel(t));void*r;CHECK0(vp_thread_join(t,&r));EXPECT(r==VP_CANCELED);EXPECT(atomic_load(&cleanup_count)==11);EXPECT(atomic_load(&dtor_count)==1);CHECK0(vp_key_delete(key));return 0;}
