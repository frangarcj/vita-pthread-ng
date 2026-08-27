#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>
#include "test.h"

static volatile sig_atomic_t got_usr1;
#if defined(__SANITIZE_THREAD__)
static atomic_int signal_spin;
#endif
static pthread_t expected_thread;

static void on_usr1(int sig)
{
    if (sig == SIGUSR1 && pthread_equal(pthread_self(), expected_thread))
        got_usr1 = 1;
}

static void *check_inherited(void *arg)
{
    (void)arg;
    sigset_t set;
    EXPECT(pthread_sigmask(0, NULL, &set) == 0);
    EXPECT(sigismember(&set, SIGUSR2) == 1);
    return NULL;
}

static void *unblock_and_wait(void *arg)
{
    sem_t *ready = arg;
    sigset_t set;
    EXPECT(sigemptyset(&set) == 0);
    EXPECT(sigaddset(&set, SIGUSR1) == 0);
    EXPECT(pthread_sigmask(SIG_UNBLOCK, &set, NULL) == 0);
    expected_thread = pthread_self();
    EXPECT(sem_post(ready) == 0);
    while (!got_usr1)
    {
#if defined(__SANITIZE_THREAD__)
        atomic_fetch_add_explicit(&signal_spin, 1, memory_order_relaxed);
#endif
    }
    return NULL;
}

int main(void)
{
    struct sigaction sa = {0}, oldsa;
    sa.sa_handler = on_usr1;
    EXPECT(sigemptyset(&sa.sa_mask) == 0);
    EXPECT(sigaction(SIGUSR1, &sa, &oldsa) == 0);
    sigset_t usr2, oldmask;
    EXPECT(sigemptyset(&usr2) == 0);
    EXPECT(sigaddset(&usr2, SIGUSR2) == 0);
    EXPECT(pthread_sigmask(SIG_BLOCK, &usr2, &oldmask) == 0);

    pthread_t inherited;
    EXPECT(pthread_create(&inherited, NULL, check_inherited, NULL) == 0);
    EXPECT(pthread_join(inherited, NULL) == 0);
    sem_t ready;
    EXPECT(sem_init(&ready, 0, 0) == 0);
    pthread_t td;
    EXPECT(pthread_create(&td, NULL, unblock_and_wait, &ready) == 0);
    EXPECT(sem_wait(&ready) == 0);
    EXPECT(pthread_kill(td, 0) == 0);
    EXPECT(pthread_kill(td, SIGUSR1) == 0);
    EXPECT(pthread_join(td, NULL) == 0);
    EXPECT(got_usr1 == 1);

    sigset_t empty;
    EXPECT(sigemptyset(&empty) == 0);
    EXPECT(pthread_sigmask(SIG_SETMASK, &oldmask, NULL) == 0);
    EXPECT(pthread_sigmask(SIG_SETMASK + 123, &empty, NULL) == EINVAL);
    EXPECT(pthread_kill(td, 0) == ESRCH);
    EXPECT(sigaction(SIGUSR1, &oldsa, NULL) == 0);
    EXPECT(sem_destroy(&ready) == 0);
    return 0;
}