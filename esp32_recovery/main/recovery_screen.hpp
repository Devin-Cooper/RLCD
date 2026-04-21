#pragma once

namespace recovery {
// Init SPI + ST7305 panel + framebuffer, render the RECOVERY MODE banner
// once, and spawn a heartbeat task that blinks a bottom-right pixel every
// second so a frozen device is visually distinguishable from a running one.
void startDisplay();
} // namespace recovery
