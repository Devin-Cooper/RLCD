#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ssh {

struct KnownHostsEntry {
    std::string host;
    std::string key_type;
    std::string blob_b64;
    std::string comment;
    bool is_hashed;
};

/// Parse one line. Returns nullopt on blank lines, comment lines, or malformed ones.
std::optional<KnownHostsEntry> parse_known_hosts_entry(std::string_view line);

/// Parse a whole file body.
std::vector<KnownHostsEntry> parse_known_hosts_file(std::string_view contents);

} // namespace ssh
