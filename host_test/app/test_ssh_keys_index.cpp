// host_test/app/test_ssh_keys_index.cpp
#include <catch2/catch_test_macros.hpp>

#include "ssh_keys_index.hpp"  // pure-C++ helper, extracted from key_store.cpp for host testability

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace ssh_keys;

static KeyMeta make_meta(uint8_t tag, const char* name) {
    KeyMeta m;
    m.id.bytes.fill(tag);
    m.type = KeyType::Ed25519;
    m.rsa_bits = 0;
    m.created_utc = 1'000'000 + tag;
    m.use_count = tag;
    std::strncpy(m.name, name, sizeof(m.name) - 1);
    for (size_t i = 0; i < 32; ++i) m.fp_sha256[i] = static_cast<uint8_t>(i ^ tag);
    return m;
}

TEST_CASE("empty index roundtrip", "[ssh_keys_index]") {
    std::vector<KeyMeta> empty;
    auto blob = index_serialize(empty);
    REQUIRE(blob.size() >= 16);  // IndexHeader

    auto out = index_deserialize(blob);
    REQUIRE(out.has_value());
    REQUIRE(out->empty());
}

TEST_CASE("one key roundtrip preserves all fields", "[ssh_keys_index]") {
    std::vector<KeyMeta> in = { make_meta(0x42, "my-key") };
    auto blob = index_serialize(in);
    auto out = index_deserialize(blob);
    REQUIRE(out.has_value());
    REQUIRE(out->size() == 1);
    const auto& got = (*out)[0];
    REQUIRE(got.id == in[0].id);
    REQUIRE(std::string(got.name) == "my-key");
    REQUIRE(got.type == KeyType::Ed25519);
    REQUIRE(got.created_utc == 1'000'066);
    REQUIRE(got.use_count == 0x42);
    REQUIRE(got.fp_sha256 == in[0].fp_sha256);
}

TEST_CASE("32 keys roundtrip preserves order", "[ssh_keys_index]") {
    std::vector<KeyMeta> in;
    for (int i = 0; i < 32; ++i) {
        char n[32];
        std::snprintf(n, sizeof(n), "key-%02d", i);
        in.push_back(make_meta(static_cast<uint8_t>(i + 1), n));
    }
    auto blob = index_serialize(in);
    // 112 bytes per record × 32 records + 16-byte header = 3600 bytes
    REQUIRE(blob.size() == 16 + 32 * 112);
    auto out = index_deserialize(blob);
    REQUIRE(out.has_value());
    REQUIRE(out->size() == 32);
    for (int i = 0; i < 32; ++i) {
        REQUIRE((*out)[i].id == in[i].id);
    }
}

TEST_CASE("garbage blob returns nullopt", "[ssh_keys_index]") {
    std::vector<uint8_t> bad = { 0xFF, 0x00, 0x00 };
    REQUIRE_FALSE(index_deserialize(bad).has_value());
}

TEST_CASE("version mismatch returns nullopt", "[ssh_keys_index]") {
    std::vector<KeyMeta> in = { make_meta(1, "k") };
    auto blob = index_serialize(in);
    blob[0] = 2;  // bump version byte
    REQUIRE_FALSE(index_deserialize(blob).has_value());
}

TEST_CASE("rsa key preserves rsa_bits", "[ssh_keys_index]") {
    KeyMeta m = make_meta(7, "rsa-k");
    m.type = KeyType::Rsa;
    m.rsa_bits = 2048;
    std::vector<KeyMeta> in = { m };
    auto blob = index_serialize(in);
    auto out = index_deserialize(blob);
    REQUIRE(out.has_value());
    REQUIRE((*out)[0].type == KeyType::Rsa);
    REQUIRE((*out)[0].rsa_bits == 2048);
}
