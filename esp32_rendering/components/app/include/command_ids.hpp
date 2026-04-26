// esp32_rendering/components/app/include/command_ids.hpp
#pragma once
#include <cstdint>

namespace app {

// Stable command ids for CommandRegistry + dispatcher (Phase 10).
namespace cmd_id {
    constexpr uint16_t OpenDashboard       = 0x100;
    constexpr uint16_t OpenTerminal        = 0x101;
    constexpr uint16_t WifiNetworks        = 0x102;
    constexpr uint16_t Servers             = 0x103;
    constexpr uint16_t AddServer           = 0x104;
    constexpr uint16_t SshKeys             = 0x105;
    constexpr uint16_t Settings            = 0x106;
    constexpr uint16_t PairKeyboard        = 0x107;
    constexpr uint16_t ForgetAllKeyboards  = 0x108;
    constexpr uint16_t About               = 0x109;
    constexpr uint16_t CycleFontSize       = 0x10A;
    constexpr uint16_t ReconnectSsh        = 0x10B;
    constexpr uint16_t SwitchToNextServer  = 0x10C;
    constexpr uint16_t ConnectServerBase   = 0x200;
    constexpr uint16_t ConnectServerEnd    = 0x20F;
}

// Per-screen focus-rect tween ids (low 24 bits of an Animator tag).
namespace focus_id {
    constexpr uint32_t MenuScreen         = 0x01;
    constexpr uint32_t WifiScreen         = 0x02;
    constexpr uint32_t ServerListScreen   = 0x03;
    constexpr uint32_t SshKeyListScreen   = 0x04;
    constexpr uint32_t SshKeyImportScreen = 0x05;
    constexpr uint32_t CommandPalette     = 0x06;
}

} // namespace app
