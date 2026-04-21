#pragma once

#include "ssh_keys.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

typedef struct ssh_key_struct* ssh_key;

namespace ssh_keys {

/// SHA-256(blob) -> "SHA256:<43-char-unpadded-base64>".
/// Host-test and firmware share the signature; host impl uses OpenSSL,
/// firmware impl uses mbedtls. Both in key_codec_host.cpp for tests only.
std::string fingerprint_from_pubkey_blob(const uint8_t* blob, size_t len);

/// On firmware only: given a libssh ssh_key, extract the raw pubkey blob
/// (11B algo string + N pubkey bytes for ed25519; similar for others) and
/// return its SHA-256-SHA256:<b64> fingerprint. Called by KeyStore::add.
std::string fingerprint_of_ssh_key(ssh_key k);

/// On firmware only: produce the "ssh-ed25519 AAAA... name\n" line for
/// display / QR / enroll. Caller provides the name (== KeyMeta.name).
std::string pubkey_line(ssh_key k, const char* name);

/// On firmware only: write PEM private key at `path` (atomic temp+rename).
/// Returns true on success.
bool write_private_key_pem(ssh_key k, const char* path);

/// On firmware only: derive the pubkey, then write the pubkey line file at `path`.
bool write_public_key_file(ssh_key k, const char* name, const char* path);

/// On firmware only: return (KeyType, rsa_bits) for a libssh ssh_key.
/// rsa_bits is 0 for non-RSA. Returns KeyType::Ed25519 as fallback on unknown.
KeyType ssh_key_to_type(ssh_key k, uint16_t& out_rsa_bits);

} // namespace ssh_keys
