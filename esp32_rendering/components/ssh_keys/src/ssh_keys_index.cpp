#include "ssh_keys_index.hpp"

#include <cstring>

namespace ssh_keys {

static KeyMetaRecord meta_to_record(const KeyMeta& m) {
    KeyMetaRecord r{};
    std::memcpy(r.id, m.id.bytes.data(), 16);
    r.type = static_cast<uint8_t>(m.type);
    r.rsa_bits = m.rsa_bits;
    std::memcpy(r.fp_sha256, m.fp_sha256.data(), 32);
    r.created_utc = m.created_utc;
    r.use_count = m.use_count;
    size_t nlen = 0;
    while (nlen < sizeof(r.name) && m.name[nlen] != '\0') ++nlen;
    r.name_len = static_cast<uint16_t>(nlen);
    std::memcpy(r.name, m.name, nlen);
    return r;
}

static KeyMeta record_to_meta(const KeyMetaRecord& r) {
    KeyMeta m;
    std::memcpy(m.id.bytes.data(), r.id, 16);
    m.type = static_cast<KeyType>(r.type);
    m.rsa_bits = r.rsa_bits;
    std::memcpy(m.fp_sha256.data(), r.fp_sha256, 32);
    m.created_utc = r.created_utc;
    m.use_count = r.use_count;
    size_t nlen = r.name_len < sizeof(m.name) - 1 ? r.name_len : sizeof(m.name) - 1;
    std::memcpy(m.name, r.name, nlen);
    m.name[nlen] = '\0';
    return m;
}

std::vector<uint8_t> index_serialize(const std::vector<KeyMeta>& keys) {
    IndexHeader hdr{};
    hdr.version = INDEX_VERSION;
    hdr.count = static_cast<uint16_t>(keys.size());

    std::vector<uint8_t> out(sizeof(hdr) + keys.size() * sizeof(KeyMetaRecord));
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    for (size_t i = 0; i < keys.size(); ++i) {
        KeyMetaRecord r = meta_to_record(keys[i]);
        std::memcpy(out.data() + sizeof(hdr) + i * sizeof(r), &r, sizeof(r));
    }
    return out;
}

std::optional<std::vector<KeyMeta>> index_deserialize(const std::vector<uint8_t>& blob) {
    if (blob.size() < sizeof(IndexHeader)) return std::nullopt;
    IndexHeader hdr;
    std::memcpy(&hdr, blob.data(), sizeof(hdr));
    if (hdr.version != INDEX_VERSION) return std::nullopt;
    size_t expected = sizeof(IndexHeader) + size_t(hdr.count) * sizeof(KeyMetaRecord);
    if (blob.size() != expected) return std::nullopt;

    std::vector<KeyMeta> out;
    out.reserve(hdr.count);
    for (uint16_t i = 0; i < hdr.count; ++i) {
        KeyMetaRecord r;
        std::memcpy(&r, blob.data() + sizeof(hdr) + i * sizeof(r), sizeof(r));
        out.push_back(record_to_meta(r));
    }
    return out;
}

} // namespace ssh_keys
