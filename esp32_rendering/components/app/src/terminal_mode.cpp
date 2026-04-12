#include "terminal_mode.hpp"
#include <esp_log.h>

static const char* TAG = "terminal_mode";

namespace app {

TerminalMode::TerminalMode(onebit::IFramebuffer& fb, const onebit::BitmapFont& font)
    : fb_(fb)
    , buffer_(nullptr)
    , parser_(nullptr)
    , renderer_(nullptr)
{
    rebuild(font);
}

TerminalMode::~TerminalMode() {
    delete renderer_;
    delete parser_;
    delete buffer_;
}

void TerminalMode::rebuild(const onebit::BitmapFont& font) {
    // Calculate grid dimensions from display size and font metrics
    int cell_w = font.glyph_width;   // no extra spacing — renderer handles it
    int cell_h = font.glyph_height + 1;  // +1 row spacing (matches TerminalRenderer)
    int new_cols = fb_.width() / cell_w;
    int new_rows = fb_.height() / cell_h;

    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    ESP_LOGI(TAG, "Terminal grid: %dx%d (font %dx%d)",
             new_cols, new_rows, font.glyph_width, font.glyph_height);

    // Preserve output callback if parser exists
    // (OutputCallback is stored in parser, not directly accessible — we
    //  re-wire after rebuild via setOutputCallback)

    delete renderer_;
    delete parser_;
    delete buffer_;

    buffer_ = new onebit::TerminalBuffer(new_cols, new_rows, 500);
    parser_ = new onebit::AnsiParser(*buffer_);
    renderer_ = new onebit::TerminalRenderer(fb_, font);
}

void TerminalMode::feedData(const uint8_t* data, size_t len) {
    if (parser_) {
        parser_->feed(data, len);
    }
}

void TerminalMode::setOutputCallback(
        std::function<void(const uint8_t*, size_t)> cb) {
    // Re-create parser with new output callback
    if (buffer_) {
        const onebit::BitmapFont* current_font = nullptr;
        if (renderer_) {
            current_font = &renderer_->font();
        }

        delete parser_;
        parser_ = new onebit::AnsiParser(
            *buffer_,
            [cb](const uint8_t* data, size_t len) {
                if (cb) cb(data, len);
            }
        );

        // Renderer stays — font unchanged, just rewired the parser
        (void)current_font;
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
