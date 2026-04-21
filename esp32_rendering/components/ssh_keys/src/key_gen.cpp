#include "ssh_keys.hpp"
#include "ssh_key_codec.hpp"

#include "esp_log.h"
#include "libssh/libssh.h"
// libssh/pki.h triggers a C++ -Wchanges-meaning error on its internal
// ssh_key_struct (typedef-name clashes with field-name). libssh/libssh.h
// already exposes ssh_pki_generate, SSH_KEYTYPE_ED25519, and ssh_key_free,
// so pki.h is not required here.
#include "wifi_manager.hpp"

static const char* TAG = "ssh_keys";

namespace ssh_keys {

enum class GenerateResult : uint8_t {
    Ok = 0,
    WifiDown,
    LibsshError,
    StorePushFailed,
    IndexFull,
};

/// Generate an Ed25519 key, add it to the store, return result + filled-out id.
/// On success, `out_id` is populated; on failure, caller surfaces the result.
GenerateResult generate_ed25519(KeyStore& store, wifi::WifiManager& wifi,
                                const char* name, uint64_t now_utc,
                                KeyId& out_id) {
    if (wifi.connectionInfo().state != wifi::State::Connected) {
        return GenerateResult::WifiDown;
    }
    if (static_cast<int>(store.keys().size()) >= store.max_keys()) {
        return GenerateResult::IndexFull;
    }

    ssh_key k = nullptr;
    int rc = ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, &k);
    if (rc != SSH_OK || k == nullptr) {
        ESP_LOGE(TAG, "ssh_pki_generate: %d", rc);
        return GenerateResult::LibsshError;
    }

    KeyMeta tmpl;
    size_t nlen = 0;
    while (nlen < sizeof(tmpl.name) - 1 && name[nlen] != '\0') { tmpl.name[nlen] = name[nlen]; ++nlen; }
    tmpl.name[nlen] = '\0';
    tmpl.type = KeyType::Ed25519;
    tmpl.rsa_bits = 0;
    tmpl.created_utc = now_utc;

    out_id = store.add(tmpl, k);
    ssh_key_free(k);
    if (out_id == KeyId{}) {
        return GenerateResult::StorePushFailed;
    }
    return GenerateResult::Ok;
}

} // namespace ssh_keys
