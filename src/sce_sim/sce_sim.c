#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include "vpthread/sce_sim.h"

#if defined(__SANITIZE_THREAD__)
#define VP_SIM_TSAN 1
#elif defined(__clang__)
#if __has_feature(thread_sanitizer)
#define VP_SIM_TSAN 1
#endif
#endif

#if defined(VP_SIM_TSAN)
extern int vp_sim_host_cancel_cleanup(void *);
extern void vp_sim_host_cancel_release(void *);
#endif

enum obj_kind
{
    OBJ_NONE = 0,
    OBJ_THREAD,
    OBJ_MUTEX,
    OBJ_SEMA
};

struct sim_thread
{
    SceUID uid;
    pthread_t host;
    pthread_mutex_t lock;
    SceKernelThreadEntry entry;
    void *arg_copy;
    SceSize arg_size;
    size_t stack_size;
    int priority;
    int started;
    int finished;
    int joined;
    int delete_on_exit;
    int host_cancelled;
    int status;
    atomic_int start_returned;
    _Atomic(SceSimInjectEntry) inject_entry;
    _Atomic(void *) inject_arg;
    _Atomic(const _Atomic unsigned *) inject_guard;
    _Atomic(_Atomic int *) inject_inflight;
};

struct sim_mutex
{
    pthread_mutex_t m;
};
struct sim_sema
{
    pthread_mutex_t m;
    pthread_cond_t c;
    int count;
    int max_count;
};
struct slot
{
    enum obj_kind kind;
    void *p;
};

#define SIM_MAX_OBJECTS 4096
static struct slot g_slots[SIM_MAX_OBJECTS];
static pthread_mutex_t g_slots_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_size_t g_live_objects;
static _Thread_local SceUID g_current_uid;
static _Thread_local struct sim_thread *g_current_thread;
static pthread_once_t g_signal_once = PTHREAD_ONCE_INIT;
static int g_inject_signal;
static atomic_int g_fail_next_thread_create;

static SceUID alloc_slot(enum obj_kind kind, void *p)
{
    pthread_mutex_lock(&g_slots_lock);
    for (int i = 1; i < SIM_MAX_OBJECTS; i++)
    {
        if (g_slots[i].kind == OBJ_NONE)
        {
            g_slots[i].kind = kind;
            g_slots[i].p = p;
            atomic_fetch_add_explicit(&g_live_objects, 1, memory_order_relaxed);
            pthread_mutex_unlock(&g_slots_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&g_slots_lock);
    return SCE_KERNEL_ERROR_NO_MEMORY;
}

static void *get_obj(SceUID uid, enum obj_kind kind)
{
    if (uid <= 0 || uid >= SIM_MAX_OBJECTS)
        return NULL;
    pthread_mutex_lock(&g_slots_lock);
    void *p = g_slots[uid].kind == kind ? g_slots[uid].p : NULL;
    pthread_mutex_unlock(&g_slots_lock);
    return p;
}

static void clear_slot(SceUID uid, enum obj_kind kind)
{
    pthread_mutex_lock(&g_slots_lock);
    if (uid > 0 && uid < SIM_MAX_OBJECTS && g_slots[uid].kind == kind)
    {
        g_slots[uid].kind = OBJ_NONE;
        g_slots[uid].p = NULL;
        atomic_fetch_sub_explicit(&g_live_objects, 1, memory_order_relaxed);
    }
    pthread_mutex_unlock(&g_slots_lock);
}

static int add_timeout(struct timespec *out, uint32_t usec)
{
    if (clock_gettime(CLOCK_REALTIME, out) != 0)
        return -1;
    uint64_t ns = (uint64_t)out->tv_nsec + (uint64_t)usec * 1000u;
    out->tv_sec += (time_t)(ns / 1000000000u);
    out->tv_nsec = (long)(ns % 1000000000u);
    return 0;
}

static void set_injected_context(ucontext_t *uc, SceSimInjectEntry entry, void *arg)
{
#if defined(__x86_64__)
    uintptr_t sp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    sp = (sp - 512u) & ~(uintptr_t)0xFu;
    sp -= 8u; /* SysV function-entry alignment: RSP % 16 == 8. */
    uc->uc_mcontext.gregs[REG_RSP] = (greg_t)sp;
    uc->uc_mcontext.gregs[REG_RDI] = (greg_t)(uintptr_t)arg;
    uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)entry;
#elif defined(__aarch64__)
    uintptr_t sp = (uintptr_t)uc->uc_mcontext.sp;
    sp = (sp - 512u) & ~(uintptr_t)0xFu;
    uc->uc_mcontext.sp = sp;
    uc->uc_mcontext.regs[0] = (uintptr_t)arg;
    uc->uc_mcontext.regs[30] = 0;
    uc->uc_mcontext.pc = (uintptr_t)entry;
#elif defined(__arm__)
    uintptr_t sp = (uintptr_t)uc->uc_mcontext.arm_sp;
    sp = (sp - 256u) & ~(uintptr_t)7u;
    uintptr_t pc = (uintptr_t)entry;
    uc->uc_mcontext.arm_sp = sp;
    uc->uc_mcontext.arm_r0 = (uintptr_t)arg;
    uc->uc_mcontext.arm_lr = 0;
    uc->uc_mcontext.arm_pc = pc & ~(uintptr_t)1u;
    if (pc & 1u)
        uc->uc_mcontext.arm_cpsr |= (1u << 5);
    else
        uc->uc_mcontext.arm_cpsr &= ~(1u << 5);
#else
#error "sce_sim async context injection needs an architecture implementation"
#endif
}

#if defined(__GNUC__)
__attribute__((no_sanitize_thread))
#endif
static void inject_signal_handler(int sig, siginfo_t *si, void *opaque)
{
    (void)sig;
    (void)si;
    struct sim_thread *t = g_current_thread;
    if (!t)
        return;

    SceSimInjectEntry entry = atomic_load_explicit(&t->inject_entry, memory_order_acquire);
    if (!entry)
        return;

    const _Atomic unsigned *guard = atomic_load_explicit(&t->inject_guard, memory_order_acquire);
    _Atomic int *inflight = atomic_load_explicit(&t->inject_inflight, memory_order_acquire);
    if (guard && atomic_load_explicit(guard, memory_order_acquire) != 0)
    {
        atomic_store_explicit(&t->inject_entry, NULL, memory_order_release);
        atomic_store_explicit(&t->inject_arg, NULL, memory_order_release);
        if (inflight)
            atomic_store_explicit(inflight, 0, memory_order_release);
        return;
    }

    void *arg = atomic_load_explicit(&t->inject_arg, memory_order_acquire);
    atomic_store_explicit(&t->inject_entry, NULL, memory_order_release);
    atomic_store_explicit(&t->inject_arg, NULL, memory_order_release);
    set_injected_context((ucontext_t *)opaque, entry, arg);
}

static void install_signal(void)
{
    g_inject_signal = SIGRTMIN + 3;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = inject_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(g_inject_signal, &sa, NULL) != 0)
        abort();
}

struct start_pack
{
    SceUID uid;
};

static void thread_cleanup(void *opaque)
{
    struct sim_thread *t = opaque;
    pthread_mutex_lock(&t->lock);
    t->finished = 1;
    int delete_now = t->delete_on_exit;
    pthread_mutex_unlock(&t->lock);
    g_current_uid = 0;
    g_current_thread = NULL;

#if defined(VP_SIM_TSAN)
    if (t->host_cancelled)
        vp_sim_host_cancel_release(t->arg_copy);
#endif

    if (delete_now)
    {
        /* The child can reach ExitDelete before pthread_create has returned
         * to StartThread; do not free the object while pthread_create may
         * still be storing t->host. */
        while (!atomic_load_explicit(&t->start_returned, memory_order_acquire))
            sched_yield();
        (void)pthread_detach(pthread_self());
        clear_slot(t->uid, OBJ_THREAD);
        free(t->arg_copy);
        pthread_mutex_destroy(&t->lock);
        free(t);
    }
}

#if defined(VP_SIM_TSAN)
static void sim_host_cancel_cleanup(void *opaque)
{
    int active = vp_sim_host_cancel_cleanup(opaque);
    if (active && g_current_thread)
        g_current_thread->host_cancelled = 1;
}
#endif

static void *thread_main(void *opaque)
{
    struct start_pack *pack = opaque;
    SceUID uid = pack->uid;
    free(pack);
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return NULL;

    while (!atomic_load_explicit(&t->start_returned, memory_order_acquire))
        sched_yield();

    (void)pthread_once(&g_signal_once, install_signal);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, g_inject_signal);
    (void)pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    g_current_uid = uid;
    g_current_thread = t;

    int status;
    pthread_cleanup_push(thread_cleanup, t);
#if defined(VP_SIM_TSAN)
    pthread_cleanup_push(sim_host_cancel_cleanup, t->arg_copy);
    (void)pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
#endif
    status = t->entry(t->arg_size, t->arg_copy);
#if defined(VP_SIM_TSAN)
    pthread_cleanup_pop(0);
#endif
    pthread_mutex_lock(&t->lock);
    t->status = status;
    pthread_mutex_unlock(&t->lock);
    pthread_cleanup_pop(1);
    return NULL;
}

SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry,
                             int priority, SceSize stack_size, SceUInt attr,
                             int affinity, const void *opt)
{
    (void)name;
    (void)attr;
    (void)affinity;
    (void)opt;
    if (!entry)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    int forced_error = atomic_exchange_explicit(&g_fail_next_thread_create, 0, memory_order_acq_rel);
    if (forced_error)
        return forced_error;
    struct sim_thread *t = calloc(1, sizeof(*t));
    if (!t)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    if (pthread_mutex_init(&t->lock, NULL) != 0)
    {
        free(t);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    t->entry = entry;
    t->priority = priority;
    t->stack_size = stack_size;
    atomic_init(&t->start_returned, 0);
    atomic_init(&t->inject_entry, NULL);
    atomic_init(&t->inject_arg, NULL);
    atomic_init(&t->inject_guard, NULL);
    atomic_init(&t->inject_inflight, NULL);
    SceUID uid = alloc_slot(OBJ_THREAD, t);
    if (uid < 0)
    {
        pthread_mutex_destroy(&t->lock);
        free(t);
        return uid;
    }
    t->uid = uid;
    return uid;
}

int sceKernelStartThread(SceUID uid, SceSize argc, const void *argv)
{
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    pthread_mutex_lock(&t->lock);
    if (t->started)
    {
        pthread_mutex_unlock(&t->lock);
        return SCE_KERNEL_ERROR_ALREADY_STARTED;
    }
    if (argc)
    {
        t->arg_copy = malloc(argc);
        if (!t->arg_copy)
        {
            pthread_mutex_unlock(&t->lock);
            return SCE_KERNEL_ERROR_NO_MEMORY;
        }
        memcpy(t->arg_copy, argv, argc);
    }
    t->arg_size = argc;
    t->started = 1;
    pthread_mutex_unlock(&t->lock);

    struct start_pack *pack = malloc(sizeof(*pack));
    if (!pack)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    pack->uid = uid;
    pthread_attr_t a;
    int r = pthread_attr_init(&a);
    if (!r && t->stack_size)
    {
        size_t ss = t->stack_size;
        size_t min_stack = (size_t)PTHREAD_STACK_MIN;
        if (ss < min_stack)
            ss = min_stack;
        r = pthread_attr_setstacksize(&a, ss);
    }
    if (!r)
        r = pthread_create(&t->host, &a, thread_main, pack);
    pthread_attr_destroy(&a);
    if (r)
    {
        free(pack);
        atomic_store_explicit(&t->start_returned, 1, memory_order_release);
        return r == ENOMEM ? SCE_KERNEL_ERROR_NO_MEMORY : SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    atomic_store_explicit(&t->start_returned, 1, memory_order_release);
    return 0;
}

int sceKernelWaitThreadEnd(SceUID uid, int *status, SceUInt *timeout_us)
{
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    if (g_current_uid == uid)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    if (timeout_us)
    {
        /* pthread_timedjoin_np is Linux-specific, which is fine inside the
         * Linux-only SCE simulator. */
        struct timespec ts;
        if (add_timeout(&ts, *timeout_us) != 0)
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        int r = pthread_timedjoin_np(t->host, NULL, &ts);
        if (r == ETIMEDOUT)
            return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
        if (r && r != ESRCH && r != EINVAL)
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        int r = pthread_join(t->host, NULL);
        if (r && r != ESRCH && r != EINVAL)
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&t->lock);
    t->joined = 1;
    if (status)
        *status = t->status;
    pthread_mutex_unlock(&t->lock);
    return 0;
}

int sceKernelDeleteThread(SceUID uid)
{
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    pthread_mutex_lock(&t->lock);
    int can_delete = !t->started || t->finished || t->joined;
    pthread_mutex_unlock(&t->lock);
    if (!can_delete)
        return SCE_KERNEL_ERROR_THREAD_NOT_DORMANT;
    clear_slot(uid, OBJ_THREAD);
    free(t->arg_copy);
    pthread_mutex_destroy(&t->lock);
    free(t);
    return 0;
}

int sceKernelExitThread(int status)
{
    if (g_current_thread)
    {
        pthread_mutex_lock(&g_current_thread->lock);
        g_current_thread->status = status;
        pthread_mutex_unlock(&g_current_thread->lock);
    }
    pthread_exit(NULL);
    __builtin_unreachable();
}

int sceKernelExitDeleteThread(int status)
{
    struct sim_thread *t = g_current_thread;
    SceUID uid = g_current_uid;
    if (t)
    {
        pthread_mutex_lock(&t->lock);
        t->status = status;
        t->delete_on_exit = 1;
        pthread_mutex_unlock(&t->lock);
    }
    /* Do not free the sim object before pthread cleanup has finished using it.
     * The common pthread layer unregisters the public handle first. */
    (void)uid;
    pthread_exit(NULL);
    __builtin_unreachable();
}

SceUID sceKernelGetThreadId(void)
{
    return g_current_uid;
}

int sceKernelChangeThreadPriority(SceUID uid, int priority)
{
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    pthread_mutex_lock(&t->lock);
    t->priority = priority;
    pthread_mutex_unlock(&t->lock);
    return 0;
}

int sceKernelDelayThread(SceUInt usec)
{
    struct timespec ts = {
        (time_t)(usec / 1000000u),
        (long)((usec % 1000000u) * 1000u)};
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
    {
    }
    return 0;
}

SceUID sceKernelCreateMutex(const char *name, SceUInt attr,
                            int init_count, const void *opt)
{
    (void)name;
    (void)attr;
    (void)opt;
    if (init_count != 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    struct sim_mutex *m = calloc(1, sizeof(*m));
    if (!m)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    if (pthread_mutex_init(&m->m, NULL) != 0)
    {
        free(m);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    SceUID uid = alloc_slot(OBJ_MUTEX, m);
    if (uid < 0)
    {
        pthread_mutex_destroy(&m->m);
        free(m);
    }
    return uid;
}

int sceKernelDeleteMutex(SceUID uid)
{
    struct sim_mutex *m = get_obj(uid, OBJ_MUTEX);
    if (!m)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    int r = pthread_mutex_destroy(&m->m);
    if (r)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    clear_slot(uid, OBJ_MUTEX);
    free(m);
    return 0;
}

int sceKernelLockMutex(SceUID uid, int count, SceUInt *timeout_us)
{
    if (count != 1)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    struct sim_mutex *m = get_obj(uid, OBJ_MUTEX);
    if (!m)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    if (!timeout_us)
        return pthread_mutex_lock(&m->m) ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;

    struct timespec ts;
    if (add_timeout(&ts, *timeout_us))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    int r = pthread_mutex_timedlock(&m->m, &ts);
    if (r == ETIMEDOUT)
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    return r ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
}

int sceKernelTryLockMutex(SceUID uid, int count)
{
    if (count != 1)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    struct sim_mutex *m = get_obj(uid, OBJ_MUTEX);
    if (!m)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    int r = pthread_mutex_trylock(&m->m);
    if (r == EBUSY)
        return SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN;
    return r ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
}

int sceKernelUnlockMutex(SceUID uid, int count)
{
    if (count != 1)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    struct sim_mutex *m = get_obj(uid, OBJ_MUTEX);
    if (!m)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    return pthread_mutex_unlock(&m->m) ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
}

SceUID sceKernelCreateSema(const char *name, SceUInt attr,
                           int init_count, int max_count, const void *opt)
{
    (void)name;
    (void)attr;
    (void)opt;
    if (init_count < 0 || max_count <= 0 || init_count > max_count)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    struct sim_sema *sema = calloc(1, sizeof(*sema));
    if (!sema)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    if (pthread_mutex_init(&sema->m, NULL) != 0)
    {
        free(sema);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    if (pthread_cond_init(&sema->c, NULL) != 0)
    {
        pthread_mutex_destroy(&sema->m);
        free(sema);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    sema->count = init_count;
    sema->max_count = max_count;
    SceUID uid = alloc_slot(OBJ_SEMA, sema);
    if (uid < 0)
    {
        pthread_cond_destroy(&sema->c);
        pthread_mutex_destroy(&sema->m);
        free(sema);
    }
    return uid;
}

int sceKernelDeleteSema(SceUID uid)
{
    struct sim_sema *sema = get_obj(uid, OBJ_SEMA);
    if (!sema)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    clear_slot(uid, OBJ_SEMA);
    pthread_cond_destroy(&sema->c);
    pthread_mutex_destroy(&sema->m);
    free(sema);
    return 0;
}

int sceKernelWaitSema(SceUID uid, int count, SceUInt *timeout_us)
{
    if (count != 1)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    struct sim_sema *sema = get_obj(uid, OBJ_SEMA);
    if (!sema)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;

    pthread_mutex_lock(&sema->m);
    if (!timeout_us)
    {
        while (!sema->count)
            pthread_cond_wait(&sema->c, &sema->m);
    }
    else
    {
        struct timespec ts;
        if (add_timeout(&ts, *timeout_us))
        {
            pthread_mutex_unlock(&sema->m);
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        }
        while (!sema->count)
        {
            int r = pthread_cond_timedwait(&sema->c, &sema->m, &ts);
            if (r == ETIMEDOUT)
            {
                pthread_mutex_unlock(&sema->m);
                return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
            }
            if (r)
            {
                pthread_mutex_unlock(&sema->m);
                return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    sema->count--;
    pthread_mutex_unlock(&sema->m);
    return 0;
}

int sceKernelPollSema(SceUID uid, int count)
{
    if (count != 1)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    struct sim_sema *sema = get_obj(uid, OBJ_SEMA);
    if (!sema)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    pthread_mutex_lock(&sema->m);
    if (!sema->count)
    {
        pthread_mutex_unlock(&sema->m);
        return SCE_KERNEL_ERROR_SEMA_ZERO;
    }
    sema->count--;
    pthread_mutex_unlock(&sema->m);
    return 0;
}

int sceKernelSignalSema(SceUID uid, int count)
{
    if (count != 1)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    struct sim_sema *sema = get_obj(uid, OBJ_SEMA);
    if (!sema)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    pthread_mutex_lock(&sema->m);
    if (sema->count == sema->max_count)
    {
        pthread_mutex_unlock(&sema->m);
        /* Binary wake is intentionally idempotent for the simulator. */
        return 0;
    }
    sema->count++;
    pthread_cond_signal(&sema->c);
    pthread_mutex_unlock(&sema->m);
    return 0;
}

int sceSimInjectThreadNoreturn(SceUID uid, SceSimInjectEntry entry, void *arg,
                               const _Atomic unsigned *guard,
                               _Atomic int *inflight)
{
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;

    pthread_mutex_lock(&t->lock);
    int alive = t->started && !t->finished;
    pthread_mutex_unlock(&t->lock);
    if (!alive)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;

#if defined(VP_SIM_TSAN)
    (void)entry;
    (void)arg;
    (void)guard;
    (void)inflight;
    int r = pthread_cancel(t->host);
    if (r)
        return r == ESRCH ? SCE_KERNEL_ERROR_UNKNOWN_UID
                          : SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    return 0;
#else

    (void)pthread_once(&g_signal_once, install_signal);
    atomic_store_explicit(&t->inject_arg, arg, memory_order_release);
    atomic_store_explicit(&t->inject_guard, guard, memory_order_release);
    atomic_store_explicit(&t->inject_inflight, inflight, memory_order_release);
    atomic_store_explicit(&t->inject_entry, entry, memory_order_release);

    int r = pthread_kill(t->host, g_inject_signal);
    if (r)
    {
        atomic_store_explicit(&t->inject_entry, NULL, memory_order_release);
        return r == ESRCH ? SCE_KERNEL_ERROR_UNKNOWN_UID
                          : SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    return 0;
#endif
}

int sceSimThreadKill(SceUID uid, int sig)
{
    struct sim_thread *t = get_obj(uid, OBJ_THREAD);
    if (!t)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    pthread_mutex_lock(&t->lock);
    int alive = t->started && !t->finished;
    pthread_t host = t->host;
    pthread_mutex_unlock(&t->lock);
    if (!alive)
        return SCE_KERNEL_ERROR_UNKNOWN_UID;
    int r = pthread_kill(host, sig);
    if (r == 0)
        return 0;
    return r == ESRCH ? SCE_KERNEL_ERROR_UNKNOWN_UID : SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

int sceSimAtfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    return pthread_atfork(prepare, parent, child) == 0 ? 0 : SCE_KERNEL_ERROR_NO_MEMORY;
}

size_t sceSimLiveObjectCount(void)
{
    return atomic_load_explicit(&g_live_objects, memory_order_relaxed);
}

void sceSimFailNextThreadCreate(int error_code)
{
    atomic_store_explicit(&g_fail_next_thread_create, error_code, memory_order_release);
}
