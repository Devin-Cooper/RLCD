#pragma once

#include "ssh_key_types.hpp"

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>

// Forward-declare libssh types so this header doesn't drag libssh deps into
// every caller — opaque pointers only. Callers that actually invoke libssh
// include libssh/libssh.h themselves.
typedef struct ssh_key_struct* ssh_key;

namespace ssh_keys {

struct KeyMeta {
    KeyId    id;
    char     name[32] = {};
    KeyType  type = KeyType::Ed25519;
    uint16_t rsa_bits = 0;
    uint64_t created_utc = 0;
    std::array<uint8_t, 32> fp_sha256{};
    uint32_t use_count = 0;
};

/// Managed store: NVS single-blob index + LittleFS PEM / .pub blobs.
///
/// **Thread safety:** not internally synchronized. In the current firmware,
/// both the main (UI) task and the esp_console REPL task reach into KeyStore,
/// so external serialization will be required once UI screens start writing
/// (Phase 13). Until then: the REPL is the sole writer; reads from main are
/// safe because no concurrent mutation exists. A future commit (tracked as
/// part of Phase 16's grep-audit) must add explicit locking before any UI
/// screen in Phase 13 calls a mutating KeyStore method.
class KeyStore {
public:
    KeyStore();
    ~KeyStore();
    KeyStore(const KeyStore&) = delete;
    KeyStore& operator=(const KeyStore&) = delete;

    /// Load index from NVS and sweep any orphan `.tmp` / index-less LittleFS
    /// files. Safe to call multiple times. Returns true on success.
    bool init();

    /// Sorted by created_utc desc, then name asc (for stable list UI).
    const std::vector<KeyMeta>& keys() const { return keys_; }

    /// Resolve an ssh_key_id hex string to its LittleFS private-key path.
    /// Returns true + fills out_path on hit; false otherwise.
    /// Thread-safe for read (no mutation; callers must not delete concurrently).
    bool path_for(const char* ssh_key_id, char* out_path, size_t cap) const;

    /// Same as path_for but for the .pub file. Keeps the layout constant in
    /// one place so export_text/export_qr don't hardcode paths.
    bool pub_path_for(const char* ssh_key_id, char* out_path, size_t cap) const;

    /// True if any stored key matches; used by list/picker UI.
    bool contains(const KeyId& id) const;

    /// Lookup by id; returns nullptr if not present.
    const KeyMeta* find(const KeyId& id) const;

    /// Add a key. Both PEM + .pub must already be derivable from `priv`.
    /// Writes both files atomically, then commits the index. Returns the
    /// generated KeyId on success, or empty id on any failure.
    /// Caller owns `priv` (libssh ssh_key); this function reads from it and
    /// does NOT free it.
    KeyId add(const KeyMeta& meta_template, ssh_key priv);

    /// Rename. Returns false on id-not-found, collision, or NVS write failure.
    bool rename(const KeyId& id, const char* new_name);

    /// Delete. Returns false if key is referenced by a server (caller provides
    /// that check via a callback to avoid coupling this component to
    /// sdcard_config). Deletes NVS entry first (point of no return), then
    /// unlinks LittleFS files.
    ///
    /// `is_referenced_by_server(id, user_data) -> list of server names` is
    /// called with the target id. Returning a non-empty list aborts the
    /// delete.
    using ReferenceCheck = std::vector<const char*> (*)(const KeyId& id, void* user_data);
    bool delete_key(const KeyId& id, ReferenceCheck check, void* user_data);

    /// Show plaintext-warning once per device. Returns true if the caller
    /// should show the modal (first time); sets the sentinel to 1.
    bool warn_plaintext_needed();

    /// Hard cap from Kconfig; accessible to UI for the "index full" Modal.
    int max_keys() const;

private:
    std::vector<KeyMeta> keys_;
    bool serialize_to_nvs();
    bool deserialize_from_nvs();
    void sweep_orphans();
};

} // namespace ssh_keys
