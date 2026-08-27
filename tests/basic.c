#include <stdint.h>
#include <stdio.h>
#include "vpthread/vpthread.h"
#include "test.h"
static vp_mutex_t *m; static int counter;
static void *worker(void*p){intptr_t n=(intptr_t)p;EXPECT(vp_thread_self());for(int i=0;i<20000;i++){CHECK0(vp_mutex_lock(m));counter++;CHECK0(vp_mutex_unlock(m));}return(void*)(n*3);}
int main(void){CHECK0(vp_mutex_init(&m,VP_MUTEX_NORMAL));vp_thread_t a,b;CHECK0(vp_thread_create(&a,NULL,worker,(void*)7));CHECK0(vp_thread_create(&b,NULL,worker,(void*)11));void*ra,*rb;CHECK0(vp_thread_join(a,&ra));CHECK0(vp_thread_join(b,&rb));EXPECT((intptr_t)ra==21);EXPECT((intptr_t)rb==33);EXPECT(counter==40000);CHECK0(vp_mutex_destroy(m));printf("basic PASS\n");}
