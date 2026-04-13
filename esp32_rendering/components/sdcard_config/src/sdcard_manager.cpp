#include "sdcard_manager.hpp"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include <cstring>

static const char* TAG = "sdcard";
static const char* MOUNT_POINT = "/sdcard";

// Card detect pin — active low (low = card inserted)
static constexpr gpio_num_t CD_PIN = GPIO_NUM_17;

namespace sdcard {

SDCardManager::SDCardManager() : mounted_(false), card_(nullptr) {}

SDCardManager::~SDCardManager() {
    if (mounted_) unmount();
}

bool SDCardManager::mount() {
    if (mounted_) return true;

    // Skip CD pin check — just try to mount. If no card, mount fails gracefully.
    // The Waveshare board's CD pin behavior is unclear (GPIO17 reads high with card inserted).
    ESP_LOGI(TAG, "Attempting SD card mount...");

    esp_vfs_fat_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 3;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.flags = SDMMC_HOST_FLAG_1BIT;

    // Waveshare ESP32-S3-RLCD-4.2 pins: CMD=GPIO21, CLK=GPIO38, D0=GPIO39
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = GPIO_NUM_38;
    slot.cmd = GPIO_NUM_21;
    slot.d0  = GPIO_NUM_39;
    slot.width = 1;

    ESP_LOGI(TAG, "Mounting SD card...");
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot,
                                             &mount_config, &card_);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        card_ = nullptr;
        return false;
    }

    mounted_ = true;
    ESP_LOGI(TAG, "SD card mounted at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, card_);
    return true;
}

void SDCardManager::unmount() {
    if (!mounted_) return;
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card_);
    card_ = nullptr;
    mounted_ = false;
    ESP_LOGI(TAG, "SD card unmounted");
}

} // namespace sdcard
