#include "terminal_mode.hpp"
#include <esp_log.h>

static const char* TAG = "terminal_mode";

namespace app {

TerminalMode::TerminalMode(onebit::IFramebuffer& fb, const onebit::BitmapFont& font)
    : fb_(fb)
{
    rebuild(font);
}

TerminalMode::~TerminalMode() = default;

void TerminalMode::rebuild(const onebit::BitmapFont& font) {
    // Calculate grid dimensions from display size and font metrics
    int cell_w = font.glyph_width;
    int cell_h = font.glyph_height + 1;
    int new_cols = fb_.width() / cell_w;
    int new_rows = fb_.height() / cell_h;

    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    ESP_LOGI(TAG, "Terminal grid: %dx%d (font %dx%d)",
             new_cols, new_rows, font.glyph_width, font.glyph_height);

    // unique_ptr destruction order matches declaration order (renderer,
    // parser, buffer) when we reassign. Reset explicitly to drop the old
    // objects before allocating the new buffer.
    renderer_.reset();
    parser_.reset();
    buffer_.reset();

    buffer_ = std::make_unique<onebit::TerminalBuffer>(new_cols, new_rows, 500);
    // Re-apply stored output callback so DSR / cursor-position replies
    // continue to flow after a font cycle (Spec 04 bug 1).
    if (output_cb_) {
        auto cb = output_cb_;
        parser_ = std::make_unique<onebit::AnsiParser>(
            *buffer_,
            [cb](const uint8_t* data, size_t len) { if (cb) cb(data, len); });
    } else {
        parser_ = std::make_unique<onebit::AnsiParser>(*buffer_);
    }
    renderer_ = std::make_unique<onebit::TerminalRenderer>(fb_, font);
}

void TerminalMode::feedData(const uint8_t* data, size_t len) {
    if (parser_) {
        parser_->feed(data, len);
    }
}

void TerminalMode::setOutputCallback(
        std::function<void(const uint8_t*, size_t)> cb) {
    // Store so subsequent rebuild() calls can re-wire the parser.
    output_cb_ = cb;

    if (!buffer_) return;

    parser_.reset();
    if (output_cb_) {
        auto stored = output_cb_;
        parser_ = std::make_unique<onebit::AnsiParser>(
            *buffer_,
            [stored](const uint8_t* data, size_t len) { if (stored) stored(data, len); });
    } else {
        parser_ = std::make_unique<onebit::AnsiParser>(*buffer_);
    }
}

void TerminalMode::render() {
    if (renderer_ && buffer_) {
        renderer_->render(*buffer_);
    }
}

void TerminalMode::setFont(const onebit::BitmapFont& font) {
    int old_cols = cols();
    int old_rows = rows();

    rebuild(font);

    int new_cols_val = cols();
    int new_rows_val = rows();

    if ((new_cols_val != old_cols || new_rows_val != old_rows) && resize_cb_) {
        resize_cb_(new_cols_val, new_rows_val);
    }
}

int TerminalMode::cols() const {
    return buffer_ ? buffer_->cols() : 0;
}

int TerminalMode::rows() const {
    return buffer_ ? buffer_->rows() : 0;
}

} // namespace app
