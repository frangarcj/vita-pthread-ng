#include <stdatomic.h>
#include "vpthread/vpthread.h"
#include "test.h"
static vp_once_t once=VP_ONCE_INIT;static atomic_int count;
static void init(void){atomic_fetch_add(&count,1);vp_sleep_ms(1);}
static void*w(void*p){(void)p;CHECK0(vp_once(&once,init));return NULL;}
int main(void){vp_thread_t t[16];for(int i=0;i<16;i++)CHECK0(vp_thread_create(&t[i],NULL,w,NULL));for(int i=0;i<16;i++)CHECK0(vp_thread_join(t[i],NULL));EXPECT(atomic_load(&count)==1);return 0;}
