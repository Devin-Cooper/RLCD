// esp32_recovery/main/recovery_repl.cpp
// Minimal REPL over USB-JTAG CDC for factory-recovery firmware.
// Protocol: >>> OK <payload>  |  >>> ERR <code> <msg>  |  >>> DATA <line>

#include "recovery_repl.hpp"

#include "esp_console.h"
#include "esp_core_dump.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Response helpers — single REPL task, no mutex needed
// ---------------------------------------------------------------------------
namespace {

void ok(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs(">>> OK ", stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(ap);
}

void err(int code, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stdout, ">>> ERR %d ", code);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(ap);
}

void data(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs(">>> DATA ", stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(ap);
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

static int cmd_ping(int /*argc*/, char** /*argv*/) {
    ok("pong uptime=%lld", (long long)esp_timer_get_time());
    return 0;
}

static int cmd_info(int /*argc*/, char** /*argv*/) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const char* running_label = running ? running->label : "unknown";
    int subtype = running ? (int)running->subtype : -1;

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const char* state_str = "n/a";
    if (running) {
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
        // ESP_ERR_NOT_SUPPORTED → running from factory partition → state_str stays "n/a"
    }

    const esp_app_desc_t* desc = esp_app_get_description();
    char sha_hex[17] = {};
    for (int i = 0; i < 8; ++i) {
        snprintf(sha_hex + i * 2, 3, "%02x", desc->app_elf_sha256[i]);
    }

    const char* reset_str = "UNKNOWN";
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   reset_str = "POWERON"; break;
        case ESP_RST_EXT:       reset_str = "EXT"; break;
        case ESP_RST_SW:        reset_str = "SW"; break;
        case ESP_RST_PANIC:     reset_str = "PANIC"; break;
        case ESP_RST_INT_WDT:   reset_str = "INT_WDT"; break;
        case ESP_RST_TASK_WDT:  reset_str = "TASK_WDT"; break;
        case ESP_RST_WDT:       reset_str = "WDT"; break;
        case ESP_RST_DEEPSLEEP: reset_str = "DEEPSLEEP"; break;
        case ESP_RST_BROWNOUT:  reset_str = "BROWNOUT"; break;
        case ESP_RST_SDIO:      reset_str = "SDIO"; break;
        default: break;
    }

    ok("version=%s built=%s sha=%s running_label=%s subtype=%d state=%s reset=%s heap=%u",
       desc->version, desc->date, sha_hex,
       running_label, subtype, state_str, reset_str,
       (unsigned)esp_get_free_heap_size());
    return 0;
}

static int cmd_reboot(int /*argc*/, char** /*argv*/) {
    ok("%s", "");
    fflush(stdout);
    esp_restart();
    return 0;
}

static int cmd_reboot_ota(int /*argc*/, char** /*argv*/) {
    const esp_partition_t* p = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (!p) {
        err(1, "ota_0 not found");
        return 1;
    }
    esp_err_t e = esp_ota_set_boot_partition(p);
    if (e != ESP_OK) {
        err(2, "esp_ota_set_boot_partition failed: %d", (int)e);
        return 1;
    }
    ok("%s", "");
    fflush(stdout);
    esp_restart();
    return 0;
}

static int cmd_erase_nvs(int argc, char** argv) {
    if (argc < 2 || strcmp(argv[1], "--yes") != 0) {
        err(1, "refuse: require --yes");
        return 1;
    }
    esp_err_t e = nvs_flash_erase();
    if (e != ESP_OK) {
        err(2, "nvs_flash_erase failed: %d", (int)e);
        return 1;
    }
    e = nvs_flash_init();
    if (e != ESP_OK) {
        err(3, "nvs_flash_init failed: %d", (int)e);
        return 1;
    }
    ok("%s", "");
    return 0;
}

// Streams coredump in base64 DATA chunks then OK with total size.
static int cmd_coredump_dump(int /*argc*/, char** /*argv*/) {
    size_t addr = 0, size = 0;
    esp_err_t e = esp_core_dump_image_get(&addr, &size);
    if (e != ESP_OK) {
        err(4, "no coredump");
        return 1;
    }

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (!part) {
        err(5, "no coredump partition");
        return 1;
    }

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
            uint32_t v = (uint32_t)raw[i] << 16;
            size_t pad = 0;
            if (i + 1 < chunk) v |= (uint32_t)raw[i + 1] << 8; else pad++;
            if (i + 2 < chunk) v |= raw[i + 2];                 else pad++;
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

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
namespace recovery {

void startRepl() {
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "> ";
    repl_cfg.max_history_len = 16;

    esp_console_dev_usb_serial_jtag_config_t jtag_cfg =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    esp_console_repl_t* repl = nullptr;
    ESP_ERROR_CHECK(
        esp_console_new_repl_usb_serial_jtag(&jtag_cfg, &repl_cfg, &repl));

    // Field order (IDF 5.5): name, help, hint, func, argtable, func_w_context, context
    const esp_console_cmd_t cmds[] = {
        {"ping",         "Heartbeat — returns pong + uptime",           nullptr, cmd_ping,         nullptr, nullptr, nullptr},
        {"info",         "Firmware version, sha, partition, heap",      nullptr, cmd_info,         nullptr, nullptr, nullptr},
        {"reboot",       "Restart device",                              nullptr, cmd_reboot,       nullptr, nullptr, nullptr},
        {"reboot-ota",   "Set ota_0 boot partition then restart",       nullptr, cmd_reboot_ota,   nullptr, nullptr, nullptr},
        {"erase-nvs",    "Erase NVS flash (requires --yes)",            nullptr, cmd_erase_nvs,    nullptr, nullptr, nullptr},
        {"coredump-dump","Stream coredump in base64 DATA chunks + OK",  nullptr, cmd_coredump_dump,nullptr, nullptr, nullptr},
    };
    for (const auto& c : cmds) {
        esp_console_cmd_register(&c);
    }

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

} // namespace recovery
