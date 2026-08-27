#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "internal.h"

struct key_slot
{
    int used;
    unsigned gen;
    void (*dtor)(void *);
};

static struct key_slot g_keys[VP_KEYS_MAX];
static atomic_flag g_key_guard = ATOMIC_FLAG_INIT;

static void key_lock(void)
{
    while (atomic_flag_test_and_set_explicit(&g_key_guard, memory_order_acquire))
        vp_hal_yield();
}

static void key_unlock(void)
{
    atomic_flag_clear_explicit(&g_key_guard, memory_order_release);
}

static vp_thread_t key_guard_enter_current(void)
{
    vp_thread_t t = vp_thread_self();
    if (t)
        vp_async_guard_enter(t);
    return t;
}

static void key_guard_leave_current(vp_thread_t t)
{
    if (t)
        vp_async_guard_leave(t);
}

static int key_decode(vp_key_t key, unsigned *idx, unsigned *gen)
{
    *idx = key & 0xffu;
    *gen = key >> 8;
    return *idx > 0 && *idx < VP_KEYS_MAX && *gen != 0;
}

int vp_key_create(vp_key_t *key, void (*dtor)(void *))
{
    if (!key)
        return EINVAL;
    vp_thread_t current = key_guard_enter_current();
    key_lock();
    for (unsigned i = 1; i < VP_KEYS_MAX; i++)
    {
        if (!g_keys[i].used)
        {
            g_keys[i].used = 1;
            g_keys[i].gen++;
            if (!g_keys[i].gen)
                g_keys[i].gen = 1;
            g_keys[i].dtor = dtor;
            *key = (g_keys[i].gen << 8) | i;
            key_unlock();
            key_guard_leave_current(current);
            return 0;
        }
    }
    key_unlock();
    key_guard_leave_current(current);
    return EAGAIN;
}

int vp_key_delete(vp_key_t key)
{
    unsigned i, g;
    if (!key_decode(key, &i, &g))
        return EINVAL;
    vp_thread_t current = key_guard_enter_current();
    key_lock();
    if (!g_keys[i].used || g_keys[i].gen != g)
    {
        key_unlock();
        key_guard_leave_current(current);
        return EINVAL;
    }
    g_keys[i].used = 0;
    g_keys[i].dtor = NULL;
    key_unlock();
    key_guard_leave_current(current);
    return 0;
}

int vp_setspecific(vp_key_t key, const void *value)
{
    unsigned i, g;
    if (!key_decode(key, &i, &g))
        return EINVAL;
    vp_thread_t current = key_guard_enter_current();
    key_lock();
    int valid = g_keys[i].used && g_keys[i].gen == g;
    key_unlock();
    key_guard_leave_current(current);
    if (!valid)
        return EINVAL;

    vp_thread_t t = vp_thread_self();
    if (!t)
        return ENOMEM;
    vp_async_guard_enter(t);
    for (struct vp_tsd_entry *e = t->tsd; e; e = e->next)
    {
        if (e->key == key)
        {
            e->value = (void *)value;
            vp_async_guard_leave(t);
            return 0;
        }
    }

    struct vp_tsd_entry *e = malloc(sizeof(*e));
    if (!e)
    {
        vp_async_guard_leave(t);
        return ENOMEM;
    }
    e->key = key;
    e->value = (void *)value;
    e->next = t->tsd;
    t->tsd = e;
    vp_async_guard_leave(t);
    return 0;
}

void *vp_getspecific(vp_key_t key)
{
    vp_thread_t t = vp_thread_self();
    if (!t)
        return NULL;
    for (struct vp_tsd_entry *e = t->tsd; e; e = e->next)
        if (e->key == key)
            return e->value;
    return NULL;
}

void vp_run_tsd_destructors(vp_thread_t t)
{
    if (!t)
        return;
    for (int iter = 0; iter < VP_DTOR_ITERATIONS; iter++)
    {
        int called = 0;
        for (struct vp_tsd_entry *e = t->tsd; e; e = e->next)
        {
            if (!e->value)
                continue;

            unsigned i, g;
            if (!key_decode(e->key, &i, &g))
            {
                e->value = NULL;
                continue;
            }

            key_lock();
            void (*dtor)(void *) =
                (g_keys[i].used && g_keys[i].gen == g) ? g_keys[i].dtor : NULL;
            key_unlock();

            if (dtor)
            {
                void *value = e->value;
                e->value = NULL;
                called = 1;
                dtor(value);
            }
            else
            {
                e->value = NULL;
            }
        }
        if (!called)
            break;
    }
}
