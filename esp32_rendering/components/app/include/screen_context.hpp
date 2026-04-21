#pragma once

#include <cstdint>
#include <functional>

namespace onebit { class IFramebuffer; }
namespace st7305 { class Display; }
namespace ssh    { class SshClient; }
namespace wifi   { class WifiManager; }
namespace sdcard { class ConfigManager; }
namespace ble_hid { class BleHidHost; }
namespace ssh_keys { class KeyStore; }

namespace app {

class ScreenStack;
class OverlayManager;
class Dashboard;
class TerminalMode;
struct Settings;

/// References to long-lived app state passed to each Screen at construction.
/// Screens never take ownership. Kept as a bundle so Screen constructors
/// stay readable (one param instead of many).
struct ScreenContext {
    onebit::IFramebuffer&  fb;
    st7305::Display&       display;
    ssh::SshClient&        sshClient;
    wifi::WifiManager&     wifiMgr;
    sdcard::ConfigManager& configMgr;
    ssh_keys::KeyStore&    keyStore;
    ble_hid::BleHidHost&   bleHost;
    Settings&              settings;
    ScreenStack&           stack;
    OverlayManager&        overlay;
    Dashboard&             dashboard;
    TerminalMode&          terminalMode;
    uint8_t&               currentFontSize;

    // Plan Amendment K: rotate to the next server (Btn B long in Dashboard).
    // Populated in main.cpp at Task 14.
    std::function<void()>  switchToNextServer;

    // Plan Amendment K: reconnect SSH to the current active server.
    // Populated in main.cpp at Task 14; used by ServerListScreen Shift+A
    // (Task 21) to make the "set active" keypress actually reconnect.
    std::function<void()>  switchToActiveServer;
};

} // namespace app
