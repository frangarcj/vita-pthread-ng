#include <stdatomic.h>
#include "vpthread/vpthread.h"
#include "test.h"
static atomic_int ready;
static void *worker(void*p){(void)p;CHECK0(vp_setcanceltype(VP_CANCEL_ASYNCHRONOUS,NULL));atomic_store_explicit(&ready,1,memory_order_release);for(;;){}return NULL;}
int main(void){for(int i=0;i<100;i++){atomic_store(&ready,0);vp_thread_t t;CHECK0(vp_thread_create(&t,NULL,worker,NULL));while(!atomic_load_explicit(&ready,memory_order_acquire)){}CHECK0(vp_cancel(t));void*r=NULL;CHECK0(vp_thread_join(t,&r));EXPECT(r==VP_CANCELED);}return 0;}
