#include "audio_bus.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>

namespace audio_bus {

static const char* TAG = "audio_bus";

AudioBus::AudioBus(uint32_t sample_rate_hz, uint8_t bits_per_sample,
                   uint8_t tx_channels, uint8_t rx_channels,
                   i2s_port_t port, AudioBusPins pins)
    : sample_rate_(sample_rate_hz),
      bits_per_sample_(bits_per_sample),
      tx_channels_(tx_channels),
      rx_channels_(rx_channels),
      port_(port),
      pins_(pins) {}

AudioBus::~AudioBus() {
    running_.store(false);
    if (tx_task_) vTaskDelete(tx_task_);
    if (rx_task_) vTaskDelete(rx_task_);
    if (tx_chan_) i2s_del_channel(tx_chan_);
    if (rx_chan_) i2s_del_channel(rx_chan_);
    if (tx_ring_) vStreamBufferDelete(tx_ring_);
    if (rx_ring_) vStreamBufferDelete(rx_ring_);
}

bool AudioBus::init() {
    static constexpr size_t kRingBytes = 16 * 1024;  // 16 KB rings
    tx_ring_ = xStreamBufferCreate(kRingBytes, 4);
    rx_ring_ = xStreamBufferCreate(kRingBytes, 4);
    if (!tx_ring_ || !rx_ring_) {
        ESP_LOGE(TAG, "StreamBuffer alloc failed");
        return false;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port_, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 240;
    chan_cfg.auto_clear = true;
    if (i2s_new_channel(&chan_cfg, &tx_chan_, &rx_chan_) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed");
        return false;
    }

    // TX: 1-channel mono, slot_mask = LEFT.
    {
        i2s_std_config_t cfg = {};
        cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_);
        cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)bits_per_sample_, I2S_SLOT_MODE_MONO);
        cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
        cfg.gpio_cfg = {
            .mclk = pins_.mclk,
            .bclk = pins_.bclk,
            .ws   = pins_.ws,
            .dout = pins_.dout,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {},
        };
        if (i2s_channel_init_std_mode(tx_chan_, &cfg) != ESP_OK) {
            ESP_LOGE(TAG, "TX channel init failed");
            return false;
        }
    }

    // RX: 2-channel stereo.
    {
        i2s_std_config_t cfg = {};
        cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_);
        cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)bits_per_sample_, I2S_SLOT_MODE_STEREO);
        cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
        cfg.gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   // already driven by TX
            .bclk = pins_.bclk,
            .ws   = pins_.ws,
            .dout = I2S_GPIO_UNUSED,
            .din  = pins_.din,
            .invert_flags = {},
        };
        if (i2s_channel_init_std_mode(rx_chan_, &cfg) != ESP_OK) {
            ESP_LOGE(TAG, "RX channel init failed");
            return false;
        }
    }

    if (i2s_channel_enable(tx_chan_) != ESP_OK) return false;
    if (i2s_channel_enable(rx_chan_) != ESP_OK) return false;

    running_.store(true);
    xTaskCreatePinnedToCore(&AudioBus::txTaskTramp, "audio_tx", 3072, this, 10, &tx_task_, 1);
    xTaskCreatePinnedToCore(&AudioBus::rxTaskTramp, "audio_rx", 3072, this, 10, &rx_task_, 1);

    ESP_LOGI(TAG, "AudioBus initialized: %lu Hz, %u-bit, TX=%u ch, RX=%u ch",
             (unsigned long)sample_rate_, bits_per_sample_, tx_channels_, rx_channels_);
    return true;
}

size_t AudioBus::write(const void* pcm, size_t bytes, TickType_t timeout) {
    return xStreamBufferSend(tx_ring_, pcm, bytes, timeout);
}
size_t AudioBus::read(void* pcm, size_t bytes, TickType_t timeout) {
    return xStreamBufferReceive(rx_ring_, pcm, bytes, timeout);
}

int64_t AudioBus::rxWarmupRemainingUs() const {
    int64_t now = esp_timer_get_time();
    int64_t until = rx_warmup_until_us_.load();
    return until > now ? (until - now) : 0;
}

void AudioBus::rxFlush() {
    if (rx_ring_) xStreamBufferReset(rx_ring_);
    if (rx_chan_) {
        i2s_channel_disable(rx_chan_);
        i2s_channel_enable(rx_chan_);
    }
}

void AudioBus::txTaskTramp(void* arg) { static_cast<AudioBus*>(arg)->txLoop(); }
void AudioBus::rxTaskTramp(void* arg) { static_cast<AudioBus*>(arg)->rxLoop(); }

void AudioBus::txLoop() {
    constexpr size_t kFrameBytes = 240 * 2;  // 240 frames * 2 bytes (mono 16-bit)
    static uint8_t buf[kFrameBytes];

    while (running_.load()) {
        size_t got = xStreamBufferReceive(tx_ring_, buf, kFrameBytes, pdMS_TO_TICKS(10));
        if (got == 0) {
            // Underrun — write silence so MCLK keeps flowing for the codecs.
            std::memset(buf, 0, kFrameBytes);
            tx_underruns_.fetch_add(1);
            got = kFrameBytes;
        }
        size_t written = 0;
        i2s_channel_write(tx_chan_, buf, got, &written, portMAX_DELAY);
        tx_frames_.fetch_add(written / 2);
    }
}

void AudioBus::rxLoop() {
    constexpr size_t kFrameBytes = 240 * 4;  // 240 frames * 4 bytes (stereo 16-bit)
    static uint8_t buf[kFrameBytes];

    while (running_.load()) {
        size_t got = 0;
        i2s_channel_read(rx_chan_, buf, kFrameBytes, &got, portMAX_DELAY);
        if (got == 0) continue;
        rx_frames_.fetch_add(got / 4);

        // Drop frames if no consumer or still in mic warm-up.
        if (!rx_consumer_active_.load()) continue;
        if (rxWarmupRemainingUs() > 0) continue;

        size_t sent = xStreamBufferSend(rx_ring_, buf, got, 0);
        if (sent < got) rx_overruns_.fetch_add(1);
    }
}

}  // namespace audio_bus
