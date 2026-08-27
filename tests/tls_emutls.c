/* Verifies that _Thread_local storage is genuinely per-thread on top of
 * vpthread.  On the Vita backend _Thread_local compiles to libgcc emutls,
 * which needs the gthread layer to be active (pthread_* symbols linked); if
 * it is not, every "thread-local" variable degrades to one process-global
 * slot and the cross-thread checks below fail. */
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include "vpthread/vpthread.h"
#include "test.h"

static _Thread_local intptr_t tls_word;
static _Thread_local intptr_t tls_arr[8]; /* second object grows the emutls array */
static _Atomic int stage_a, stage_b;

static void *worker_a(void *p)
{
    (void)p;
    tls_word = 0xA0A0;
    tls_arr[7] = 111;
    atomic_store(&stage_a, 1);
    while (!atomic_load(&stage_b))
        vp_sleep_ms(1);
    /* B wrote its own TLS after us; ours must be intact. */
    EXPECT(tls_word == 0xA0A0);
    EXPECT(tls_arr[7] == 111);
    return (void *)tls_word;
}

static void *worker_b(void *p)
{
    (void)p;
    while (!atomic_load(&stage_a))
        vp_sleep_ms(1);
    tls_word = 0xB1B1;
    tls_arr[7] = 222;
    atomic_store(&stage_b, 1);
    EXPECT(tls_word == 0xB1B1);
    EXPECT(tls_arr[7] == 222);
    return (void *)tls_word;
}

int main(void)
{
    tls_word = 0x33;
    tls_arr[7] = 7;
    vp_thread_t a, b;
    CHECK0(vp_thread_create(&a, NULL, worker_a, NULL));
    CHECK0(vp_thread_create(&b, NULL, worker_b, NULL));
    void *ra, *rb;
    CHECK0(vp_thread_join(a, &ra));
    CHECK0(vp_thread_join(b, &rb));
    EXPECT((intptr_t)ra == 0xA0A0);
    EXPECT((intptr_t)rb == 0xB1B1);
    /* Both workers wrote after main; main's TLS must be untouched. */
    EXPECT(tls_word == 0x33);
    EXPECT(tls_arr[7] == 7);
    printf("tls_emutls PASS\n");
}
