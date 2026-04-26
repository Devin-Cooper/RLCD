#pragma once

#include "i2c_bsp.hpp"
#include "pcf85063.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstddef>

namespace time_service {

struct TzEntry {
    const char* label;
    const char* posix_tz;
};

class TimeService {
public:
    explicit TimeService(i2c_bsp::I2cMasterBus& bus);
    ~TimeService();

    TimeService(const TimeService&) = delete;
    TimeService& operator=(const TimeService&) = delete;

    bool init();
    bool wasUnsetAtBoot() const;
    bool isTimeValid() const;

    void onWifiUp();

    bool setManual(const sensors::RtcTime& localTime);
    void setTimezone(const char* posixTz);
    const char* timezone() const;

    static const TzEntry* catalog(size_t* count);

private:
    sensors::Pcf85063 rtc_;
    std::atomic<bool> wasUnset_{false};
    std::atomic<bool> sntpStarted_{false};
    char tz_[64] = "UTC0";
    TaskHandle_t writebackTask_ = nullptr;

    static void writebackTaskTrampoline(void* arg);
    void writebackLoop();
    void persistTz();
    void loadTzFromNvs();
    void writeRtcFromSystemTime();
    static void sntpSyncCb(struct timeval* tv);
};

}  // namespace time_service
