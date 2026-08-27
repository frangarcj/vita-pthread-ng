#ifndef VPTHREAD_SCE_SIM_H
#define VPTHREAD_SCE_SIM_H
#include <stddef.h>
#include <stdint.h>

typedef int32_t SceUID;
typedef uint32_t SceUInt;
typedef uint32_t SceSize;
typedef int (*SceKernelThreadEntry)(SceSize argc, void *argv);

#define SCE_KERNEL_ERROR_WAIT_TIMEOUT (-0x1001)
#define SCE_KERNEL_ERROR_NO_MEMORY (-0x1002)
#define SCE_KERNEL_ERROR_UNKNOWN_UID (-0x1003)
#define SCE_KERNEL_ERROR_INVALID_ARGUMENT (-0x1004)
#define SCE_KERNEL_ERROR_SEMA_ZERO (-0x1005)
#define SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN (-0x1006)
#define SCE_KERNEL_ERROR_ALREADY_STARTED (-0x1007)
#define SCE_KERNEL_ERROR_THREAD_NOT_DORMANT (-0x1008)

SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry,
                             int priority, SceSize stack_size, SceUInt attr,
                             int affinity, const void *opt);
int sceKernelStartThread(SceUID uid, SceSize argc, const void *argv);
int sceKernelWaitThreadEnd(SceUID uid, int *status, SceUInt *timeout_us);
int sceKernelDeleteThread(SceUID uid);
int sceKernelExitThread(int status) __attribute__((noreturn));
int sceKernelExitDeleteThread(int status) __attribute__((noreturn));
SceUID sceKernelGetThreadId(void);
int sceKernelChangeThreadPriority(SceUID uid, int priority);
int sceKernelDelayThread(SceUInt usec);

SceUID sceKernelCreateMutex(const char *name, SceUInt attr, int init_count, const void *opt);
int sceKernelDeleteMutex(SceUID uid);
int sceKernelLockMutex(SceUID uid, int count, SceUInt *timeout_us);
int sceKernelTryLockMutex(SceUID uid, int count);
int sceKernelUnlockMutex(SceUID uid, int count);

SceUID sceKernelCreateSema(const char *name, SceUInt attr, int init_count, int max_count, const void *opt);
int sceKernelDeleteSema(SceUID uid);
int sceKernelWaitSema(SceUID uid, int count, SceUInt *timeout_us);
int sceKernelPollSema(SceUID uid, int count);
int sceKernelSignalSema(SceUID uid, int count);

/* Linux-only simulation of Vita's suspend/get-context/set-context/resume path.
 * The signal handler only edits ucontext.  The injected function runs after
 * sigreturn in normal thread context and must not return. */
typedef void (*SceSimInjectEntry)(void *arg);
int sceSimInjectThreadNoreturn(SceUID uid, SceSimInjectEntry entry, void *arg,
                               const _Atomic unsigned *guard,
                               _Atomic int *inflight);
int sceSimThreadKill(SceUID uid, int sig);
int sceSimAtfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
size_t sceSimLiveObjectCount(void);
void sceSimFailNextThreadCreate(int error_code);
#endif
