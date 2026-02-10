#include <stdio.h>
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>

#include "display_bsp.h"
#include "i2c_bsp.h"
#include "audio_bsp.h"

static const char *TAG = "HelloVU";

// Pin definitions
#define RLCD_MOSI_PIN  12
#define RLCD_SCK_PIN   11
#define RLCD_DC_PIN    5
#define RLCD_CS_PIN    40
#define RLCD_RST_PIN   41
#define I2C_SDA_PIN    13
#define I2C_SCL_PIN    14

// VU meter configuration
#define VU_SEGMENTS     8
#define VU_SEG_WIDTH    30
#define VU_SEG_HEIGHT   28
#define VU_SEG_GAP      4
#define VU_LEFT_X       20
#define VU_RIGHT_X      (LCD_WIDTH - VU_LEFT_X - VU_SEG_WIDTH)
#define VU_TOP_Y        ((LCD_HEIGHT - (VU_SEGMENTS * VU_SEG_HEIGHT + (VU_SEGMENTS - 1) * VU_SEG_GAP)) / 2)

// Audio configuration
#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_BUFFER_SIZE  512

// Shared state
static volatile uint8_t level_left = 0;
static volatile uint8_t level_right = 0;
static SemaphoreHandle_t level_mutex = NULL;

// Display and audio objects
static DisplayPort *display = NULL;
static I2cMasterBus *i2c_bus = NULL;
static AudioPort *audio = NULL;

// Smoothing state for VU meters
static float smooth_left = 0.0f;
static float smooth_right = 0.0f;

// Attack and decay coefficients (for ~50ms attack, ~300ms decay at 20fps)
static const float ATTACK_COEF = 0.7f;
static const float DECAY_COEF = 0.15f;

static void draw_vu_meter(uint16_t x, uint8_t level) {
    // Draw all 8 segments, filling from bottom to top
    for (int seg = 0; seg < VU_SEGMENTS; seg++) {
        // Segment 0 is at the bottom, segment 7 at the top
        // Y position: bottom segment has highest Y value
        uint16_t seg_y = VU_TOP_Y + (VU_SEGMENTS - 1 - seg) * (VU_SEG_HEIGHT + VU_SEG_GAP);

        // Fill segment if level is high enough (seg 0 fills first)
        uint8_t color = (seg < level) ? ColorBlack : ColorWhite;
        display->RLCD_FillRect(x, seg_y, VU_SEG_WIDTH, VU_SEG_HEIGHT, color);
    }
}

static void draw_static_content() {
    // Clear screen
    display->RLCD_ColorClear(ColorWhite);

    // Draw centered text "JP LISTENNING DEVICE"
    const char *text = "JP LISTENNING DEVICE";
    int text_len = strlen(text);
    int text_width = text_len * 8;  // 8 pixels per character
    int text_x = (LCD_WIDTH - text_width) / 2;
    int text_y = (LCD_HEIGHT - 16) / 2;  // 16 pixels tall font

    display->RLCD_DrawString(text_x, text_y, text, ColorBlack);

    // Draw initial empty VU meters
    draw_vu_meter(VU_LEFT_X, 0);
    draw_vu_meter(VU_RIGHT_X, 0);

    display->RLCD_Display();
}

static float calculate_rms(int16_t *samples, int count) {
    if (count == 0) return 0.0f;

    int64_t sum_sq = 0;
    for (int i = 0; i < count; i++) {
        int32_t sample = samples[i];
        sum_sq += sample * sample;
    }

    return sqrtf((float)sum_sq / count);
}

static uint8_t rms_to_level(float rms) {
    // Map RMS to 0-8 segments
    // Typical 16-bit audio RMS range: 0 to ~20000 for loud sounds
    // Using log scale for more natural response
    if (rms < 100.0f) return 0;

    float db = 20.0f * log10f(rms / 32768.0f);  // dB relative to full scale
    // Map -60dB to 0dB to 0-8 segments
    float normalized = (db + 60.0f) / 60.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return (uint8_t)(normalized * VU_SEGMENTS + 0.5f);
}

static void audio_task(void *arg) {
    ESP_LOGI(TAG, "Audio task started");

    int16_t *audio_buffer = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_INTERNAL);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }

    // Separate buffers for left and right channels
    int16_t *left_samples = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE / 2, MALLOC_CAP_INTERNAL);
    int16_t *right_samples = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE / 2, MALLOC_CAP_INTERNAL);

    if (!left_samples || !right_samples) {
        ESP_LOGE(TAG, "Failed to allocate channel buffers");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        // Read stereo audio data (interleaved L, R, L, R, ...)
        int bytes_read = audio->readMicData(audio_buffer, AUDIO_BUFFER_SIZE);

        if (bytes_read > 0) {
            int sample_count = bytes_read / 4;  // 2 bytes per sample, 2 channels

            // Deinterleave stereo to separate L/R
            for (int i = 0; i < sample_count; i++) {
                left_samples[i] = audio_buffer[i * 2];
                right_samples[i] = audio_buffer[i * 2 + 1];
            }

            // Calculate RMS for each channel
            float rms_left = calculate_rms(left_samples, sample_count);
            float rms_right = calculate_rms(right_samples, sample_count);

            // Apply smoothing (fast attack, slow decay)
            if (rms_left > smooth_left) {
                smooth_left = smooth_left + ATTACK_COEF * (rms_left - smooth_left);
            } else {
                smooth_left = smooth_left + DECAY_COEF * (rms_left - smooth_left);
            }

            if (rms_right > smooth_right) {
                smooth_right = smooth_right + ATTACK_COEF * (rms_right - smooth_right);
            } else {
                smooth_right = smooth_right + DECAY_COEF * (rms_right - smooth_right);
            }

            // Convert to segment levels
            uint8_t new_level_left = rms_to_level(smooth_left);
            uint8_t new_level_right = rms_to_level(smooth_right);

            // Update shared state
            if (xSemaphoreTake(level_mutex, portMAX_DELAY) == pdTRUE) {
                level_left = new_level_left;
                level_right = new_level_right;
                xSemaphoreGive(level_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // ~100Hz audio processing
    }
}

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");

    uint8_t prev_left = 255;
    uint8_t prev_right = 255;

    while (1) {
        uint8_t cur_left = 0, cur_right = 0;

        // Get current levels
        if (xSemaphoreTake(level_mutex, portMAX_DELAY) == pdTRUE) {
            cur_left = level_left;
            cur_right = level_right;
            xSemaphoreGive(level_mutex);
        }

        // Only update display if levels changed
        if (cur_left != prev_left || cur_right != prev_right) {
            draw_vu_meter(VU_LEFT_X, cur_left);
            draw_vu_meter(VU_RIGHT_X, cur_right);
            display->RLCD_Display();

            prev_left = cur_left;
            prev_right = cur_right;
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // 20fps display update
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Hello VU starting...");

    // Create mutex for shared level data
    level_mutex = xSemaphoreCreateMutex();
    assert(level_mutex);

    // Initialize I2C bus
    ESP_LOGI(TAG, "Initializing I2C bus...");
    i2c_bus = new I2cMasterBus(I2C_SCL_PIN, I2C_SDA_PIN);

    // Initialize display
    ESP_LOGI(TAG, "Initializing display...");
    display = new DisplayPort(RLCD_MOSI_PIN, RLCD_SCK_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN, LCD_WIDTH, LCD_HEIGHT);
    display->RLCD_Init();

    // Draw static content (text)
    ESP_LOGI(TAG, "Drawing static content...");
    draw_static_content();

    // Initialize audio
    ESP_LOGI(TAG, "Initializing audio...");
    audio = new AudioPort(*i2c_bus);
    if (!audio->init()) {
        ESP_LOGE(TAG, "Failed to initialize audio, VU meters will not respond");
    } else {
        audio->setMicGain(25.0f);  // Set mic gain to 25dB
    }

    // Start tasks
    ESP_LOGI(TAG, "Starting tasks...");
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Hello VU initialized!");
}
