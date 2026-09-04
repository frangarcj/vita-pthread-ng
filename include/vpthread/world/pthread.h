#ifndef VPTHREAD_WORLD_PTHREAD_H
#define VPTHREAD_WORLD_PTHREAD_H
/* Installable pthread.h for the vita-scelibc world.  The internal ABI header
 * already declares the full POSIX surface and, under this define, pulls the
 * opaque handle typedefs from vita-libc's bits/pthread_types.h instead of
 * redefining them; this file is just that entry point under its public name. */
#ifndef VPTHREAD_HAL_VITA_SCELIBC
#define VPTHREAD_HAL_VITA_SCELIBC 1
#endif
#define __NEED_time_t
#define __NEED_sigset_t
#define __NEED_clockid_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>
#include <sys/types.h>
#include <time.h>
#include "vpthread/abi/pthread.h"
#endif
