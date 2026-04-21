// Host-only stub for test_ssh_keys_codec: implements
// fingerprint_from_pubkey_blob with OpenSSL so the host test links without
// pulling in libssh (which requires ESP-IDF). Firmware uses the mbedtls
// variant in key_codec.cpp.

#include "ssh_key_codec.hpp"

#include <cstring>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace ssh_keys {

std::string fingerprint_from_pubkey_blob(const uint8_t* blob, size_t len) {
    uint8_t digest[SHA256_DIGEST_LENGTH];
    SHA256(blob, len, digest);

    // base64, no padding
    char b64[64] = {};
    int n = EVP_EncodeBlock(reinterpret_cast<uint8_t*>(b64), digest, sizeof(digest));
    (void)n;
    // Strip trailing '=' padding
    size_t blen = std::strlen(b64);
    while (blen && b64[blen - 1] == '=') { b64[--blen] = '\0'; }
    std::string out = "SHA256:";
    out += b64;
    return out;
}

// fingerprint_of_ssh_key / pubkey_line / write_private_key_pem / write_public_key_file
// / ssh_key_to_type are firmware-only (libssh) — empty in host builds.

} // namespace ssh_keys
