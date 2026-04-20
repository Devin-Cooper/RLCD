// ESP32-S3-RLCD recovery firmware — T4 stub.
// Actual behavior (display + REPL) lands in T5 and T6.
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "recovery";

extern "C" void app_main() {
    ESP_LOGI(TAG, "recovery boot");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
