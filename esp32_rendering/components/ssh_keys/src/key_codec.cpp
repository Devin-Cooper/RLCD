#include "ssh_key_codec.hpp"
#include "ssh_key_export.hpp"
#include "ssh_keys.hpp"
#include "ssh_keys_index.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "esp_log.h"
#include "esp_random.h"
#include "libssh/libssh.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"

// ssh_key_size() lives in libssh/pki.h (semi-public header). Including pki.h
// here triggers a C++ -Wchanges-meaning error on the internal ssh_key_struct.
// Forward-declare the single symbol we need instead. Flagged per code-review H3.
extern "C" int ssh_key_size(ssh_key key);

static const char* TAG = "ssh_keys";
static constexpr const char* KEYS_DIR_PATH = "/littlefs/ssh_keys";

namespace ssh_keys {

// --- fingerprint helpers ---

std::string fingerprint_from_pubkey_blob(const uint8_t* blob, size_t len) {
    uint8_t digest[32];
    mbedtls_sha256(blob, len, digest, 0);
    unsigned char b64[64] = {};
    size_t olen = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &olen, digest, sizeof(digest));
    // Strip '=' padding
    while (olen && b64[olen - 1] == '=') b64[--olen] = 0;
    std::string out = "SHA256:";
    out.append(reinterpret_cast<const char*>(b64), olen);
    return out;
}

bool fp_sha256_b64(const std::array<uint8_t, 32>& fp, char* out, size_t cap) {
    if (!out || cap < 53) return false;
    unsigned char b64[48] = {};
    size_t olen = 0;
    if (mbedtls_base64_encode(b64, sizeof(b64), &olen,
                               fp.data(), fp.size()) != 0) {
        return false;
    }
    // ssh-keygen drops the two '=' pad chars from the SHA256:<fp> output;
    // mbedtls adds them. Strip any trailing '='.
    while (olen > 0 && b64[olen - 1] == '=') { b64[--olen] = '\0'; }
    int n = std::snprintf(out, cap, "SHA256:%s", b64);
    return n > 0 && static_cast<size_t>(n) < cap;
}

std::string fingerprint_of_ssh_key(ssh_key k) {
    unsigned char* hash = nullptr;
    size_t hash_len = 0;
    if (ssh_get_publickey_hash(k, SSH_PUBLICKEY_HASH_SHA256, &hash, &hash_len) != 0) {
        return {};
    }
    unsigned char b64[64] = {};
    size_t olen = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &olen, hash, hash_len);
    while (olen && b64[olen - 1] == '=') b64[--olen] = 0;
    ssh_clean_pubkey_hash(&hash);
    std::string out = "SHA256:";
    out.append(reinterpret_cast<const char*>(b64), olen);
    return out;
}

// --- pubkey line ---

std::string pubkey_line(ssh_key k, const char* name) {
    ssh_key pub = nullptr;
    if (ssh_pki_export_privkey_to_pubkey(k, &pub) != SSH_OK) return {};
    char* b64 = nullptr;
    if (ssh_pki_export_pubkey_base64(pub, &b64) != SSH_OK) {
        ssh_key_free(pub);
        return {};
    }
    const char* algo = ssh_key_type_to_char(ssh_key_type(pub));
    std::string out;
    if (algo && b64) {
        out.reserve(std::strlen(algo) + 1 + std::strlen(b64) + 1 + std::strlen(name) + 1);
        out.append(algo).append(" ").append(b64).append(" ").append(name);
    }
    if (b64) ssh_string_free_char(b64);
    ssh_key_free(pub);
    return out;
}

// --- atomic file writers ---

static bool rename_atomic(const char* tmp, const char* final_path) {
    if (rename(tmp, final_path) != 0) { unlink(tmp); return false; }
    return true;
}

bool write_private_key_pem(ssh_key k, const char* path) {
    char tmp[96];
    std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int rc = ssh_pki_export_privkey_file(k, nullptr, nullptr, nullptr, tmp);
    if (rc != SSH_OK) { unlink(tmp); return false; }
    return rename_atomic(tmp, path);
}

bool write_public_key_file(ssh_key k, const char* name, const char* path) {
    char tmp[96];
    std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    auto line = pubkey_line(k, name);
    if (line.empty()) return false;
    line.push_back('\n');
    FILE* f = std::fopen(tmp, "wb");
    if (!f) return false;
    size_t n = std::fwrite(line.data(), 1, line.size(), f);
    std::fclose(f);
    if (n != line.size()) { unlink(tmp); return false; }
    return rename_atomic(tmp, path);
}

// --- KeyType classification ---

KeyType ssh_key_to_type(ssh_key k, uint16_t& out_rsa_bits) {
    out_rsa_bits = 0;
    switch (ssh_key_type(k)) {
        case SSH_KEYTYPE_ED25519:
            return KeyType::Ed25519;
        case SSH_KEYTYPE_ECDSA_P256:
            return KeyType::EcdsaP256;
        case SSH_KEYTYPE_ECDSA_P384:
            return KeyType::EcdsaP384;
        case SSH_KEYTYPE_ECDSA_P521:
            return KeyType::EcdsaP521;
        case SSH_KEYTYPE_RSA:
            out_rsa_bits = static_cast<uint16_t>(ssh_key_size(k));
            return KeyType::Rsa;
        default:
            return KeyType::Ed25519;  // should not happen after import filter
    }
}

// --- KeyStore::add ---

KeyId KeyStore::add(const KeyMeta& tmpl, ssh_key priv) {
    if (static_cast<int>(keys_.size()) >= max_keys()) {
        ESP_LOGW(TAG, "add: index full (%d/%d)", (int)keys_.size(), max_keys());
        return {};
    }

    KeyMeta meta = tmpl;
    meta.id = KeyId::random();
    // Ensure no collision with existing ids (astronomically unlikely, but cheap).
    while (contains(meta.id)) meta.id = KeyId::random();
    meta.use_count = 0;

    uint16_t rsa_bits = 0;
    meta.type = ssh_key_to_type(priv, rsa_bits);
    meta.rsa_bits = rsa_bits;

    unsigned char* hash = nullptr;
    size_t hash_len = 0;
    if (ssh_get_publickey_hash(priv, SSH_PUBLICKEY_HASH_SHA256, &hash, &hash_len) == 0 &&
        hash && hash_len == 32) {
        std::memcpy(meta.fp_sha256.data(), hash, 32);
    }
    if (hash) ssh_clean_pubkey_hash(&hash);

    char priv_path[96], pub_path[96];
    std::snprintf(priv_path, sizeof(priv_path), "%s/%s",     KEYS_DIR_PATH, meta.id.hex().c_str());
    std::snprintf(pub_path,  sizeof(pub_path),  "%s/%s.pub", KEYS_DIR_PATH, meta.id.hex().c_str());

    if (!write_private_key_pem(priv, priv_path)) { ESP_LOGE(TAG, "write priv failed"); return {}; }
    if (!write_public_key_file(priv, meta.name, pub_path)) {
        ESP_LOGE(TAG, "write pub failed");
        unlink(priv_path);
        return {};
    }

    auto snapshot = keys_;
    keys_.push_back(meta);
    if (!serialize_to_nvs()) {
        ESP_LOGE(TAG, "add: nvs commit failed; rollback");
        keys_ = snapshot;
        unlink(priv_path);
        unlink(pub_path);
        return {};
    }
    // Re-sort after insert
    std::sort(keys_.begin(), keys_.end(), [](const KeyMeta& a, const KeyMeta& b) {
        if (a.created_utc != b.created_utc) return a.created_utc > b.created_utc;
        return std::strcmp(a.name, b.name) < 0;
    });
    auto fp = fingerprint_of_ssh_key(priv);
    ESP_LOGI(TAG, "Added %s: %s", meta.id.hex().c_str(), fp.c_str());
    return meta.id;
}

} // namespace ssh_keys
