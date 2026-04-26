// Split out from command_registry.cpp so host tests can link the
// registry's "globals" half without dragging in all of ConfigManager.
//
// Host tests for the registry only exercise registerGlobals() and the
// kAllDispatchableIds array, so they link only command_registry.cpp.
// On-device firmware links both this TU and command_registry.cpp via
// the app component CMakeLists.

#include "command_registry.hpp"
#include "config_manager.hpp"
#include <cstring>
#include <cstdio>

namespace app {

namespace {
void put_dyn(Command& c, const char* title, const char* hint, uint16_t id) {
    std::strncpy(c.title, title, sizeof(c.title) - 1);
    c.title[sizeof(c.title) - 1] = '\0';
    std::strncpy(c.hint, hint, sizeof(c.hint) - 1);
    c.hint[sizeof(c.hint) - 1] = '\0';
    c.id = id;
}
} // namespace

void CommandRegistry::refreshDynamicServerCommands(const sdcard::ConfigManager& cfg) {
    dynamic_count_ = 0;
    int n = cfg.serverCount();
    if (n < 0) n = 0;
    if (n > static_cast<int>(kMaxDynamicServers))
        n = static_cast<int>(kMaxDynamicServers);
    for (int i = 0; i < n; ++i) {
        const auto& srv = cfg.getServer(i);
        // 40-byte dest, "Connect to: " is 12 + NUL → 27 bytes for the name.
        // Use %.*s with explicit precision to avoid format-truncation.
        char title[40];
        std::snprintf(title, sizeof(title), "Connect to: %.27s", srv.creds.name);
        put_dyn(dynamic_[dynamic_count_++], title, "",
                static_cast<uint16_t>(cmd_id::ConnectServerBase + i));
    }
}

} // namespace app
