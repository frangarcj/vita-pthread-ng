#include <stdatomic.h>
#include "vpthread/vpthread.h"
#include "test.h"
static vp_cond_t*c;static vp_mutex_t*m1,*m2;static atomic_int ready,woke;
static void *w(void*p){vp_mutex_t*m=p;CHECK0(vp_mutex_lock(m));atomic_fetch_add(&ready,1);CHECK0(vp_cond_wait(c,m));atomic_fetch_add(&woke,1);CHECK0(vp_mutex_unlock(m));return NULL;}
int main(void){CHECK0(vp_cond_init(&c));CHECK0(vp_mutex_init(&m1,0));CHECK0(vp_mutex_init(&m2,0));vp_thread_t a,b;CHECK0(vp_thread_create(&a,NULL,w,m1));CHECK0(vp_thread_create(&b,NULL,w,m2));while(atomic_load(&ready)!=2){}
/* Lock/unlock both predicates to prove each waiter has atomically released
 * its mutex from inside cond_wait, which also means it is queued. */
CHECK0(vp_mutex_lock(m1));CHECK0(vp_mutex_unlock(m1));
CHECK0(vp_mutex_lock(m2));CHECK0(vp_mutex_unlock(m2));
CHECK0(vp_cond_broadcast(c));CHECK0(vp_thread_join(a,NULL));CHECK0(vp_thread_join(b,NULL));EXPECT(atomic_load(&woke)==2);CHECK0(vp_mutex_destroy(m1));CHECK0(vp_mutex_destroy(m2));CHECK0(vp_cond_destroy(c));return 0;}
