#pragma once

#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/terminal/terminal_buffer.hpp>
#include <1bit/terminal/ansi_parser.hpp>
#include <1bit/terminal/terminal_renderer.hpp>
#include "ssh_client.hpp"
#include <cstdint>
#include <functional>
#include <memory>

namespace app {

/// Terminal mode: wires together SSH client, AnsiParser, TerminalBuffer,
/// and TerminalRenderer into a complete interactive terminal.
class TerminalMode {
public:
    /// Construct terminal mode with a framebuffer and initial font.
    /// Allocates TerminalBuffer sized to fit the display at the given font size.
    TerminalMode(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);
    ~TerminalMode();

    // Non-copyable
    TerminalMode(const TerminalMode&) = delete;
    TerminalMode& operator=(const TerminalMode&) = delete;

    /// Feed raw data from SSH channel into the ANSI parser.
    void feedData(const uint8_t* data, size_t len);

    /// Set callback for data that needs to be sent back (DSR responses).
    /// This should route to ssh::SshClient::send().
    void setOutputCallback(std::function<void(const uint8_t*, size_t)> cb);

    /// Render the terminal to the framebuffer.
    void render();

    /// Change font and resize terminal grid accordingly.
    /// Notifies SSH client of new dimensions via resize callback if set.
    void setFont(const onebit::BitmapFont& font);

    /// Set callback to notify SSH of terminal resize (cols, rows).
    void setResizeCallback(std::function<void(int, int)> cb) {
        resize_cb_ = cb;
    }

    /// Current terminal dimensions in character cells.
    int cols() const;
    int rows() const;

private:
    onebit::IFramebuffer& fb_;
    std::unique_ptr<onebit::TerminalBuffer> buffer_;
    std::unique_ptr<onebit::AnsiParser> parser_;
    std::unique_ptr<onebit::TerminalRenderer> renderer_;

    std::function<void(int, int)> resize_cb_;
    std::function<void(const uint8_t*, size_t)> output_cb_;

    /// Rebuild terminal buffer and renderer for new font.
    void rebuild(const onebit::BitmapFont& font);
};

} // namespace app
