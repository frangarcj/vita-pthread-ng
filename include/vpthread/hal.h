#ifndef VPTHREAD_HAL_H
#define VPTHREAD_HAL_H
#if defined(VPTHREAD_HAL_SCE_SIM)
#include "hal_sce_sim.h"
#elif defined(VPTHREAD_HAL_VITA)
#include "hal_vita.h"
#elif defined(VPTHREAD_HAL_VITA_SCELIBC)
#include "hal_vita_scelibc.h"
#else
#error "Select VPTHREAD_HAL_SCE_SIM, VPTHREAD_HAL_VITA or VPTHREAD_HAL_VITA_SCELIBC"
#endif
#endif
