#ifndef RLCD_HOST_SHIM_FREERTOS_H
#define RLCD_HOST_SHIM_FREERTOS_H

#include <stdint.h>

typedef uint32_t TickType_t;
typedef int BaseType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdFAIL  0

#define portMAX_DELAY        ((TickType_t)0xFFFFFFFFU)
#define configTICK_RATE_HZ   1000
#define pdMS_TO_TICKS(ms)    ((TickType_t)(ms))
#define portYIELD_FROM_ISR() ((void)0)

#endif
