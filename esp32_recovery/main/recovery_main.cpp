// ESP32-S3-RLCD recovery firmware entry point.
// Display rendering + heartbeat in recovery_screen.cpp; REPL lands in T6.
#include "recovery_screen.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "recovery";

extern "C" void app_main() {
    ESP_LOGI(TAG, "recovery boot");
    recovery::startDisplay();

    // REPL task lands in T6. Until then, idle — the heartbeat task keeps
    // the display pulsing so a user can see the device is alive.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
