#include "time_service.hpp"

namespace time_service {

TimeService::TimeService(i2c_bsp::I2cMasterBus& bus) : rtc_(bus) {}
TimeService::~TimeService() {
    if (writebackTask_) vTaskDelete(writebackTask_);
}

bool TimeService::init() { return false; }
bool TimeService::wasUnsetAtBoot() const { return wasUnset_.load(); }
bool TimeService::isTimeValid() const { return !wasUnset_.load(); }
void TimeService::onWifiUp() {}
bool TimeService::setManual(const sensors::RtcTime&) { return false; }
void TimeService::setTimezone(const char*) {}
const char* TimeService::timezone() const { return tz_; }

void TimeService::writebackTaskTrampoline(void* arg) {
    static_cast<TimeService*>(arg)->writebackLoop();
}
void TimeService::writebackLoop() {}
void TimeService::persistTz() {}
void TimeService::loadTzFromNvs() {}
void TimeService::writeRtcFromSystemTime() {}
void TimeService::sntpSyncCb(struct timeval*) {}

}  // namespace time_service
