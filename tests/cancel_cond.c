#include <stdatomic.h>
#include "vpthread/vpthread.h"
#include "test.h"
static vp_cond_t*c;static vp_mutex_t*m;static atomic_int ready,cleanup_saw_lock;
static void clean(void*p){(void)p;int r=vp_mutex_trylock(m);if(r!=0)atomic_store(&cleanup_saw_lock,1);else vp_mutex_unlock(m);vp_mutex_unlock(m);}
static void*w(void*p){(void)p;vp_cleanup_t cl;CHECK0(vp_mutex_lock(m));vp_cleanup_push_impl(&cl,clean,NULL);atomic_store(&ready,1);CHECK0(vp_cond_wait(c,m));vp_cleanup_pop_impl(&cl,1);return NULL;}
int main(void){CHECK0(vp_cond_init(&c));CHECK0(vp_mutex_init(&m,VP_MUTEX_ERRORCHECK));vp_thread_t t;CHECK0(vp_thread_create(&t,NULL,w,NULL));while(!atomic_load(&ready)){}CHECK0(vp_cancel(t));void*r;CHECK0(vp_thread_join(t,&r));EXPECT(r==VP_CANCELED);EXPECT(atomic_load(&cleanup_saw_lock)==1);CHECK0(vp_mutex_destroy(m));CHECK0(vp_cond_destroy(c));return 0;}
