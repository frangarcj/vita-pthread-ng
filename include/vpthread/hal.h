#ifndef VPTHREAD_HAL_H
#define VPTHREAD_HAL_H
#if defined(VPTHREAD_HAL_SCE_SIM)
#include "hal_sce_sim.h"
#elif defined(VPTHREAD_HAL_VITA)
#include "hal_vita.h"
#else
#error "Select VPTHREAD_HAL_SCE_SIM or VPTHREAD_HAL_VITA"
#endif
#endif
