#pragma once
#include "command_ids.hpp"
#include "screen.hpp"        // for app::Command forward; full Command lives here
#include "span_view.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

namespace sdcard { class ConfigManager; }

namespace app {

/// One entry in the command palette / registry.
///
/// `title` is what the user sees in the palette; `hint` is an optional
/// keybind/affordance string shown after the title (e.g. "Btn A long").
/// `id` is the stable cmd_id::* value used by dispatchCommand.
struct Command {
    char     title[40];
    char     hint[24];
    uint16_t id;
};

/// Singleton holding the global command list and the per-server "Connect to:
/// <name>" dynamic commands. Globals are populated once at boot via
/// registerGlobals(); dynamic server entries are refreshed whenever the
/// server list changes (add/edit/delete).
class CommandRegistry {
public:
    static CommandRegistry& instance();

    void registerGlobals();
    void refreshDynamicServerCommands(const sdcard::ConfigManager& cfg);

    SpanView<const Command> globals() const;
    SpanView<const Command> dynamicServerCommands() const;

    static constexpr std::size_t kMaxGlobals = 32;
    static constexpr std::size_t kMaxDynamicServers = 16;

private:
    CommandRegistry() = default;
    Command  globals_[kMaxGlobals]{};
    std::size_t globals_count_ = 0;
    Command  dynamic_[kMaxDynamicServers]{};
    std::size_t dynamic_count_ = 0;
};

/// Source-of-truth list of every dispatchable command id known to the
/// registry. Used by host tests to verify that registerGlobals() emits
/// commands that are all reachable through dispatchCommand.
///
/// 16 globals + 16 ConnectServerBase+i slots = 32 entries.
extern const std::array<uint16_t, 32> kAllDispatchableIds;

} // namespace app
