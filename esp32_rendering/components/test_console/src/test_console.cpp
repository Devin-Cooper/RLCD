#include "test_console.hpp"
#include "test_console_response.hpp"
#include "test_console_context.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_console.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

#include <cstdarg>
#include <cstdio>

static const char* TAG = "test_console";

namespace test_console {

static Context* s_ctx = nullptr;
Context* getContext() { return s_ctx; }
void setContext(Context* ctx) { s_ctx = ctx; }

static SemaphoreHandle_t s_response_mutex = nullptr;

static void emit_line(const char* prefix, const char* fmt, va_list ap) {
    if (!s_response_mutex) return;
    if (xSemaphoreTake(s_response_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "response mutex timeout");
        return;
    }
    fputs(prefix, stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
    xSemaphoreGive(s_response_mutex);
}

void ok(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    emit_line(">>> OK ", fmt, ap);
    va_end(ap);
}

void err(int code, const char* fmt, ...) {
    if (!s_response_mutex) return;
    if (xSemaphoreTake(s_response_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    fprintf(stdout, ">>> ERR %d ", code);
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
    xSemaphoreGive(s_response_mutex);
}

void data(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    emit_line(">>> DATA ", fmt, ap);
    va_end(ap);
}

// Forward decls for command groups — registered in later tasks.
void registerRuntimeCommands();
void registerIntrospectCommands();
void registerInjectionCommands();
void registerNvsCommands();

void init(Context& ctx) {
#if !CONFIG_TEST_CONSOLE_ENABLED
    (void)ctx;
    return;
#else
    setContext(&ctx);

    if (!s_response_mutex) {
        s_response_mutex = xSemaphoreCreateMutex();
    }

    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "> ";
    repl_config.max_history_len = CONFIG_TEST_CONSOLE_HISTORY_LEN;
    repl_config.task_stack_size = 8192;
    repl_config.task_priority = 5;
    repl_config.task_core_id = 0;

    esp_console_dev_uart_config_t uart_cfg =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    uart_cfg.channel = CONFIG_ESP_CONSOLE_UART_NUM;
    uart_cfg.baud_rate = CONFIG_TEST_CONSOLE_BAUD;

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_config, &repl));

    esp_console_register_help_command();
    registerRuntimeCommands();
    registerIntrospectCommands();
    registerInjectionCommands();
    registerNvsCommands();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "test_console started on UART%d @ %d baud",
             (int)CONFIG_ESP_CONSOLE_UART_NUM, (int)CONFIG_TEST_CONSOLE_BAUD);
#endif
}

} // namespace test_console
