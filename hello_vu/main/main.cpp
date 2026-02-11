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
#define VU_SEGMENTS     16
#define VU_SEG_WIDTH    60
#define VU_SEG_HEIGHT   14
#define VU_SEG_GAP      2
#define VU_LEFT_X       10
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

// AGC (Automatic Gain Control) state
// Reference level adapts to the signal - fast attack, slow release
static float ref_level_left = 100.0f;   // Initial reference RMS
static float ref_level_right = 100.0f;

// Noise floor tracking - slowly follows the minimum signal level
static float noise_floor_left = 0.0f;
static float noise_floor_right = 0.0f;

// AGC parameters (at ~100Hz audio processing rate)
static const float AGC_ATTACK_COEF = 0.3f;    // Fast attack (~30ms)
static const float AGC_RELEASE_COEF = 0.005f; // Slow release (~2 seconds)
static const float AGC_MIN_REF = 20.0f;       // Minimum reference (max sensitivity)
static const float AGC_MAX_REF = 5000.0f;     // Maximum reference (min sensitivity)
static const float AGC_TARGET = 0.6f;         // Target level (60% of full scale)

// Noise floor parameters - tracks the "quiet" baseline
static const float NOISE_ATTACK_COEF = 0.02f;   // Moderate rise to track varying noise
static const float NOISE_RELEASE_COEF = 0.05f;  // Moderate drop when signal drops

static void draw_vu_meter(uint16_t x, uint8_t level) {
    // Draw all segments, filling from bottom to top
    for (int seg = 0; seg < VU_SEGMENTS; seg++) {
        // Segment 0 is at the bottom, highest segment at the top
        uint16_t seg_y = VU_TOP_Y + (VU_SEGMENTS - 1 - seg) * (VU_SEG_HEIGHT + VU_SEG_GAP);

        // Fill segment - black if active, white if inactive
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

// Update noise floor estimate - tracks the baseline signal level
static void update_noise_floor(float rms, float *noise_floor) {
    if (rms < 1.0f) rms = 1.0f;

    // Slowly rise to track noise, quickly drop when signal drops
    if (rms > *noise_floor) {
        *noise_floor += NOISE_ATTACK_COEF * (rms - *noise_floor);
    } else {
        *noise_floor += NOISE_RELEASE_COEF * (rms - *noise_floor);
    }

    // Keep noise floor reasonable
    if (*noise_floor < 1.0f) *noise_floor = 1.0f;
}

// Update AGC reference level based on signal above noise floor
static void update_agc_reference(float signal, float *ref_level) {
    if (signal < 1.0f) return;

    // Fast attack: quickly raise reference when signal is loud
    // Slow release: slowly lower reference when signal is quiet
    if (signal > *ref_level) {
        *ref_level += AGC_ATTACK_COEF * (signal - *ref_level);
    } else {
        *ref_level += AGC_RELEASE_COEF * (signal - *ref_level);
    }

    // Clamp reference to valid range
    if (*ref_level < AGC_MIN_REF) *ref_level = AGC_MIN_REF;
    if (*ref_level > AGC_MAX_REF) *ref_level = AGC_MAX_REF;
}

static uint8_t rms_to_level_agc(float rms, float noise_floor, float ref_level) {
    // Subtract noise floor to get actual signal
    float signal = rms - noise_floor;

    // Require signal to be at least 20% above noise floor to register
    float threshold = noise_floor * 0.3f;
    if (signal < threshold) return 0;
    signal -= threshold;  // Subtract threshold from signal

    // Normalize signal against the adaptive reference level
    float normalized = (signal / ref_level) / AGC_TARGET;

    // Apply soft compression for more natural response
    normalized = sqrtf(normalized);

    if (normalized > 1.0f) normalized = 1.0f;

    return (uint8_t)(normalized * VU_SEGMENTS + 0.5f);
}

static void audio_task(void *arg) {
    ESP_LOGI(TAG, "Audio task started");

    // 32-bit samples, 4 TDM slots
    int32_t *audio_buffer = (int32_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE * 2, MALLOC_CAP_INTERNAL);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }

    // Separate buffers for left and right channels (16-bit after conversion)
    int16_t *left_samples = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_INTERNAL);
    int16_t *right_samples = (int16_t *)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_INTERNAL);

    if (!left_samples || !right_samples) {
        ESP_LOGE(TAG, "Failed to allocate channel buffers");
        vTaskDelete(NULL);
        return;
    }

    static int debug_counter = 0;

    while (1) {
        // Read TDM audio data (4 channels: MIC1, MIC2, MIC3, MIC4)
        int bytes_read = audio->readMicData(audio_buffer, AUDIO_BUFFER_SIZE * 2);

        if (bytes_read > 0) {
            // 4 bytes per sample (32-bit), 4 channels per frame
            int frame_count = bytes_read / 16;

            // Extract MIC1 (slot 0) and MIC2 (slot 1), convert 32-bit to 16-bit
            for (int i = 0; i < frame_count; i++) {
                // TDM frame: [MIC1, MIC2, MIC3, MIC4]
                left_samples[i] = (int16_t)(audio_buffer[i * 4] >> 16);      // MIC1
                right_samples[i] = (int16_t)(audio_buffer[i * 4 + 1] >> 16); // MIC2
            }

            int sample_count = frame_count;

            // Calculate RMS for each channel
            float rms_left = calculate_rms(left_samples, sample_count);
            float rms_right = calculate_rms(right_samples, sample_count);

            // Update noise floor estimates (tracks baseline)
            update_noise_floor(rms_left, &noise_floor_left);
            update_noise_floor(rms_right, &noise_floor_right);

            // Calculate signal above noise floor
            float signal_left = rms_left - noise_floor_left;
            float signal_right = rms_right - noise_floor_right;
            if (signal_left < 0) signal_left = 0;
            if (signal_right < 0) signal_right = 0;

            // Update AGC reference levels based on signal (not raw RMS)
            update_agc_reference(signal_left, &ref_level_left);
            update_agc_reference(signal_right, &ref_level_right);

            // Debug logging every 100 iterations (~1 second)
            if (++debug_counter >= 100) {
                ESP_LOGI(TAG, "L: rms=%.0f nf=%.0f sig=%.0f | R: rms=%.0f nf=%.0f sig=%.0f",
                         rms_left, noise_floor_left, signal_left,
                         rms_right, noise_floor_right, signal_right);
                debug_counter = 0;
            }

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

            // Convert to segment levels using AGC with noise floor subtraction
            uint8_t new_level_left = rms_to_level_agc(smooth_left, noise_floor_left, ref_level_left);
            uint8_t new_level_right = rms_to_level_agc(smooth_right, noise_floor_right, ref_level_right);

            // Update shared state
            if (xSemaphoreTake(level_mutex, portMAX_DELAY) == pdTRUE) {
                level_left = new_level_left;
                level_right = new_level_right;
                xSemaphoreGive(level_mutex);
            }
        } else {
            // Log read failure periodically
            if (++debug_counter >= 100) {
                ESP_LOGW(TAG, "Audio read failed or returned 0 bytes");
                debug_counter = 0;
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

    // Give hardware time to stabilize after power-up
    vTaskDelay(pdMS_TO_TICKS(500));

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

    // Extra delay after display init for LCD to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

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
