#include <errno.h>
#include "vpthread/vpthread.h"
#include "test.h"
int main(void){vp_mutex_t*m;CHECK0(vp_mutex_init(&m,VP_MUTEX_ERRORCHECK));CHECK0(vp_mutex_lock(m));EXPECT(vp_mutex_lock(m)==EDEADLK);CHECK0(vp_mutex_unlock(m));CHECK0(vp_mutex_destroy(m));CHECK0(vp_mutex_init(&m,VP_MUTEX_RECURSIVE));CHECK0(vp_mutex_lock(m));CHECK0(vp_mutex_lock(m));CHECK0(vp_mutex_unlock(m));CHECK0(vp_mutex_unlock(m));CHECK0(vp_mutex_destroy(m));return 0;}
