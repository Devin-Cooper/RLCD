#pragma once

#ifdef __cplusplus

namespace app { class ScreenStack; class OverlayManager;
                class Dashboard; class TerminalMode; struct Settings; }
namespace wifi    { class WifiManager; }
namespace sdcard  { class ConfigManager; }
namespace ble_hid { class BleHidHost; }
namespace ssh     { class SshClient; }

namespace test_console {

struct Context {
    app::ScreenStack&      stack;
    app::OverlayManager&   overlay;
    wifi::WifiManager&     wifiMgr;
    sdcard::ConfigManager& configMgr;
    ble_hid::BleHidHost&   bleHost;
    ssh::SshClient&        sshClient;
    app::Settings&         settings;
};

/// Initialize esp_console on UART0, register all commands, start REPL
/// task. No-op if CONFIG_TEST_CONSOLE_ENABLED is not set. Call AFTER
/// all subsystems in ctx are initialized and stack.push(Dashboard)
/// has run (see spec §Architecture initialization ordering).
void init(Context& ctx);

} // namespace test_console

#endif
