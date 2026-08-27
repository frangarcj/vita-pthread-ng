#include <errno.h>
#include <stdatomic.h>
#include "vpthread/hal.h"
#include "vpthread/vpthread.h"

int vp_once(vp_once_t *once, void (*init)(void))
{
    if (!once || !init)
        return EINVAL;
    int state = atomic_load_explicit(&once->state, memory_order_acquire);
    if (state == 2)
        return 0;
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&once->state, &expected, 1,
                                                memory_order_acq_rel, memory_order_acquire))
    {
        init();
        atomic_store_explicit(&once->state, 2, memory_order_release);
        return 0;
    }
    while (atomic_load_explicit(&once->state, memory_order_acquire) != 2)
        vp_hal_yield();
    return 0;
}
