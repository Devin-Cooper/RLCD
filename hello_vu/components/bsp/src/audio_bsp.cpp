#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <driver/i2s_std.h>
#include "audio_bsp.h"

static const char *TAG = "AudioBSP";

// I2S pins for ES7210 (microphone ADC)
#define I2S_MCK_PIN   GPIO_NUM_38
#define I2S_BCK_PIN   GPIO_NUM_9
#define I2S_WS_PIN    GPIO_NUM_10
#define I2S_DI_PIN    GPIO_NUM_8   // Data in from ES7210

// ES7210 register definitions for initialization
#define ES7210_RESET_REG      0x00
#define ES7210_CLK_ON_REG     0x01
#define ES7210_MCLK_CTL_REG   0x02
#define ES7210_MST_CLK_REG    0x03
#define ES7210_ADC_OSR_REG    0x07
#define ES7210_MODE_CTL_REG   0x08
#define ES7210_TIME_CTL0_REG  0x09
#define ES7210_TIME_CTL1_REG  0x0A
#define ES7210_SDP_CFG1_REG   0x11
#define ES7210_ADC12_HPF1_REG 0x21
#define ES7210_ADC12_HPF2_REG 0x22
#define ES7210_ADC1_GAIN_REG  0x43
#define ES7210_ADC2_GAIN_REG  0x44
#define ES7210_ADC3_GAIN_REG  0x45
#define ES7210_ADC4_GAIN_REG  0x46
#define ES7210_ADC_PWR_REG    0x4B
#define ES7210_MIC12_PWR_REG  0x4C

static i2s_chan_handle_t rx_handle = NULL;

AudioPort::AudioPort(I2cMasterBus &i2cbus) : i2cbus_(i2cbus) {
}

AudioPort::~AudioPort() {
    if (rx_handle) {
        i2s_channel_disable(rx_handle);
        i2s_del_channel(rx_handle);
    }
}

void AudioPort::es7210_write_reg(uint8_t reg, uint8_t val) {
    i2cbus_.i2c_write_reg(ES7210_ADDR, reg, val);
}

bool AudioPort::init() {
    // Initialize ES7210 ADC
    ESP_LOGI(TAG, "Initializing ES7210...");

    // Software reset
    es7210_write_reg(ES7210_RESET_REG, 0xFF);
    vTaskDelay(pdMS_TO_TICKS(10));
    es7210_write_reg(ES7210_RESET_REG, 0x41);

    // Clock configuration
    es7210_write_reg(ES7210_CLK_ON_REG, 0x3F);
    es7210_write_reg(ES7210_MCLK_CTL_REG, 0xC1);  // MCLK from pin
    es7210_write_reg(ES7210_MST_CLK_REG, 0x00);   // Slave mode
    es7210_write_reg(ES7210_ADC_OSR_REG, 0x20);   // OSR=32

    // I2S configuration - 16-bit, I2S format
    es7210_write_reg(ES7210_SDP_CFG1_REG, 0x00);  // I2S 16-bit

    // ADC configuration
    es7210_write_reg(ES7210_MODE_CTL_REG, 0x10);  // Single speed
    es7210_write_reg(ES7210_TIME_CTL0_REG, 0x30);
    es7210_write_reg(ES7210_TIME_CTL1_REG, 0x00);

    // Enable HPF
    es7210_write_reg(ES7210_ADC12_HPF1_REG, 0x2A);
    es7210_write_reg(ES7210_ADC12_HPF2_REG, 0x0A);

    // Set ADC gain (0dB default)
    es7210_write_reg(ES7210_ADC1_GAIN_REG, 0x10);
    es7210_write_reg(ES7210_ADC2_GAIN_REG, 0x10);
    es7210_write_reg(ES7210_ADC3_GAIN_REG, 0x10);
    es7210_write_reg(ES7210_ADC4_GAIN_REG, 0x10);

    // Power up ADCs and MICs
    es7210_write_reg(ES7210_ADC_PWR_REG, 0xFF);
    es7210_write_reg(ES7210_MIC12_PWR_REG, 0xFF);

    vTaskDelay(pdMS_TO_TICKS(50));

    // Initialize I2S for recording
    ESP_LOGI(TAG, "Initializing I2S...");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;

    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_PIN,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DI_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S std mode: %s", esp_err_to_name(ret));
        return false;
    }

    ret = i2s_channel_enable(rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "Audio initialized successfully");
    return true;
}

void AudioPort::setMicGain(float db) {
    // Convert dB to ES7210 gain register value
    // ES7210 gain range: 0dB to 37.5dB in 0.5dB steps (0x00 to 0x4B)
    if (db < 0.0f) db = 0.0f;
    if (db > 37.5f) db = 37.5f;

    uint8_t gain_val = (uint8_t)(db * 2);
    es7210_write_reg(ES7210_ADC1_GAIN_REG, gain_val);
    es7210_write_reg(ES7210_ADC2_GAIN_REG, gain_val);
}

int AudioPort::readMicData(void *buffer, int len) {
    if (!rx_handle) return -1;

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(rx_handle, buffer, len, &bytes_read, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        return -1;
    }
    return (int)bytes_read;
}
