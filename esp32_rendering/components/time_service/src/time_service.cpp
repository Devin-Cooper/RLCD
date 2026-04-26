#include "time_service.hpp"

#include <esp_log.h>
#include <nvs.h>
#include <sys/time.h>
#include <time.h>
#include <cstring>
#include <cstdlib>

static const char* TAG = "time_service";
static constexpr const char* kNvsNs = "time_service";
static constexpr const char* kNvsKeyTz = "tz";

namespace time_service {

TimeService::TimeService(i2c_bsp::I2cMasterBus& bus) : rtc_(bus) {}
TimeService::~TimeService() {
    if (writebackTask_) vTaskDelete(writebackTask_);
}

bool TimeService::init() {
    if (!rtc_.init()) {
        ESP_LOGE(TAG, "Pcf85063 init failed");
        wasUnset_.store(true);
        loadTzFromNvs();
        setenv("TZ", tz_, 1);
        tzset();
        return false;
    }

    loadTzFromNvs();
    setenv("TZ", tz_, 1);
    tzset();

    if (rtc_.oscillatorStoppedAtBoot()) {
        ESP_LOGW(TAG, "RTC was unset at boot");
        wasUnset_.store(true);
        return true;
    }

    sensors::RtcTime t{};
    if (!rtc_.getTime(t)) {
        ESP_LOGW(TAG, "RTC returned invalid time");
        wasUnset_.store(true);
        return true;
    }

    struct tm tm_utc{};
    tm_utc.tm_year = t.year - 1900;
    tm_utc.tm_mon  = t.month - 1;
    tm_utc.tm_mday = t.day;
    tm_utc.tm_hour = t.hour;
    tm_utc.tm_min  = t.minute;
    tm_utc.tm_sec  = t.second;
    tm_utc.tm_isdst = 0;
    // newlib lacks timegm(); compute UTC epoch by temporarily switching TZ.
    char* prevTz = getenv("TZ");
    char savedTz[64];
    if (prevTz) {
        std::strncpy(savedTz, prevTz, sizeof(savedTz) - 1);
        savedTz[sizeof(savedTz) - 1] = '\0';
    } else {
        savedTz[0] = '\0';
    }
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t ts = mktime(&tm_utc);
    if (savedTz[0]) {
        setenv("TZ", savedTz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    struct timeval tv{ .tv_sec = ts, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    ESP_LOGI(TAG, "System time set from RTC: %lld (UTC)", (long long)ts);
    return true;
}

bool TimeService::wasUnsetAtBoot() const { return wasUnset_.load(); }
bool TimeService::isTimeValid() const { return !wasUnset_.load(); }
void TimeService::onWifiUp() {}
bool TimeService::setManual(const sensors::RtcTime&) { return false; }

void TimeService::setTimezone(const char* posixTz) {
    if (!posixTz) return;
    std::strncpy(tz_, posixTz, sizeof(tz_) - 1);
    tz_[sizeof(tz_) - 1] = '\0';
    setenv("TZ", tz_, 1);
    tzset();
    persistTz();
}

const char* TimeService::timezone() const { return tz_; }

void TimeService::writebackTaskTrampoline(void* arg) {
    static_cast<TimeService*>(arg)->writebackLoop();
}
void TimeService::writebackLoop() {}

void TimeService::persistTz() {
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, kNvsKeyTz, tz_);
    nvs_commit(h);
    nvs_close(h);
}

void TimeService::loadTzFromNvs() {
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(tz_);
    if (nvs_get_str(h, kNvsKeyTz, tz_, &len) != ESP_OK) {
        std::strcpy(tz_, "UTC0");
    }
    nvs_close(h);
}

void TimeService::writeRtcFromSystemTime() {}
void TimeService::sntpSyncCb(struct timeval*) {}

}  // namespace time_service
