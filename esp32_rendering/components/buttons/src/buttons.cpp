#include "buttons.hpp"
#include "esp_log.h"

static const char* TAG = "buttons";

namespace buttons {

ButtonHandler::ButtonHandler(const Config& config)
    : config_(config) {}

ButtonHandler::~ButtonHandler() {
    stopAutoUpdate();
}

bool ButtonHandler::init() {
    if (initialized_) {
        return true;
    }

    // Configure GPIO pins for both buttons
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << config_.pinA) | (1ULL << config_.pinB);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Active-low buttons

    esp_err_t err = gpio_config(&gpio_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(err));
        return false;
    }

    // Initialize state for each button
    for (size_t i = 0; i < static_cast<size_t>(Button::Count); i++) {
        states_[i] = ButtonState{};
        states_[i].buttonLevel = 1;  // Not pressed (active-low)
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Buttons initialized: A=GPIO%d (left), B=GPIO%d (right)",
             config_.pinA, config_.pinB);
    return true;
}

gpio_num_t ButtonHandler::getPin(size_t idx) const {
    return (idx == 0) ? config_.pinA : config_.pinB;
}

uint8_t ButtonHandler::readGpioLevel(size_t idx) const {
    return gpio_get_level(getPin(idx));
}

void ButtonHandler::fireEvent(size_t idx, Event event) {
    auto& s = states_[idx];
    size_t eventIdx = static_cast<size_t>(event);

    // Set polling flags
    switch (event) {
        case Event::SingleClick:
            s.clickedFlag = true;
            break;
        case Event::DoubleClick:
            s.doubleClickedFlag = true;
            break;
        case Event::LongPressStart:
            s.longPressedFlag = true;
            break;
        default:
            break;
    }

    // Fire callback if registered
    if (s.callbacks[eventIdx]) {
        s.callbacks[eventIdx](static_cast<Button>(idx), event, s.userData[eventIdx]);
    }
}

void ButtonHandler::processButton(size_t idx) {
    auto& s = states_[idx];
    uint8_t level = readGpioLevel(idx);
    constexpr uint8_t ACTIVE_LEVEL = 0;  // Active-low

    // Increment ticks when not idle
    if (s.state != STATE_IDLE) {
        s.ticks++;
    }

    // Debounce filtering: require consecutive stable reads
    if (level != s.buttonLevel) {
        if (++s.debounceCnt >= config_.debounceTicks) {
            s.buttonLevel = level;
            s.debounceCnt = 0;
        }
    } else {
        s.debounceCnt = 0;
    }

    // State machine
    switch (s.state) {
        case STATE_IDLE:
            if (s.buttonLevel == ACTIVE_LEVEL) {
                fireEvent(idx, Event::PressDown);
                s.ticks = 0;
                s.repeat = 1;
                s.state = STATE_PRESS;
            }
            break;

        case STATE_PRESS:
            if (s.buttonLevel != ACTIVE_LEVEL) {
                // Button released
                fireEvent(idx, Event::PressUp);
                s.ticks = 0;
                s.state = STATE_RELEASE;
            } else if (s.ticks > config_.longPressTicks) {
                // Long press detected
                fireEvent(idx, Event::LongPressStart);
                s.state = STATE_LONG_HOLD;
            }
            break;

        case STATE_RELEASE:
            if (s.buttonLevel == ACTIVE_LEVEL) {
                // Button pressed again (potential double-click)
                fireEvent(idx, Event::PressDown);
                s.repeat++;
                s.ticks = 0;
                s.state = STATE_REPEAT;
            } else if (s.ticks > config_.shortPressTicks) {
                // Timeout: determine click type based on repeat count
                if (s.repeat == 1) {
                    fireEvent(idx, Event::SingleClick);
                } else if (s.repeat == 2) {
                    fireEvent(idx, Event::DoubleClick);
                }
                s.state = STATE_IDLE;
            }
            break;

        case STATE_REPEAT:
            if (s.buttonLevel != ACTIVE_LEVEL) {
                // Released during repeat sequence
                fireEvent(idx, Event::PressUp);
                s.ticks = 0;
                s.state = STATE_RELEASE;
            } else if (s.ticks > config_.longPressTicks) {
                // Long press during repeat
                fireEvent(idx, Event::LongPressStart);
                s.state = STATE_LONG_HOLD;
            }
            break;

        case STATE_LONG_HOLD:
            if (s.buttonLevel != ACTIVE_LEVEL) {
                // Released from long press
                fireEvent(idx, Event::PressUp);
                s.state = STATE_IDLE;
            } else {
                // Still holding - fire hold event periodically
                if (s.ticks % config_.shortPressTicks == 0) {
                    fireEvent(idx, Event::LongPressHold);
                }
            }
            break;
    }
}

void ButtonHandler::update() {
    if (!initialized_) {
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(Button::Count); i++) {
        processButton(i);
    }
}

bool ButtonHandler::isPressed(Button btn) const {
    size_t idx = static_cast<size_t>(btn);
    if (idx >= static_cast<size_t>(Button::Count)) {
        return false;
    }
    return states_[idx].buttonLevel == 0;  // Active-low
}

bool ButtonHandler::wasClicked(Button btn) {
    size_t idx = static_cast<size_t>(btn);
    if (idx >= static_cast<size_t>(Button::Count)) {
        return false;
    }
    bool clicked = states_[idx].clickedFlag;
    states_[idx].clickedFlag = false;
    return clicked;
}

bool ButtonHandler::wasDoubleClicked(Button btn) {
    size_t idx = static_cast<size_t>(btn);
    if (idx >= static_cast<size_t>(Button::Count)) {
        return false;
    }
    bool clicked = states_[idx].doubleClickedFlag;
    states_[idx].doubleClickedFlag = false;
    return clicked;
}

bool ButtonHandler::wasLongPressed(Button btn) {
    size_t idx = static_cast<size_t>(btn);
    if (idx >= static_cast<size_t>(Button::Count)) {
        return false;
    }
    bool pressed = states_[idx].longPressedFlag;
    states_[idx].longPressedFlag = false;
    return pressed;
}

void ButtonHandler::onEvent(Button btn, Event event, Callback cb, void* userData) {
    size_t idx = static_cast<size_t>(btn);
    size_t eventIdx = static_cast<size_t>(event);
    if (idx >= static_cast<size_t>(Button::Count) ||
        eventIdx >= static_cast<size_t>(Event::Count)) {
        return;
    }
    states_[idx].callbacks[eventIdx] = cb;
    states_[idx].userData[eventIdx] = userData;
}

void ButtonHandler::clearCallbacks(Button btn) {
    size_t idx = static_cast<size_t>(btn);
    if (idx >= static_cast<size_t>(Button::Count)) {
        return;
    }
    for (size_t i = 0; i < static_cast<size_t>(Event::Count); i++) {
        states_[idx].callbacks[i] = nullptr;
        states_[idx].userData[i] = nullptr;
    }
}

void ButtonHandler::timerCallback(void* arg) {
    auto* handler = static_cast<ButtonHandler*>(arg);
    handler->update();
}

bool ButtonHandler::startAutoUpdate() {
    if (timerHandle_) {
        return true;  // Already running
    }

    if (!initialized_) {
        ESP_LOGE(TAG, "Cannot start auto-update: not initialized");
        return false;
    }

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = timerCallback;
    timer_args.arg = this;
    timer_args.name = "buttons";
    timer_args.dispatch_method = ESP_TIMER_TASK;

    esp_err_t err = esp_timer_create(&timer_args, &timerHandle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_timer_start_periodic(timerHandle_, config_.tickIntervalMs * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
        esp_timer_delete(timerHandle_);
        timerHandle_ = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Auto-update started (%dms interval)", config_.tickIntervalMs);
    return true;
}

void ButtonHandler::stopAutoUpdate() {
    if (timerHandle_) {
        esp_timer_stop(timerHandle_);
        esp_timer_delete(timerHandle_);
        timerHandle_ = nullptr;
        ESP_LOGI(TAG, "Auto-update stopped");
    }
}

bool ButtonHandler::isAutoUpdateRunning() const {
    return timerHandle_ != nullptr;
}

}  // namespace buttons
