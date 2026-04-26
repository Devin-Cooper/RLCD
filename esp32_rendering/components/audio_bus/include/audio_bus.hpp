#pragma once

#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace audio_bus {

struct AudioBusPins {
    gpio_num_t mclk;
    gpio_num_t bclk;
    gpio_num_t ws;
    gpio_num_t dout;   // ESP -> ES8311
    gpio_num_t din;    // ES7210 -> ESP
};

class AudioBus {
public:
    AudioBus(uint32_t sample_rate_hz, uint8_t bits_per_sample,
             uint8_t tx_channels, uint8_t rx_channels,
             i2s_port_t port, AudioBusPins pins);
    ~AudioBus();

    AudioBus(const AudioBus&) = delete;
    AudioBus& operator=(const AudioBus&) = delete;

    bool init();

    size_t write(const void* pcm, size_t bytes, TickType_t timeout);
    size_t read(void* pcm, size_t bytes, TickType_t timeout);

    uint32_t sampleRate() const { return sample_rate_; }
    uint8_t  txChannels() const { return tx_channels_; }
    uint8_t  rxChannels() const { return rx_channels_; }

    uint64_t txFramesWritten() const { return tx_frames_; }
    uint64_t rxFramesRead() const { return rx_frames_; }
    uint64_t txUnderruns() const { return tx_underruns_; }
    uint64_t rxOverruns() const { return rx_overruns_; }

    // Hooks for Speaker/Microphone integration.
    void setRxConsumerActive(bool active) { rx_consumer_active_.store(active); }
    void setRxWarmupUntilUs(int64_t until_us) { rx_warmup_until_us_.store(until_us); }
    int64_t rxWarmupRemainingUs() const;

    // Required for Microphone wake() — flush stale DMA descriptors.
    void rxFlush();

private:
    uint32_t sample_rate_;
    uint8_t  bits_per_sample_;
    uint8_t  tx_channels_;
    uint8_t  rx_channels_;
    i2s_port_t port_;
    AudioBusPins pins_;

    i2s_chan_handle_t tx_chan_ = nullptr;
    i2s_chan_handle_t rx_chan_ = nullptr;

    StreamBufferHandle_t tx_ring_ = nullptr;
    StreamBufferHandle_t rx_ring_ = nullptr;

    TaskHandle_t tx_task_ = nullptr;
    TaskHandle_t rx_task_ = nullptr;

    std::atomic<uint64_t> tx_frames_{0};
    std::atomic<uint64_t> rx_frames_{0};
    std::atomic<uint64_t> tx_underruns_{0};
    std::atomic<uint64_t> rx_overruns_{0};
    std::atomic<bool> rx_consumer_active_{false};
    std::atomic<int64_t> rx_warmup_until_us_{0};
    std::atomic<bool> running_{false};

    static void txTaskTramp(void* arg);
    static void rxTaskTramp(void* arg);
    void txLoop();
    void rxLoop();
};

}  // namespace audio_bus
