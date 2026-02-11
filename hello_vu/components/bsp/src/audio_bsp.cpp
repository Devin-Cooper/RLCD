#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s_tdm.h>
#include "audio_bsp.h"

static const char *TAG = "AudioBSP";

// I2S pins for ES7210 (microphone ADC) - from Waveshare board_cfg.h
#define I2S_MCK_PIN   GPIO_NUM_16
#define I2S_BCK_PIN   GPIO_NUM_9
#define I2S_WS_PIN    GPIO_NUM_45
#define I2S_DI_PIN    GPIO_NUM_10  // Data in from ES7210

// ES7210 register definitions
#define ES7210_RESET_REG       0x00
#define ES7210_CLK_ON_REG      0x01
#define ES7210_MCLK_CTL_REG    0x02
#define ES7210_MST_CLK_REG     0x03
#define ES7210_LRCK_DIVH_REG   0x04
#define ES7210_LRCK_DIVL_REG   0x05
#define ES7210_POWER_DOWN_REG  0x06
#define ES7210_OSR_REG         0x07
#define ES7210_MODE_CTL_REG    0x08
#define ES7210_TIME_CTL0_REG   0x09
#define ES7210_TIME_CTL1_REG   0x0A
#define ES7210_SDP_CFG1_REG    0x11
#define ES7210_SDP_CFG2_REG    0x12
#define ES7210_ADC_AUTOMUTE_REG 0x13
#define ES7210_ADC34_MUTE_REG  0x14
#define ES7210_ADC12_MUTE_REG  0x15
#define ES7210_ALC_SEL_REG     0x16
#define ES7210_ALC_COM_CFG1_REG 0x17
#define ES7210_ADC12_HPF1_REG  0x21
#define ES7210_ADC12_HPF2_REG  0x22
#define ES7210_ADC34_HPF1_REG  0x23
#define ES7210_ADC34_HPF2_REG  0x24
#define ES7210_ANALOG_REG      0x40
#define ES7210_MIC12_BIAS_REG  0x41
#define ES7210_MIC34_BIAS_REG  0x42
#define ES7210_ADC1_GAIN_REG   0x43
#define ES7210_ADC2_GAIN_REG   0x44
#define ES7210_ADC3_GAIN_REG   0x45
#define ES7210_ADC4_GAIN_REG   0x46
#define ES7210_ADC12_MUTE_GAIN_REG 0x47
#define ES7210_ADC34_MUTE_GAIN_REG 0x48
#define ES7210_ADC_PWR_REG     0x4B
#define ES7210_MIC12_PWR_REG   0x4C
#define ES7210_MIC34_PWR_REG   0x4D

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
    esp_err_t ret = i2cbus_.i2c_write_reg(ES7210_ADDR, reg, val);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ES7210 write reg 0x%02X failed: %s", reg, esp_err_to_name(ret));
    }
}

bool AudioPort::init() {
    ESP_LOGI(TAG, "Initializing ES7210...");

    // Reset ES7210 (from Waveshare example)
    es7210_write_reg(ES7210_RESET_REG, 0xFF);       // Soft reset
    vTaskDelay(pdMS_TO_TICKS(20));
    es7210_write_reg(ES7210_RESET_REG, 0x41);       // Release reset
    es7210_write_reg(ES7210_CLK_ON_REG, 0x3F);      // Clock off during config

    // Timing configuration
    es7210_write_reg(ES7210_TIME_CTL0_REG, 0x30);   // Chip init state period
    es7210_write_reg(ES7210_TIME_CTL1_REG, 0x30);   // Power up state period

    // HPF configuration (from Waveshare)
    es7210_write_reg(0x23, 0x2A);                   // ADC12_HPF2
    es7210_write_reg(0x22, 0x0A);                   // ADC12_HPF1
    es7210_write_reg(0x20, 0x0A);                   // ADC34_HPF2
    es7210_write_reg(0x21, 0x2A);                   // ADC34_HPF1

    // Slave mode
    es7210_write_reg(ES7210_MODE_CTL_REG, 0x00);    // Slave mode, bit0 = 0

    // Analog power configuration
    es7210_write_reg(ES7210_ANALOG_REG, 0x43);      // VDDA=3.3V, VMID 5KΩ
    es7210_write_reg(ES7210_MIC12_BIAS_REG, 0x70);  // MIC1/2 bias 2.87V
    es7210_write_reg(ES7210_MIC34_BIAS_REG, 0x70);  // MIC3/4 bias 2.87V

    // OSR and clock divider
    es7210_write_reg(ES7210_OSR_REG, 0x20);         // OSR
    es7210_write_reg(ES7210_MCLK_CTL_REG, 0xC1);    // Clock divider with DLL

    // TDM mode enable
    es7210_write_reg(ES7210_SDP_CFG2_REG, 0x02);    // TDM mode

    // Power up sequence (from Waveshare es7210_start)
    es7210_write_reg(ES7210_CLK_ON_REG, 0x00);      // Enable all clocks
    es7210_write_reg(ES7210_POWER_DOWN_REG, 0x00);  // Power up
    es7210_write_reg(ES7210_ANALOG_REG, 0x43);      // Analog power

    // MIC power registers (critical - from Waveshare)
    es7210_write_reg(0x47, 0x08);                   // MIC1 power
    es7210_write_reg(0x48, 0x08);                   // MIC2 power
    es7210_write_reg(0x49, 0x08);                   // MIC3 power
    es7210_write_reg(0x4A, 0x08);                   // MIC4 power

    // Set gain (30dB - gain value 10 = 30dB based on enum)
    uint8_t gain = 0x1A;  // Enable + 30dB gain
    es7210_write_reg(ES7210_ADC1_GAIN_REG, gain);
    es7210_write_reg(ES7210_ADC2_GAIN_REG, gain);
    es7210_write_reg(ES7210_ADC3_GAIN_REG, gain);
    es7210_write_reg(ES7210_ADC4_GAIN_REG, gain);

    // MIC12/34 power
    es7210_write_reg(ES7210_ADC_PWR_REG, 0x00);     // ADC power on
    es7210_write_reg(ES7210_MIC12_PWR_REG, 0x00);   // MIC1/2 power
    es7210_write_reg(ES7210_MIC34_PWR_REG, 0x00);   // MIC3/4 power

    // Final reset sequence
    es7210_write_reg(ES7210_RESET_REG, 0x71);
    es7210_write_reg(ES7210_RESET_REG, 0x41);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize I2S in TDM mode
    ESP_LOGI(TAG, "Initializing I2S TDM...");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;

    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return false;
    }

    // TDM config matching Waveshare example
    i2s_tdm_slot_mask_t slot_mask = (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3);
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, slot_mask),
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

    ret = i2s_channel_init_tdm_mode(rx_handle, &tdm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S TDM mode: %s", esp_err_to_name(ret));
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
