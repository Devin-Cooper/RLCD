#ifndef RLCD_HOST_SHIM_FREERTOS_TASK_H
#define RLCD_HOST_SHIM_FREERTOS_TASK_H

#include "FreeRTOS.h"

static inline void vTaskDelay(TickType_t t) { (void)t; }

#endif
