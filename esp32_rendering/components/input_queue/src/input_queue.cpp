#include "input_queue.hpp"
#include "esp_log.h"

static const char* TAG = "input_queue";

namespace input {

InputQueue::InputQueue(int capacity) {
    queue_ = xQueueCreate(capacity, sizeof(InputEvent));
    if (!queue_) {
        ESP_LOGE(TAG, "Failed to create input queue with capacity %d", capacity);
    }
}

InputQueue::~InputQueue() {
    if (queue_) {
        vQueueDelete(queue_);
    }
}

bool InputQueue::push(const InputEvent& event, uint32_t timeout_ms) {
    if (!queue_) return false;
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue_, &event, ticks) == pdTRUE;
}

bool InputQueue::pushOrDrop(const InputEvent& event) {
    if (push(event, 0)) return true;
    dropped_count_.fetch_add(1, std::memory_order_relaxed);
    return false;
}

uint32_t InputQueue::droppedCount() const {
    return dropped_count_.load(std::memory_order_relaxed);
}

bool InputQueue::pushFromISR(const InputEvent& event) {
    if (!queue_) return false;
    BaseType_t higher_priority_woken = pdFALSE;
    bool result = xQueueSendFromISR(queue_, &event, &higher_priority_woken) == pdTRUE;
    if (higher_priority_woken) {
        portYIELD_FROM_ISR();
    }
    return result;
}

bool InputQueue::pop(InputEvent& event, uint32_t timeout_ms) {
    if (!queue_) return false;
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue_, &event, ticks) == pdTRUE;
}

bool InputQueue::hasEvents() const {
    if (!queue_) return false;
    return uxQueueMessagesWaiting(queue_) > 0;
}

void InputQueue::clear() {
    if (!queue_) return;
    xQueueReset(queue_);
}

InputQueue& globalInputQueue() {
    static InputQueue instance(64);
    return instance;
}

} // namespace input
