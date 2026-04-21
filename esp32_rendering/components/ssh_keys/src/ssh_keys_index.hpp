#pragma once

#include "ssh_keys.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ssh_keys {

// Packed 112-byte record. Spec §Data model pins this with static_assert.
struct __attribute__((packed)) KeyMetaRecord {
    uint8_t  id[16];
    uint8_t  type;
    uint8_t  pad0;
    uint16_t rsa_bits;
    uint8_t  pad1[4];
    uint8_t  fp_sha256[32];
    uint64_t created_utc;
    uint32_t use_count;
    char     name[32];
    uint16_t name_len;
    uint8_t  reserved[2];
    uint8_t  tail_pad[8];
};
static_assert(sizeof(KeyMetaRecord) == 112, "KeyMetaRecord layout drift");

// 16-byte header: version(1) + count(2) + reserved(13)
struct __attribute__((packed)) IndexHeader {
    uint8_t  version;
    uint16_t count;
    uint8_t  reserved[13];
};
static_assert(sizeof(IndexHeader) == 16, "IndexHeader layout drift");

constexpr uint8_t INDEX_VERSION = 1;

/// Serialize a sorted list of KeyMeta to a raw byte blob for NVS storage.
/// Output is `IndexHeader` + count * KeyMetaRecord.
std::vector<uint8_t> index_serialize(const std::vector<KeyMeta>& keys);

/// Parse a raw byte blob. Returns nullopt on bad header/version/size.
std::optional<std::vector<KeyMeta>> index_deserialize(const std::vector<uint8_t>& blob);

} // namespace ssh_keys
