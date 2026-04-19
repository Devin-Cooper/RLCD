#ifndef RLCD_HOST_SHIM_ESP_ERR_H
#define RLCD_HOST_SHIM_ESP_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_err_t;

#define ESP_OK                0
#define ESP_FAIL              -1
#define ESP_ERR_NO_MEM        0x101
#define ESP_ERR_INVALID_ARG   0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT       0x107

#include <stdio.h>
#include <stdlib.h>
#define ESP_ERROR_CHECK(expr)                                              \
    do {                                                                   \
        esp_err_t _err = (expr);                                           \
        if (_err != ESP_OK) {                                              \
            fprintf(stderr, "ESP_ERROR_CHECK failed: %d\n", _err);         \
            abort();                                                       \
        }                                                                  \
    } while (0)

static inline const char* esp_err_to_name(esp_err_t err) {
    switch (err) {
        case ESP_OK: return "ESP_OK";
        case ESP_FAIL: return "ESP_FAIL";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
        default: return "ESP_ERR_UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif
