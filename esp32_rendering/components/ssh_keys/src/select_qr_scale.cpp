#include "ssh_key_export.hpp"

namespace ssh_keys {

int select_qr_scale(int modules) {
    if (modules <= 0) return 0;
    int quiet = 8;                        // 4-module quiet zone on each side
    int budget = 300 / (modules + quiet); // floor division
    if (budget < 1) return 0;
    if (budget > 6) return 6;
    return budget;
}

} // namespace ssh_keys
