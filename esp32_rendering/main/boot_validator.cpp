#include "boot_validator.hpp"
#include "screens/dashboard_screen.hpp"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char* TAG = "boot_validator";

namespace app {

namespace {

constexpr int64_t kMinElapsedUs      = 5LL  * 1000 * 1000;  // 5 s
constexpr int64_t kTimeoutUs         = 30LL * 1000 * 1000;  // 30 s
constexpr size_t  kMinFreeHeapBytes  = 100 * 1024;
constexpr int64_t kTestBuildMarkAfterUs = 1LL * 1000 * 1000;  // 1 s

bool dashboardInStack(ScreenStack& stack) {
    const int depth = static_cast<int>(stack.depth());
    for (int i = 0; i < depth; ++i) {
        const Screen* s = stack.at(i);
        if (s && dynamic_cast<const DashboardScreen*>(s) != nullptr) {
            return true;
        }
    }
    return false;
}

void markValid() {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ota validated");
    } else {
        // ESP_ERR_NOT_SUPPORTED fires when we're running from a factory
        // partition — that's fine, nothing to mark.
        ESP_LOGW(TAG, "mark-valid returned %d (%s)", (int)err, esp_err_to_name(err));
    }
}

void validatorTask(void* arg) {
    auto* stack = static_cast<ScreenStack*>(arg);
    const int64_t boot_us = esp_timer_get_time();

#if CONFIG_TEST_CONSOLE_ENABLED
    (void)stack;
    while (true) {
        int64_t elapsed = esp_timer_get_time() - boot_us;
        if (elapsed >= kTestBuildMarkAfterUs) {
            ESP_LOGI(TAG, "test build — marking valid unconditionally");
            markValid();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#else
    while (true) {
        int64_t elapsed = esp_timer_get_time() - boot_us;
        if (elapsed >= kTimeoutUs) {
            ESP_LOGW(TAG, "ota NOT validated — bootloader will roll back on next boot");
            break;
        }
        if (elapsed >= kMinElapsedUs
                && esp_get_free_heap_size() >= kMinFreeHeapBytes
                && dashboardInStack(*stack)) {
            markValid();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
#endif

    vTaskDelete(nullptr);
}

} // anonymous namespace

void startBootValidatorTask(ScreenStack& stack) {
    xTaskCreatePinnedToCore(
        validatorTask,
        "boot_validator",
        4096,
        &stack,
        1,           // lowest priority
        nullptr,
        0            // Core 0
    );
}

} // namespace app
