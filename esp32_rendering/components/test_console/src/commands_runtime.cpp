// esp32_rendering/components/test_console/src/commands_runtime.cpp
#include "test_console_response.hpp"

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>
#include <cstdlib>

namespace test_console {

// --- ping ---
static int cmd_ping(int, char**) {
    ok("%lld", (long long)esp_timer_get_time());
    return 0;
}

// --- uptime ---
static int cmd_uptime(int, char**) {
    ok("%lld", (long long)esp_timer_get_time());
    return 0;
}

// --- log level <tag> <level> ---
static int cmd_log_level(int argc, char** argv) {
    if (argc != 3) { err(1, "usage: log level <tag> <level>"); return 1; }
    const char* tag = argv[1];
    const char* lvl = argv[2];
    esp_log_level_t l;
    if      (!strcmp(lvl, "none"))    l = ESP_LOG_NONE;
    else if (!strcmp(lvl, "error"))   l = ESP_LOG_ERROR;
    else if (!strcmp(lvl, "warn"))    l = ESP_LOG_WARN;
    else if (!strcmp(lvl, "info"))    l = ESP_LOG_INFO;
    else if (!strcmp(lvl, "debug"))   l = ESP_LOG_DEBUG;
    else if (!strcmp(lvl, "verbose")) l = ESP_LOG_VERBOSE;
    else { err(2, "unknown level '%s'", lvl); return 1; }
    esp_log_level_set(tag, l);
    ok("%s", "");
    return 0;
}

// --- reboot ---
static int cmd_reboot(int, char**) {
    ok("%s", "");
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_restart();
    return 0;
}

// --- crash ---
static int cmd_crash(int, char**) {
    abort();
    return 0;
}

// --- coredump check ---
static int cmd_coredump_check(int, char**) {
    esp_err_t e = esp_core_dump_image_check();
    if (e == ESP_OK) {
        size_t addr = 0, size = 0;
        esp_core_dump_image_get(&addr, &size);
        ok("yes %zu", size);
    } else if (e == ESP_ERR_NOT_FOUND || e == ESP_ERR_INVALID_SIZE) {
        ok("no");
    } else {
        err(3, "check failed: %d", (int)e);
    }
    return 0;
}

// --- coredump read — streams base64 DATA lines ---
static int cmd_coredump_read(int, char**) {
    size_t addr = 0, size = 0;
    esp_err_t e = esp_core_dump_image_get(&addr, &size);
    if (e != ESP_OK) { err(4, "no coredump"); return 1; }

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (!part) { err(5, "no coredump partition"); return 1; }

    static const char B64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    uint8_t  raw[384];
    char     enc[520];
    size_t remaining = size;
    size_t offset = 0;
    while (remaining > 0) {
        size_t chunk = remaining > sizeof(raw) ? sizeof(raw) : remaining;
        if (esp_partition_read(part, offset, raw, chunk) != ESP_OK) {
            err(6, "read failed @%zu", offset);
            return 1;
        }
        size_t j = 0;
        for (size_t i = 0; i < chunk; i += 3) {
            uint32_t v = raw[i] << 16;
            size_t pad = 0;
            if (i + 1 < chunk) v |= raw[i + 1] << 8; else pad++;
            if (i + 2 < chunk) v |= raw[i + 2];     else pad++;
            enc[j++] = B64[(v >> 18) & 0x3f];
            enc[j++] = B64[(v >> 12) & 0x3f];
            enc[j++] = pad >= 2 ? '=' : B64[(v >> 6) & 0x3f];
            enc[j++] = pad >= 1 ? '=' : B64[ v       & 0x3f];
        }
        enc[j] = '\0';
        data("%s", enc);
        offset += chunk;
        remaining -= chunk;
    }
    ok("%zu", size);
    return 0;
}

// --- coredump erase ---
static int cmd_coredump_erase(int, char**) {
    esp_err_t e = esp_core_dump_image_erase();
    if (e == ESP_OK) ok("%s", "");
    else err(7, "erase failed: %d", (int)e);
    return 0;
}

// --- ota-info ---
static int cmd_ota_info(int argc, char** argv) {
    (void)argc; (void)argv;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) { err(1, "no running partition"); return 1; }

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const char* state_str = "n/a";
    esp_err_t se = esp_ota_get_state_partition(running, &state);
    if (se == ESP_OK) {
        switch (state) {
            case ESP_OTA_IMG_NEW:            state_str = "NEW"; break;
            case ESP_OTA_IMG_PENDING_VERIFY: state_str = "PENDING"; break;
            case ESP_OTA_IMG_VALID:          state_str = "VALID"; break;
            case ESP_OTA_IMG_INVALID:        state_str = "INVALID"; break;
            case ESP_OTA_IMG_ABORTED:        state_str = "ABORTED"; break;
            case ESP_OTA_IMG_UNDEFINED:
            default:                          state_str = "UNDEFINED"; break;
        }
    }
    // se == ESP_ERR_NOT_SUPPORTED when running from factory — state_str stays "n/a"

    const esp_app_desc_t* desc = esp_app_get_description();
    char sha_hex[17] = {};
    for (int i = 0; i < 8; ++i) {
        snprintf(sha_hex + i*2, 3, "%02x", desc->app_elf_sha256[i]);
    }

    ok("running_label=%s subtype=%d size=%u state=%s version=%s sha=%s",
       running->label, (int)running->subtype, (unsigned)running->size,
       state_str, desc->version, sha_hex);
    return 0;
}

void registerRuntimeCommands() {
    // Field order: command, help, hint, func, argtable, func_w_context, context
    const esp_console_cmd_t cmds[] = {
        {"ping",            "Heartbeat (returns uptime us)",       nullptr, cmd_ping,           nullptr, nullptr, nullptr},
        {"uptime",          "Microseconds since boot",             nullptr, cmd_uptime,         nullptr, nullptr, nullptr},
        {"log",             "log level <tag> <level>",             nullptr, cmd_log_level,      nullptr, nullptr, nullptr},
        {"reboot",          "esp_restart()",                       nullptr, cmd_reboot,         nullptr, nullptr, nullptr},
        {"crash",           "deliberate abort() for crash tests",  nullptr, cmd_crash,          nullptr, nullptr, nullptr},
        {"coredump-check",  "coredump check — present or not",     nullptr, cmd_coredump_check, nullptr, nullptr, nullptr},
        {"coredump-read",   "coredump read — base64 stream",       nullptr, cmd_coredump_read,  nullptr, nullptr, nullptr},
        {"coredump-erase",  "coredump erase",                      nullptr, cmd_coredump_erase, nullptr, nullptr, nullptr},
        {"ota-info",        "Running partition + otadata state + app version/sha", nullptr, cmd_ota_info, nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) esp_console_cmd_register(&c);
}

} // namespace test_console
