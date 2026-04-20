#include "test_console_response.hpp"
#include "test_console_context.hpp"

#include "esp_console.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "screen_stack.hpp"
#include "overlay.hpp"
#include "wifi_manager.hpp"
#include "ssh_client.hpp"
#include "ble_hid.hpp"
#include "config_manager.hpp"
#include "config_store_nvs.hpp"

#include <cxxabi.h>
#include <typeinfo>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace test_console {

// Build-time guard per spec — catches future `final` Screen regressions.
static_assert(std::is_polymorphic_v<app::Screen>,
              "Screen must be polymorphic for typeid to resolve derived types");

// --- stack ---
static int cmd_stack(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    size_t depth = ctx->stack.depth();
    for (size_t i = 0; i < depth; ++i) {
        app::Screen* s = ctx->stack.at(i);
        if (!s) continue;
        int status = 0;
        const char* mangled = typeid(*s).name();
        char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
        const char* name = (status == 0 && demangled) ? demangled : mangled;
        data("%zu %s %s", i, name, s->isTransparent() ? "transparent" : "opaque");
        if (demangled) free(demangled);
    }
    ok("depth=%zu", depth);
    return 0;
}

// --- heap ---
static int cmd_heap(int, char**) {
    size_t free_total = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    size_t dma_max = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    ok("free=%zu min_free=%zu dma_max=%zu", free_total, min_free, dma_max);
    return 0;
}

// --- task list ---
static int cmd_task_list(int, char**) {
#if CONFIG_FREERTOS_USE_TRACE_FACILITY && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    char buf[1024];
    vTaskList(buf);
    char* p = buf;
    while (*p) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        if (*p) data("%s", p);
        if (!nl) break;
        p = nl + 1;
    }
    ok("%s", "");
    return 0;
#else
    err(11, "FREERTOS_USE_TRACE_FACILITY not enabled");
    return 1;
#endif
}

// --- wifi status ---
static int cmd_wifi_status(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    wifi::ConnectionInfo ci = ctx->wifiMgr.connectionInfo();
    const char* st = "unknown";
    switch (ci.state) {
        case wifi::State::Disconnected: st = "disconnected"; break;
        case wifi::State::Connecting:   st = "connecting";   break;
        case wifi::State::Connected:    st = "connected";    break;
    }
    ok("state=%s ssid=%s ip=%s rssi=%d",
       st, ci.ssid, ci.ip, (int)ci.rssi);
    return 0;
}

// --- ssh status ---
static int cmd_ssh_status(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    auto s = ctx->sshClient.state();
    const char* st = "unknown";
    switch (s) {
        case ssh::State::Disconnected:    st = "disconnected";    break;
        case ssh::State::Connecting:      st = "connecting";      break;
        case ssh::State::Authenticating:  st = "authenticating";  break;
        case ssh::State::Connected:       st = "connected";       break;
        case ssh::State::Error:           st = "error";           break;
    }
    ok("state=%s", st);
    return 0;
}

// --- ble status ---
static int cmd_ble_status(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    auto s = ctx->bleHost.state();
    const char* st = "unknown";
    switch (s) {
        case ble_hid::State::Disabled:     st = "disabled";     break;
        case ble_hid::State::Scanning:     st = "scanning";     break;
        case ble_hid::State::Connecting:   st = "connecting";   break;
        case ble_hid::State::Connected:    st = "connected";    break;
        case ble_hid::State::Disconnected: st = "disconnected"; break;
    }
    ok("state=%s", st);
    return 0;
}

// --- migration ---
static int cmd_migration(int, char**) {
    auto* ctx = getContext();
    if (!ctx) { err(10, "no context"); return 1; }
    const char* r = "Unknown";
    switch (ctx->configMgr.lastMigration()) {
        case sdcard::MigrationResult::None:              r = "None";              break;
        case sdcard::MigrationResult::PathA:             r = "PathA";             break;
        case sdcard::MigrationResult::PathAHole:         r = "PathAHole";         break;
        case sdcard::MigrationResult::PathB:             r = "PathB";             break;
        case sdcard::MigrationResult::BeltAndSuspenders: r = "BeltAndSuspenders"; break;
    }
    ok("%s", r);
    return 0;
}

void registerIntrospectCommands() {
    const esp_console_cmd_t cmds[] = {
        {"stack",       "Dump ScreenStack bottom-up",         nullptr, cmd_stack,       nullptr, nullptr, nullptr},
        {"heap",        "Heap stats (free/min/dma_max)",      nullptr, cmd_heap,        nullptr, nullptr, nullptr},
        {"tasklist",    "FreeRTOS task list (vTaskList)",     nullptr, cmd_task_list,   nullptr, nullptr, nullptr},
        {"wifi-status", "WiFi connection state",              nullptr, cmd_wifi_status, nullptr, nullptr, nullptr},
        {"ssh-status",  "SSH client state",                   nullptr, cmd_ssh_status,  nullptr, nullptr, nullptr},
        {"ble-status",  "BLE HID host state",                 nullptr, cmd_ble_status,  nullptr, nullptr, nullptr},
        {"migration",   "Last migration result",              nullptr, cmd_migration,   nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
