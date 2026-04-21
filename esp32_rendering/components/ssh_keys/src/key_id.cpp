#include "ssh_key_types.hpp"

#include <cstring>
#include "esp_random.h"

namespace ssh_keys {

static char hex_digit(uint8_t nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

std::string KeyId::hex() const {
    std::string out;
    out.resize(32);
    for (size_t i = 0; i < 16; ++i) {
        out[2 * i]     = hex_digit((bytes[i] >> 4) & 0xF);
        out[2 * i + 1] = hex_digit(bytes[i] & 0xF);
    }
    return out;
}

KeyId KeyId::random() {
    KeyId id;
    esp_fill_random(id.bytes.data(), id.bytes.size());
    // Version/variant bits are not used (we don't parse as RFC 4122), but
    // we avoid the all-zeros sentinel which could collide with empty state.
    if (id.bytes == std::array<uint8_t, 16>{}) {
        id.bytes[0] = 1;
    }
    return id;
}

std::optional<KeyId> KeyId::parse(std::string_view hex) {
    if (hex.size() != 32) return std::nullopt;
    KeyId id;
    for (size_t i = 0; i < 16; ++i) {
        int hi = hex_value(hex[2 * i]);
        int lo = hex_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        id.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return id;
}

const char* key_type_glyph(KeyType type, uint16_t rsa_bits) {
    switch (type) {
        case KeyType::Ed25519:   return "ed2";
        case KeyType::EcdsaP256: return "es2";
        case KeyType::EcdsaP384: return "es3";
        case KeyType::EcdsaP521: return "es5";
        case KeyType::Rsa:
            if (rsa_bits >= 3072) return "r30";
            if (rsa_bits >= 2048) return "r20";
            return "rsa";
    }
    return "???";
}

} // namespace ssh_keys
