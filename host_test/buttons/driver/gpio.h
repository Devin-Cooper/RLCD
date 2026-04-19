#ifndef RLCD_HOST_SHIM_DRIVER_GPIO_H
#define RLCD_HOST_SHIM_DRIVER_GPIO_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int gpio_num_t;
#define GPIO_NUM_0   0
#define GPIO_NUM_18 18

typedef enum { GPIO_INTR_DISABLE = 0 } gpio_int_type_t;
typedef enum { GPIO_MODE_INPUT = 1 }    gpio_mode_t;
typedef enum { GPIO_PULLDOWN_DISABLE = 0 } gpio_pulldown_t;
typedef enum { GPIO_PULLUP_ENABLE = 1 }    gpio_pullup_t;

typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

static inline esp_err_t gpio_config(const gpio_config_t* cfg) { (void)cfg; return ESP_OK; }
static inline int       gpio_get_level(gpio_num_t pin) { (void)pin; return 1; }

#ifdef __cplusplus
}
#endif

#endif
