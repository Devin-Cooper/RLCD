#pragma once

#include <atomic>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace input {

enum class Source : uint8_t {
    Keyboard,
    Button,
    System,   // Synthetic events from WiFi/BLE state changes (spec §Async events)
};

enum class EventType : uint8_t {
    Keypress,
    ButtonShort,
    ButtonLong,
    ButtonDouble,
    WifiStateChanged,   // data[0]=wifi::State, data[1]=wifi_err_reason_t or 0, data_length=2
    BleStateChanged,    // data[0]=ble_hid::State, data_length=1
    WifiScanDone,       // data_length = 0 (added per plan Amendment H)
};

struct InputEvent {
    Source source;
    EventType type;
    uint8_t button_id;     // For button events: 0=A, 1=B
    uint8_t data[8];       // For key events: terminal byte sequence
    uint8_t data_length;   // Number of valid bytes in data
};

/// Thread-safe input event queue.
/// Both BLE keyboard and physical button handlers push events here.
/// The application layer reads from this single queue.
class InputQueue {
public:
    explicit InputQueue(int capacity = 64);
    ~InputQueue();

    InputQueue(const InputQueue&) = delete;
    InputQueue& operator=(const InputQueue&) = delete;

    /// Push an event. Returns true on success.
    bool push(const InputEvent& event, uint32_t timeout_ms = 0);

    /// Push from ISR context. Returns true on success.
    bool pushFromISR(const InputEvent& event);

    /// Push with silent drop on queue-full. Increments droppedCount on
    /// failure; always returns without blocking. Prefer at producer sites
    /// that cannot tolerate backpressure (BLE host task, button timer).
    bool pushOrDrop(const InputEvent& event);

    /// Cumulative count of events dropped by pushOrDrop (never resets).
    uint32_t droppedCount() const;

    /// Pop an event. Blocks up to timeout_ms (0 = no wait).
    bool pop(InputEvent& event, uint32_t timeout_ms = 0);

    /// Check if queue has events without removing.
    bool hasEvents() const;

    /// Drain all events.
    void clear();

private:
    QueueHandle_t queue_;
    std::atomic<uint32_t> dropped_count_{0};
};

/// Global input queue singleton.
InputQueue& globalInputQueue();

} // namespace input
