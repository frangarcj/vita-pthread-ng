#ifndef VPTHREAD_WORLD_SEMAPHORE_H
#define VPTHREAD_WORLD_SEMAPHORE_H
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Opaque handle, matching posix_sem_vita.c's struct sem_t_. */
    typedef struct sem_t_ *sem_t;

#define SEM_FAILED ((sem_t *)0)
#define SEM_VALUE_MAX 0x7fffffff

    int sem_init(sem_t *, int, unsigned int);
    int sem_destroy(sem_t *);
    int sem_wait(sem_t *);
    int sem_trywait(sem_t *);
    int sem_timedwait(sem_t *, const struct timespec *);
    int sem_post(sem_t *);
    int sem_post_multiple(sem_t *, int);
    int sem_getvalue(sem_t *, int *);

#ifdef __cplusplus
}
#endif
#endif
