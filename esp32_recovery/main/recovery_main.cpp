// ESP32-S3-RLCD recovery firmware entry point.
#include "recovery_screen.hpp"
#include "recovery_repl.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "recovery";

extern "C" void app_main() {
    ESP_LOGI(TAG, "recovery boot");
    recovery::startDisplay();
    recovery::startRepl();
}
