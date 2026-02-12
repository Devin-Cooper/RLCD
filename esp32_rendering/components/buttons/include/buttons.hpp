#pragma once

/// @file buttons.hpp
/// @brief Button handling for ESP32-S3-RLCD-4.2 board
///
/// Hardware Configuration (from schematic):
/// ┌─────────────────────────────────────────┐
/// │  Physical layout:  [A]  [PWR]  [B]      │
/// │                    Left Middle Right    │
/// ├─────────────────────────────────────────┤
/// │  Button A (Left)   │  GPIO 18  │ Key4   │
/// │  Button B (Right)  │  GPIO 0   │ Key1   │
/// │  PWR (Middle)      │  N/A      │ Key3   │
/// └─────────────────────────────────────────┘
///
/// - Buttons A and B are active-low (pressed = GPIO reads 0)
/// - Button A has external 10K pull-up resistor (R17)
/// - Button B uses internal pull-up (also BOOT button)
/// - PWR button is handled by power IC (U3), not software-accessible
///
/// Timing:
/// - Debounce: 15ms (3 ticks @ 5ms)
/// - Double-click window: 300ms
/// - Long press threshold: 1000ms
///
/// Usage:
/// @code
/// buttons::ButtonHandler btns;
/// btns.init();
/// btns.startAutoUpdate();  // 5ms timer handles state machine
///
/// // In main loop:
/// if (btns.wasClicked(buttons::Button::A)) { /* left clicked */ }
/// if (btns.wasDoubleClicked(buttons::Button::B)) { /* right double-clicked */ }
/// if (btns.wasLongPressed(buttons::Button::A)) { /* left long-pressed */ }
/// @endcode

#include <cstdint>
#include "driver/gpio.h"
#include "esp_timer.h"

namespace buttons {

/// Button identifiers
/// Physical layout on ESP32-S3-RLCD-4.2: [A/Left] [PWR/Middle] [B/Right]
/// Note: PWR button is handled by power IC, not software-accessible
enum class Button : uint8_t {
    A = 0,  // GPIO 18 - Left button (active-low, external 10K pull-up)
    B = 1,  // GPIO 0  - Right button (active-low, internal pull-up, BOOT)
    Count = 2
};

/// Event types that can trigger callbacks
enum class Event : uint8_t {
    PressDown = 0,     // Button pressed (GPIO went low)
    PressUp,           // Button released (GPIO went high)
    SingleClick,       // Single click completed
    DoubleClick,       // Double click completed
    LongPressStart,    // Long press threshold reached (~1s)
    LongPressHold,     // Continued holding after long press
    Count
};

/// Configuration for button timing and GPIO pins
struct Config {
    gpio_num_t pinA = GPIO_NUM_18;  // Left button
    gpio_num_t pinB = GPIO_NUM_0;   // Right button (BOOT)
    uint8_t tickIntervalMs = 5;     // Polling interval
    uint8_t debounceTicks = 3;      // Debounce filter depth (15ms @ 5ms tick)
    uint16_t shortPressTicks = 60;  // Short press threshold (300ms @ 5ms tick)
    uint16_t longPressTicks = 200;  // Long press threshold (1000ms @ 5ms tick)
};

/// Callback function type
using Callback = void(*)(Button btn, Event event, void* userData);

/// Button handler with debouncing and event detection
///
/// Supports two usage modes:
/// 1. Polling: Call update() regularly, check state with isPressed()/wasClicked()
/// 2. Callbacks: Register callbacks with onEvent(), optionally use startAutoUpdate()
///
/// Example (polling):
/// @code
/// buttons::ButtonHandler btns;
/// btns.init();
/// while (true) {
///     btns.update();
///     if (btns.wasClicked(Button::A)) { /* left button clicked */ }
///     if (btns.wasClicked(Button::B)) { /* right button clicked */ }
///     vTaskDelay(pdMS_TO_TICKS(5));
/// }
/// @endcode
class ButtonHandler {
public:
    explicit ButtonHandler(const Config& config = Config{});
    ~ButtonHandler();

    // Non-copyable
    ButtonHandler(const ButtonHandler&) = delete;
    ButtonHandler& operator=(const ButtonHandler&) = delete;

    /// Initialize GPIO pins and internal state
    /// @return true on success
    bool init();

    /// Poll buttons and process state machine
    /// Must be called periodically at tickIntervalMs rate
    void update();

    // ========== Polling API ==========

    /// Check if button is currently pressed (real-time, post-debounce)
    bool isPressed(Button btn) const;

    /// Check if single click occurred since last call (auto-clears)
    bool wasClicked(Button btn);

    /// Check if double click occurred since last call (auto-clears)
    bool wasDoubleClicked(Button btn);

    /// Check if long press started since last call (auto-clears)
    bool wasLongPressed(Button btn);

    // ========== Callback API ==========

    /// Register callback for a specific button and event
    /// @param btn Button to monitor
    /// @param event Event type to trigger on
    /// @param cb Callback function (nullptr to unregister)
    /// @param userData User data passed to callback
    void onEvent(Button btn, Event event, Callback cb, void* userData = nullptr);

    /// Clear all callbacks for a button
    void clearCallbacks(Button btn);

    // ========== Timer-based Auto-Update ==========

    /// Start automatic update via esp_timer (5ms periodic)
    /// After calling this, you don't need to call update() manually
    /// @return true on success
    bool startAutoUpdate();

    /// Stop automatic update timer
    void stopAutoUpdate();

    /// Check if auto-update is running
    bool isAutoUpdateRunning() const;

private:
    // State machine states
    static constexpr uint8_t STATE_IDLE = 0;
    static constexpr uint8_t STATE_PRESS = 1;
    static constexpr uint8_t STATE_RELEASE = 2;
    static constexpr uint8_t STATE_REPEAT = 3;
    static constexpr uint8_t STATE_LONG_HOLD = 4;

    struct ButtonState {
        uint16_t ticks = 0;
        uint8_t repeat = 0;
        uint8_t state = STATE_IDLE;
        uint8_t debounceCnt = 0;
        uint8_t buttonLevel = 1;  // Current debounced level (1 = not pressed)

        // Polling flags (cleared on read)
        bool clickedFlag = false;
        bool doubleClickedFlag = false;
        bool longPressedFlag = false;

        // Callbacks per event type
        Callback callbacks[static_cast<size_t>(Event::Count)] = {};
        void* userData[static_cast<size_t>(Event::Count)] = {};
    };

    Config config_;
    ButtonState states_[static_cast<size_t>(Button::Count)];
    esp_timer_handle_t timerHandle_ = nullptr;
    bool initialized_ = false;

    void processButton(size_t idx);
    uint8_t readGpioLevel(size_t idx) const;
    void fireEvent(size_t idx, Event event);
    gpio_num_t getPin(size_t idx) const;

    static void timerCallback(void* arg);
};

}  // namespace buttons
