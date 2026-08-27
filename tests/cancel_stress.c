#include <stdatomic.h>
#include "vpthread/vpthread.h"
#include "test.h"
static atomic_int ready;
static void*w(void*p){(void)p;CHECK0(vp_setcanceltype(VP_CANCEL_ASYNCHRONOUS,NULL));atomic_fetch_add(&ready,1);for(;;){}return NULL;}
int main(void){enum{N=16};for(int round=0;round<100;round++){atomic_store(&ready,0);vp_thread_t t[N];for(int i=0;i<N;i++)CHECK0(vp_thread_create(&t[i],NULL,w,NULL));while(atomic_load(&ready)!=N){}for(int i=N-1;i>=0;i--)CHECK0(vp_cancel(t[i]));for(int i=0;i<N;i++){void*r;CHECK0(vp_thread_join(t[i],&r));EXPECT(r==VP_CANCELED);}}return 0;}
