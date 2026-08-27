#include "vpthread/abi/pthread.h"
#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <time.h>
#include "internal.h"

/* Linux-only adapters used by the conformance lab.  On Vita these become
 * tiny cancellation hooks in the libc wrappers rather than replacement
 * semaphore implementations. */
int vp_posix_sem_wait(sem_t *s)
{
    if (!s)
        return -1;
    for (;;)
    {
        vp_testcancel();
        if (sem_trywait(s) == 0)
        {
            vp_testcancel();
            return 0;
        }
        if (errno != EAGAIN && errno != EINTR)
            return -1;
        vp_thread_t t = vp_thread_self();
        vp_wait_enter(t, VP_WAIT_SLEEP, NULL);
        (void)vp_hal_sleep_us(1000);
        vp_wait_leave(t);
        vp_testcancel();
    }
}

int vp_posix_sem_timedwait(sem_t *s, const struct timespec *abs)
{
    if (!s || !abs)
    {
        errno = EINVAL;
        return -1;
    }
    for (;;)
    {
        vp_testcancel();
        if (sem_trywait(s) == 0)
        {
            vp_testcancel();
            return 0;
        }
        if (errno != EAGAIN && errno != EINTR)
            return -1;
        int expired = 0;
        uint32_t us = vp_abs_to_us(abs, &expired);
        if (expired)
        {
            errno = ETIMEDOUT;
            return -1;
        }
        uint32_t slice = us > 1000u ? 1000u : us;
        vp_thread_t t = vp_thread_self();
        vp_wait_enter(t, VP_WAIT_SLEEP, NULL);
        (void)vp_hal_sleep_us(slice);
        vp_wait_leave(t);
        vp_testcancel();
    }
}

unsigned int vp_posix_sleep(unsigned int seconds)
{
    uint64_t total_ms = (uint64_t)seconds * 1000u;
    unsigned duration = total_ms > UINT32_MAX ? UINT32_MAX : (unsigned)total_ms;
    return vp_sleep_ms(duration) == 0 ? 0u : seconds;
}
