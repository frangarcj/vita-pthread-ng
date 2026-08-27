#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include "test.h"
static pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
static atomic_int counter;
static void *worker(void *arg){EXPECT(sizeof(pthread_t)==4);EXPECT(pthread_equal(pthread_self(),pthread_self()));for(int i=0;i<10000;i++){CHECK0(pthread_mutex_lock(&m));atomic_fetch_add(&counter,1);CHECK0(pthread_mutex_unlock(&m));}return arg;}
int main(void){EXPECT(sizeof(pthread_t)==4);pthread_t a,b;void*ra=NULL,*rb=NULL;CHECK0(pthread_create(&a,NULL,worker,(void*)(uintptr_t)11));CHECK0(pthread_create(&b,NULL,worker,(void*)(uintptr_t)22));CHECK0(pthread_join(a,&ra));CHECK0(pthread_join(b,&rb));EXPECT((uintptr_t)ra==11);EXPECT((uintptr_t)rb==22);EXPECT(atomic_load(&counter)==20000);CHECK0(pthread_mutex_destroy(&m));return 0;}
