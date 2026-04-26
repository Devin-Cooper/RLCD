#include "audio_bus.hpp"
#include <esp_log.h>
#include <esp_timer.h>

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

bool AudioBus::init() { return false; }
size_t AudioBus::write(const void*, size_t, TickType_t) { return 0; }
size_t AudioBus::read(void*, size_t, TickType_t) { return 0; }
int64_t AudioBus::rxWarmupRemainingUs() const {
    int64_t now = esp_timer_get_time();
    int64_t until = rx_warmup_until_us_.load();
    return until > now ? (until - now) : 0;
}
void AudioBus::rxFlush() {}
void AudioBus::txTaskTramp(void* arg) { static_cast<AudioBus*>(arg)->txLoop(); }
void AudioBus::rxTaskTramp(void* arg) { static_cast<AudioBus*>(arg)->rxLoop(); }
void AudioBus::txLoop() {}
void AudioBus::rxLoop() {}

}  // namespace audio_bus
