#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <cstddef>
#include <cstdint>

namespace app {

enum class TextInputResult : uint8_t { None, Submit, Cancel };

struct TextInputOpts {
    bool masked = false;
    bool numeric = false;
    bool tab_toggles_reveal = true;
};

/// Lightweight text-entry widget. Screens own the backing buffer and
/// embed a TextInput to drive it. Capacity includes the NUL terminator
/// — max usable length is capacity-1.
class TextInput {
public:
    TextInput(char* buffer, size_t capacity, TextInputOpts opts = {});

    TextInputResult handleKey(const uint8_t* data, size_t len);

    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                int16_t x, int16_t y, int16_t width);

    void setMasked(bool m) { opts_.masked = m; }
    void toggleReveal() { revealed_ = !revealed_; }
    void clear();
    size_t length() const { return cursor_; }
    const char* data() const { return buffer_; }

    static constexpr uint8_t KEY_CTRL_R = 0x12;  // ASCII DC2 — Ctrl+R

private:
    char*  buffer_;
    size_t capacity_;
    size_t cursor_;
    TextInputOpts opts_;
    bool revealed_ = false;
    int64_t cursor_blink_epoch_us_ = 0;
};

} // namespace app
