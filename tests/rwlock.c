#include <errno.h>
#include "vpthread/vpthread.h"
#include "test.h"
static vp_rwlock_t *rw;
static void *try_write(void*p){(void)p;EXPECT(vp_rwlock_trywrlock(rw)==EBUSY);return NULL;}
static void *try_read(void*p){(void)p;EXPECT(vp_rwlock_tryrdlock(rw)==EBUSY);return NULL;}
int main(void){CHECK0(vp_rwlock_init(&rw));CHECK0(vp_rwlock_rdlock(rw));vp_thread_t t;CHECK0(vp_thread_create(&t,NULL,try_write,NULL));CHECK0(vp_thread_join(t,NULL));CHECK0(vp_rwlock_unlock(rw));CHECK0(vp_rwlock_wrlock(rw));CHECK0(vp_thread_create(&t,NULL,try_read,NULL));CHECK0(vp_thread_join(t,NULL));CHECK0(vp_rwlock_unlock(rw));CHECK0(vp_rwlock_destroy(rw));return 0;}
