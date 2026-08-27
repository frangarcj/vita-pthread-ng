#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <stdatomic.h>
#include "test.h"
static sem_t seq,test;static atomic_int stage;
static void *run_sem(void*p){int timed=(int)(intptr_t)p;CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL));while(sem_wait(&seq)&&errno==EINTR){}CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL));atomic_store(&stage,1);if(timed){struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);ts.tv_sec++;(void)sem_timedwait(&test,&ts);}else (void)sem_wait(&test);atomic_store(&stage,2);return NULL;}
static void one(int timed,int initial){CHECK0(sem_init(&test,0,(unsigned)initial));CHECK0(sem_init(&seq,0,0));atomic_store(&stage,0);pthread_t t;void*r=NULL;CHECK0(pthread_create(&t,NULL,run_sem,(void*)(intptr_t)timed));CHECK0(pthread_cancel(t));CHECK0(sem_post(&seq));CHECK0(pthread_join(t,&r));EXPECT(r==PTHREAD_CANCELED);EXPECT(atomic_load(&stage)==1);CHECK0(sem_destroy(&seq));CHECK0(sem_destroy(&test));}
static sem_t child_gate;static void *child(void*p){(void)p;while(sem_wait(&child_gate)&&errno==EINTR){}return NULL;}static pthread_t child_t;static void *joiner(void*p){(void)p;CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL));while(sem_wait(&seq)&&errno==EINTR){}CHECK0(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL));atomic_store(&stage,1);(void)pthread_join(child_t,NULL);atomic_store(&stage,2);return NULL;}
static void join_case(void){CHECK0(sem_init(&seq,0,0));CHECK0(sem_init(&child_gate,0,0));CHECK0(pthread_create(&child_t,NULL,child,NULL));pthread_t j;void*r=NULL;atomic_store(&stage,0);CHECK0(pthread_create(&j,NULL,joiner,NULL));CHECK0(pthread_cancel(j));CHECK0(sem_post(&seq));CHECK0(pthread_join(j,&r));EXPECT(r==PTHREAD_CANCELED);EXPECT(atomic_load(&stage)==1);CHECK0(sem_post(&child_gate));CHECK0(pthread_join(child_t,NULL));CHECK0(sem_destroy(&child_gate));CHECK0(sem_destroy(&seq));}
int main(void){one(0,0);one(0,1);one(1,0);one(1,1);join_case();return 0;}
