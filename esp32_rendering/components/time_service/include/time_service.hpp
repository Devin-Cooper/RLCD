#pragma once

#include "i2c_bsp.hpp"
#include "pcf85063.hpp"
#ifndef RLCD_HOST_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <atomic>
#include <cstddef>
#include <time.h>

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
#ifndef RLCD_HOST_TEST
    TaskHandle_t writebackTask_ = nullptr;

    static void writebackTaskTrampoline(void* arg);
    void writebackLoop();
#endif
    void persistTz();
    void loadTzFromNvs();
    void writeRtcFromSystemTime();
    static void sntpSyncCb(struct timeval* tv);
};

// Pure helpers — host-testable, no hardware deps.
namespace detail {
    bool localTimeToUtcEpoch(const sensors::RtcTime& local, const char* tz, time_t& out);
    void utcEpochToRtcTime(time_t ts, sensors::RtcTime& out);
    bool sanityGuardEpoch(time_t ts);   // true if ts >= 1700000000
}

}  // namespace time_service
