#include "command_registry.hpp"
#include "config_manager.hpp"
#include <cstring>
#include <cstdio>

namespace app {

const std::array<uint16_t, 29> kAllDispatchableIds = {
    cmd_id::OpenDashboard, cmd_id::OpenTerminal, cmd_id::WifiNetworks,
    cmd_id::Servers, cmd_id::AddServer, cmd_id::SshKeys, cmd_id::Settings,
    cmd_id::PairKeyboard, cmd_id::ForgetAllKeyboards, cmd_id::About,
    cmd_id::CycleFontSize, cmd_id::ReconnectSsh, cmd_id::SwitchToNextServer,
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 0),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 1),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 2),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 3),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 4),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 5),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 6),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 7),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 8),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 9),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 10),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 11),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 12),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 13),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 14),
    static_cast<uint16_t>(cmd_id::ConnectServerBase + 15),
};

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry s;
    return s;
}

namespace {
void put(Command& c, const char* title, const char* hint, uint16_t id) {
    std::strncpy(c.title, title, sizeof(c.title) - 1);
    c.title[sizeof(c.title) - 1] = '\0';
    std::strncpy(c.hint, hint, sizeof(c.hint) - 1);
    c.hint[sizeof(c.hint) - 1] = '\0';
    c.id = id;
}
} // namespace

void CommandRegistry::registerGlobals() {
    globals_count_ = 0;
    auto& g = globals_;
    put(g[globals_count_++], "Open dashboard",                  "", cmd_id::OpenDashboard);
    put(g[globals_count_++], "Open terminal",                   "", cmd_id::OpenTerminal);
    put(g[globals_count_++], "WiFi networks...",                "", cmd_id::WifiNetworks);
    put(g[globals_count_++], "Servers...",                      "", cmd_id::Servers);
    put(g[globals_count_++], "Add server...",                   "", cmd_id::AddServer);
    put(g[globals_count_++], "SSH keys...",                     "", cmd_id::SshKeys);
    put(g[globals_count_++], "Settings...",                     "", cmd_id::Settings);
    put(g[globals_count_++], "Pair keyboard",          "Btn A long", cmd_id::PairKeyboard);
    put(g[globals_count_++], "Forget all paired keyboards",     "", cmd_id::ForgetAllKeyboards);
    put(g[globals_count_++], "About",                           "", cmd_id::About);
    put(g[globals_count_++], "Cycle font size",       "Btn B short", cmd_id::CycleFontSize);
    put(g[globals_count_++], "Reconnect SSH",                   "", cmd_id::ReconnectSsh);
    put(g[globals_count_++], "Switch to next server",  "Btn B long", cmd_id::SwitchToNextServer);
}

void CommandRegistry::refreshDynamicServerCommands(const sdcard::ConfigManager& cfg) {
    dynamic_count_ = 0;
    int n = cfg.serverCount();
    if (n < 0) n = 0;
    if (n > static_cast<int>(kMaxDynamicServers))
        n = static_cast<int>(kMaxDynamicServers);
    for (int i = 0; i < n; ++i) {
        const auto& srv = cfg.getServer(i);
        // Compose "Connect to: <name>". The destination is 40 bytes;
        // "Connect to: " is 12 bytes + NUL → 27 bytes left for the name.
        // Use %.*s with an explicit precision to avoid format-truncation
        // warnings (server name field is 32 bytes).
        char title[40];
        std::snprintf(title, sizeof(title), "Connect to: %.27s", srv.creds.name);
        put(dynamic_[dynamic_count_++], title, "",
            static_cast<uint16_t>(cmd_id::ConnectServerBase + i));
    }
}

SpanView<const Command> CommandRegistry::globals() const {
    return SpanView<const Command>(globals_, globals_count_);
}

SpanView<const Command> CommandRegistry::dynamicServerCommands() const {
    return SpanView<const Command>(dynamic_, dynamic_count_);
}

} // namespace app
