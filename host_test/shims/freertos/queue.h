#ifndef RLCD_HOST_SHIM_FREERTOS_QUEUE_H
#define RLCD_HOST_SHIM_FREERTOS_QUEUE_H

#include "FreeRTOS.h"
#include <stddef.h>
#include <string.h>
#include <stdint.h>

typedef struct QueueHandleImpl* QueueHandle_t;

#ifdef __cplusplus

#include <deque>
#include <vector>
#include <mutex>

struct QueueHandleImpl {
    size_t item_size;
    std::deque<std::vector<uint8_t>> items;
    std::mutex m;
};

static inline QueueHandle_t xQueueCreate(size_t length, size_t item_size) {
    (void)length;
    auto* q = new QueueHandleImpl{};
    q->item_size = item_size;
    return q;
}

static inline void vQueueDelete(QueueHandle_t q) { delete q; }

static inline BaseType_t xQueueSend(QueueHandle_t q, const void* item,
                                    TickType_t ticks) {
    (void)ticks;
    std::lock_guard<std::mutex> lk(q->m);
    std::vector<uint8_t> buf(q->item_size);
    memcpy(buf.data(), item, q->item_size);
    q->items.push_back(std::move(buf));
    return pdTRUE;
}

static inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void* item,
                                           BaseType_t* woken) {
    (void)woken;
    return xQueueSend(q, item, 0);
}

static inline BaseType_t xQueueReceive(QueueHandle_t q, void* out,
                                       TickType_t ticks) {
    (void)ticks;
    std::lock_guard<std::mutex> lk(q->m);
    if (q->items.empty()) return pdFALSE;
    memcpy(out, q->items.front().data(), q->item_size);
    q->items.pop_front();
    return pdTRUE;
}

static inline unsigned int uxQueueMessagesWaiting(QueueHandle_t q) {
    std::lock_guard<std::mutex> lk(q->m);
    return static_cast<unsigned int>(q->items.size());
}

static inline void xQueueReset(QueueHandle_t q) {
    std::lock_guard<std::mutex> lk(q->m);
    q->items.clear();
}

#endif /* __cplusplus */

#endif
