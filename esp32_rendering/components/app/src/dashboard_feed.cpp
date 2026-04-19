#include "dashboard_feed.hpp"
#include <cstring>
#include <initializer_list>

namespace app {

static constexpr const char* SENTINEL = "__DASH_END__";
static constexpr int SENTINEL_LEN = 12;

FeedStatus feedChunk(FeedBuffer& buf, const uint8_t* data, std::size_t len) noexcept {
    for (std::size_t i = 0; i < len; ++i) {
        char ch = static_cast<char>(data[i]);

        if (buf.skip_echo) {
            if (ch == '\n') buf.skip_echo = false;
            continue;
        }

        if (buf.output_len < buf.capacity - 1) {
            buf.output[buf.output_len++] = ch;
            buf.output[buf.output_len] = '\0';
        }

        if (buf.output_len >= SENTINEL_LEN &&
            std::strncmp(buf.output + buf.output_len - SENTINEL_LEN,
                         SENTINEL, SENTINEL_LEN) == 0) {
            buf.output_len -= SENTINEL_LEN;
            while (buf.output_len > 0 &&
                   (buf.output[buf.output_len - 1] == '\n' ||
                    buf.output[buf.output_len - 1] == '\r' ||
                    buf.output[buf.output_len - 1] == ' ')) {
                --buf.output_len;
            }
            buf.output[buf.output_len] = '\0';

            const char* bp1 = "\x1b[?2004l";
            const char* bp2 = "\x1b[?2004h";
            const int   bpn = 8;
            for (const char* seq : {bp1, bp2}) {
                char* bp = std::strstr(buf.output, seq);
                while (bp) {
                    std::memmove(bp, bp + bpn,
                        buf.output_len - (bp - buf.output) - bpn + 1);
                    buf.output_len -= bpn;
                    bp = std::strstr(buf.output, seq);
                }
            }
            while (buf.output_len > 0 &&
                   (buf.output[0] == '\n' || buf.output[0] == '\r' || buf.output[0] == ' ')) {
                std::memmove(buf.output, buf.output + 1, buf.output_len);
                --buf.output_len;
            }
            return FeedStatus::Complete;
        }
    }
    return FeedStatus::Continue;
}

} // namespace app
