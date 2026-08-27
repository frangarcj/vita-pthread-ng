#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include "test.h"
static pthread_once_t once=PTHREAD_ONCE_INIT;static atomic_int n;static void init(void){atomic_fetch_add(&n,1);}static void *ow(void*p){(void)p;CHECK0(pthread_once(&once,init));return NULL;}
static pthread_rwlock_t rw=PTHREAD_RWLOCK_INITIALIZER;static void *tryw(void*p){(void)p;return (void*)(intptr_t)pthread_rwlock_trywrlock(&rw);}
int main(void){pthread_t t[8];for(int i=0;i<8;i++)CHECK0(pthread_create(&t[i],NULL,ow,NULL));for(int i=0;i<8;i++)CHECK0(pthread_join(t[i],NULL));EXPECT(atomic_load(&n)==1);CHECK0(pthread_rwlock_rdlock(&rw));pthread_t x;void*r=NULL;CHECK0(pthread_create(&x,NULL,tryw,NULL));CHECK0(pthread_join(x,&r));EXPECT((intptr_t)r==EBUSY);CHECK0(pthread_rwlock_unlock(&rw));CHECK0(pthread_rwlock_destroy(&rw));pthread_spinlock_t s=PTHREAD_SPINLOCK_INITIALIZER;CHECK0(pthread_spin_lock(&s));EXPECT(pthread_spin_trylock(&s)==EBUSY);CHECK0(pthread_spin_unlock(&s));CHECK0(pthread_spin_destroy(&s));return 0;}
