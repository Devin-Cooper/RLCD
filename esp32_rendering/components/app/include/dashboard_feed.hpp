#pragma once

#include <cstdint>
#include <cstddef>

namespace app {

/// Caller-owned output buffer + state for feedChunk.
struct FeedBuffer {
    char*   output;       // buffer of length `capacity`
    int     output_len;   // current length, < capacity
    int     capacity;     // MAX_OUTPUT_LEN
    bool    skip_echo;    // true while dropping the echoed command line
};

enum class FeedStatus : uint8_t {
    Continue,    // bytes absorbed; no sentinel yet
    Complete,    // sentinel hit; output trimmed + paste-escape stripped
};

/// Pure: feed one chunk of SSH output into the buffer, scanning for the
/// `__DASH_END__` sentinel. Returns Complete once the sentinel is seen.
/// On Complete, trims sentinel + trailing whitespace, strips bracketed-paste
/// escape sequences, and trims leading whitespace.
///
/// Current behavior: when `output_len` reaches `capacity - 1`, further bytes
/// are dropped and the sentinel scan can never match — the caller hangs on
/// this command. Spec 04 replaces this with graceful overflow handling.
FeedStatus feedChunk(FeedBuffer& buf, const uint8_t* data, std::size_t len) noexcept;

} // namespace app
