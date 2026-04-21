#include "libssh_port.h"
#include "libssh/libssh.h"
#include "libssh/callbacks.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "libssh";

// libssh log priorities: 0=none, 1=warn, 2=info, 3=debug, 4=trace.
static void libssh_log_cb(int priority, const char *function, const char *buffer, void *userdata) {
    (void)userdata;
    if (priority <= 1) {
        ESP_LOGW(TAG, "[%s] %s", function ? function : "?", buffer ? buffer : "");
    } else {
        ESP_LOGI(TAG, "[%s] %s", function ? function : "?", buffer ? buffer : "");
    }
}

void libssh_port_init(void) {
    static bool inited = false;
    if (inited) return;
    inited = true;
    ssh_set_log_callback(libssh_log_cb);
    ssh_init();  // required once; libssh's mbedtls backend pulls entropy via ESP-IDF's mbedtls integration
}
