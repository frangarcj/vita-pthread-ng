#include <stdint.h>
#include "vpthread/vpthread.h"
#include "test.h"
/* Must exceed VP_HANDLE_SLOTS (1024) so the slot table wraps and generation
 * reuse is exercised.  A create+join cycle costs ~73ms under Vita3K, so the
 * emulated build uses a smaller count that still wraps the table. */
#if defined(__vita__)
enum { REUSE_ITERS = 1200 };
#else
enum { REUSE_ITERS = 3000 };
#endif
static void*w(void*p){return p;}
int main(void){vp_pthread_handle_t old=0;for(int i=0;i<REUSE_ITERS;i++){vp_thread_t t;CHECK0(vp_thread_create(&t,NULL,w,(void*)(intptr_t)i));vp_pthread_handle_t h=vp_thread_handle(t);EXPECT(h!=0);if(old)EXPECT(h!=old || (h&1023u)!=(old&1023u));void*r;CHECK0(vp_thread_join(t,&r));EXPECT((intptr_t)r==i);EXPECT(vp_thread_acquire_handle(h)==NULL);old=h;}return 0;}
