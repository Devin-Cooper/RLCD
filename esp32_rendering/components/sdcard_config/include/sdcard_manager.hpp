#pragma once

#include <cstdint>
#include "sdmmc_cmd.h"

namespace sdcard {

class SDCardManager {
public:
    SDCardManager();
    ~SDCardManager();

    SDCardManager(const SDCardManager&) = delete;
    SDCardManager& operator=(const SDCardManager&) = delete;

    /// Mount SD card at /sdcard using SDMMC 1-bit bus.
    /// Returns true if mounted successfully, false if no card or error.
    bool mount();

    /// Unmount the SD card.
    void unmount();

    /// Check if SD card is currently mounted.
    bool isMounted() const { return mounted_; }

private:
    bool mounted_;
    sdmmc_card_t* card_;
};

} // namespace sdcard
