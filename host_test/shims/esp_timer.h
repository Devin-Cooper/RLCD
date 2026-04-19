#ifndef RLCD_HOST_SHIM_ESP_TIMER_H
#define RLCD_HOST_SHIM_ESP_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* esp_timer_handle_t;

typedef enum { ESP_TIMER_TASK, ESP_TIMER_ISR } esp_timer_dispatch_t;

typedef void (*esp_timer_cb_t)(void* arg);

typedef struct {
    esp_timer_cb_t callback;
    void* arg;
    esp_timer_dispatch_t dispatch_method;
    const char* name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

static inline esp_err_t esp_timer_create(const esp_timer_create_args_t* args,
                                         esp_timer_handle_t* out) {
    (void)args;
    static int dummy;
    *out = (esp_timer_handle_t)&dummy;
    return ESP_OK;
}
static inline esp_err_t esp_timer_start_once(esp_timer_handle_t h, uint64_t us) {
    (void)h; (void)us; return ESP_OK;
}
static inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t h, uint64_t us) {
    (void)h; (void)us; return ESP_OK;
}
static inline esp_err_t esp_timer_stop(esp_timer_handle_t h)   { (void)h; return ESP_OK; }
static inline esp_err_t esp_timer_delete(esp_timer_handle_t h) { (void)h; return ESP_OK; }
static inline int64_t   esp_timer_get_time(void)               { return 0; }

#ifdef __cplusplus
}
#endif

#endif
