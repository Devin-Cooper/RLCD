#include "known_hosts_parser.hpp"

namespace ssh {

static std::string_view trim(std::string_view s) {
    size_t begin = 0;
    while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t')) ++begin;
    size_t end = s.size();
    while (end > begin &&
           (s[end - 1] == ' ' || s[end - 1] == '\t' ||
            s[end - 1] == '\r' || s[end - 1] == '\n')) --end;
    return s.substr(begin, end - begin);
}

std::optional<KnownHostsEntry> parse_known_hosts_entry(std::string_view line) {
    auto t = trim(line);
    if (t.empty() || t.front() == '#') return std::nullopt;

    std::vector<std::string> fields;
    std::string current;
    for (char c : t) {
        if (c == ' ' || c == '\t') {
            if (!current.empty()) { fields.push_back(std::move(current)); current.clear(); }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) fields.push_back(std::move(current));

    if (fields.size() < 3) return std::nullopt;

    KnownHostsEntry e;
    e.host = fields[0];
    e.key_type = fields[1];
    e.blob_b64 = fields[2];
    e.is_hashed = (e.host.size() >= 3 && e.host.substr(0, 3) == "|1|");
    if (fields.size() > 3) {
        for (size_t i = 3; i < fields.size(); ++i) {
            if (i > 3) e.comment.push_back(' ');
            e.comment += fields[i];
        }
    }
    return e;
}

std::vector<KnownHostsEntry> parse_known_hosts_file(std::string_view contents) {
    std::vector<KnownHostsEntry> out;
    size_t start = 0;
    while (start < contents.size()) {
        size_t end = contents.find('\n', start);
        if (end == std::string_view::npos) end = contents.size();
        auto parsed = parse_known_hosts_entry(contents.substr(start, end - start));
        if (parsed) out.push_back(*parsed);
        start = end + 1;
    }
    return out;
}

} // namespace ssh
