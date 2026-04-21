#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ssh_keys {

/// 16-byte UUID rendered as 32 lowercase hex chars (no dashes) for paths / ids.
struct KeyId {
    std::array<uint8_t, 16> bytes{};

    /// 32-char lowercase hex; never empty (all zeros if default-constructed).
    std::string hex() const;

    /// Backed by esp_fill_random. Callers must gate by Wi-Fi-up (see key_gen.cpp).
    static KeyId random();

    /// Parses 32 hex chars (case-insensitive). Returns nullopt on any non-hex char
    /// or wrong length.
    static std::optional<KeyId> parse(std::string_view hex);

    bool operator==(const KeyId&) const = default;
};

/// Supported key types. On-device generation is Ed25519-only; the others are
/// parsed on import via libssh's PEM parser.
enum class KeyType : uint8_t {
    Ed25519    = 1,
    EcdsaP256  = 2,
    EcdsaP384  = 3,
    EcdsaP521  = 4,
    Rsa        = 5,
};

/// Render a KeyType as a short lowercase glyph ("ed2", "es2", "r20", etc.)
/// matching the algo-glyph convention in SshKeyListScreen.
/// RSA also needs rsa_bits to choose "r20" / "r30" / ...; pass 0 for non-RSA.
const char* key_type_glyph(KeyType type, uint16_t rsa_bits);

} // namespace ssh_keys
