#include "command_dispatcher.hpp"
#include "command_ids.hpp"
#include "screen_context.hpp"
#include "screen_stack.hpp"
#include "settings.hpp"
#include "screens/dashboard_screen.hpp"
#include "screens/terminal_screen.hpp"
#include "screens/wifi_screen.hpp"
#include "screens/server_list_screen.hpp"
#include "screens/server_edit_screen.hpp"
#include "screens/ssh_key_list_screen.hpp"
#include "screens/settings_screen.hpp"
#include "screens/about_screen.hpp"
#include "screens/pairing_screen.hpp"
#include "screens/speaker_test_screen.hpp"
#include "screens/mic_test_screen.hpp"
#include "screens/file_browser_screen.hpp"
#include "config_manager.hpp"
#include "overlay.hpp"
#include <esp_log.h>
#include <memory>

static const char* TAG = "cmd_dispatch";

namespace app {

DispatchResult dispatchCommand(uint16_t id, ScreenContext& ctx) {
    // Contextual ids (0xFF00..0xFFFE) target the current top screen.
    // CommandPalette pops itself + applyPending() before calling
    // dispatchCommand, so `top()` here is the underlying screen that
    // owns the contextual command.
    if (id >= 0xFF00 && id < 0xFFFF) {
        if (auto* top = ctx.stack.top()) top->dispatchContextual(id);
        return DispatchResult::ScreenStays;
    }

    // ConnectServerBase..ConnectServerEnd → set active + reconnect.
    if (id >= cmd_id::ConnectServerBase && id <= cmd_id::ConnectServerEnd) {
        int idx = static_cast<int>(id - cmd_id::ConnectServerBase);
        ctx.configMgr.setActiveServer(idx);
        if (ctx.switchToActiveServer) ctx.switchToActiveServer();
        return DispatchResult::ScreenStays;
    }

    switch (id) {
        case cmd_id::OpenDashboard:
            ctx.stack.replace(std::make_unique<DashboardScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::OpenTerminal:
            ctx.stack.replace(std::make_unique<TerminalScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::WifiNetworks:
            ctx.stack.replace(std::make_unique<WifiScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::Servers:
            ctx.stack.replace(std::make_unique<ServerListScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::AddServer:
            ctx.stack.replace(std::make_unique<ServerEditScreen>(ctx, /*index=*/-1));
            return DispatchResult::StackChanged;
        case cmd_id::SshKeys:
            ctx.stack.replace(std::make_unique<SshKeyListScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::Settings:
            ctx.stack.replace(std::make_unique<SettingsScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::PairKeyboard:
            ctx.stack.clearToBaseAndPush(std::make_unique<PairingScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::ForgetAllKeyboards:
            ctx.overlay.showConfirm(
                "Forget keyboards?",
                "This wipes all bonded keyboards.",
                [](bool yes){
                    if (yes) {
                        // ble_store_clear is a no-op stub for now; real impl in Phase 13.
                        ESP_LOGI(TAG, "ForgetAllKeyboards confirmed (TODO: ble_store_clear)");
                    }
                });
            return DispatchResult::ScreenStays;
        case cmd_id::About:
            ctx.stack.replace(std::make_unique<AboutScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::CycleFontSize:
            ctx.currentFontSize = static_cast<uint8_t>((ctx.currentFontSize + 1) % 3);
            ctx.settings.font_size = ctx.currentFontSize;
            saveSettings(ctx.settings);
            return DispatchResult::ScreenStays;
        case cmd_id::ReconnectSsh:
            if (ctx.switchToActiveServer) ctx.switchToActiveServer();
            return DispatchResult::ScreenStays;
        case cmd_id::SwitchToNextServer:
            if (ctx.switchToNextServer) ctx.switchToNextServer();
            return DispatchResult::ScreenStays;
        case cmd_id::SpeakerTest:
            ctx.stack.push(std::make_unique<SpeakerTestScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::MicTest:
            ctx.stack.push(std::make_unique<MicTestScreen>(ctx));
            return DispatchResult::StackChanged;
        case cmd_id::OpenFiles:
            ctx.stack.replace(std::make_unique<FileBrowserScreen>(ctx));
            return DispatchResult::StackChanged;
    }

    ESP_LOGW(TAG, "dispatchCommand: unknown id 0x%04x", id);
    return DispatchResult::ScreenStays;
}

} // namespace app
