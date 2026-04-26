#include "time_service.hpp"

#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs.h>
#include <sys/time.h>
#include <time.h>
#include <cstring>
#include <cstdlib>

static const char* TAG = "time_service";
static constexpr const char* kNvsNs = "time_service";
static constexpr const char* kNvsKeyTz = "tz";

namespace time_service {

namespace { TimeService* g_instance = nullptr; }

TimeService::TimeService(i2c_bsp::I2cMasterBus& bus) : rtc_(bus) {}
TimeService::~TimeService() {
    if (writebackTask_) vTaskDelete(writebackTask_);
}

bool TimeService::init() {
    g_instance = this;
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

    xTaskCreatePinnedToCore(&TimeService::writebackTaskTrampoline,
                            "time_wb", 3072, this,
                            /*priority*/ 4,
                            &writebackTask_,
                            /*core*/ tskNO_AFFINITY);
    return true;
}

bool TimeService::wasUnsetAtBoot() const { return wasUnset_.load(); }
bool TimeService::isTimeValid() const { return !wasUnset_.load(); }
void TimeService::onWifiUp() {
    bool expected = false;
    if (!sntpStarted_.compare_exchange_strong(expected, true)) {
        return;
    }
    ESP_LOGI(TAG, "Starting SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(&TimeService::sntpSyncCb);
    esp_sntp_init();
}
bool TimeService::setManual(const sensors::RtcTime& local) {
    if (local.year < 2024 || local.year > 2099) return false;
    if (local.month < 1 || local.month > 12) return false;
    if (local.day   < 1 || local.day   > 31) return false;
    if (local.hour > 23 || local.minute > 59 || local.second > 59) return false;

    struct tm tm_local{};
    tm_local.tm_year = local.year - 1900;
    tm_local.tm_mon  = local.month - 1;
    tm_local.tm_mday = local.day;
    tm_local.tm_hour = local.hour;
    tm_local.tm_min  = local.minute;
    tm_local.tm_sec  = local.second;
    tm_local.tm_isdst = -1;   // let mktime decide

    setenv("TZ", tz_, 1);
    tzset();
    time_t ts = mktime(&tm_local);
    if (ts == (time_t)-1) return false;

    struct timeval tv{ .tv_sec = ts, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    struct tm tm_utc{};
    gmtime_r(&ts, &tm_utc);
    sensors::RtcTime utc{};
    utc.year   = tm_utc.tm_year + 1900;
    utc.month  = tm_utc.tm_mon + 1;
    utc.day    = tm_utc.tm_mday;
    utc.hour   = tm_utc.tm_hour;
    utc.minute = tm_utc.tm_min;
    utc.second = tm_utc.tm_sec;
    utc.weekday = tm_utc.tm_wday;
    rtc_.setTime(utc);

    wasUnset_.store(false);
    ESP_LOGI(TAG, "System time set manually: %lld (UTC)", (long long)ts);
    return true;
}

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
void TimeService::writebackLoop() {
    constexpr uint32_t kHourMs = 60 * 60 * 1000;
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kHourMs));
        writeRtcFromSystemTime();
    }
}

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

void TimeService::writeRtcFromSystemTime() {
    time_t now = time(nullptr);
    if (now < 1700000000) {
        ESP_LOGW(TAG, "Skipping RTC write — system time looks invalid (%lld)", (long long)now);
        return;
    }
    struct tm tm_utc{};
    gmtime_r(&now, &tm_utc);
    sensors::RtcTime utc{};
    utc.year   = tm_utc.tm_year + 1900;
    utc.month  = tm_utc.tm_mon + 1;
    utc.day    = tm_utc.tm_mday;
    utc.hour   = tm_utc.tm_hour;
    utc.minute = tm_utc.tm_min;
    utc.second = tm_utc.tm_sec;
    utc.weekday = tm_utc.tm_wday;
    rtc_.setTime(utc);
    ESP_LOGI(TAG, "RTC written from system time (%lld)", (long long)now);
}
void TimeService::sntpSyncCb(struct timeval* /*tv*/) {
    if (!g_instance) return;
    g_instance->wasUnset_.store(false);
    if (g_instance->writebackTask_) {
        xTaskNotifyGive(g_instance->writebackTask_);
    }
    ESP_LOGI(TAG, "SNTP sync — system time updated, RTC writeback notified");
}

}  // namespace time_service
