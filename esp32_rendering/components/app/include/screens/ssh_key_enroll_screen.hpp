#pragma once

#include "screen.hpp"
#include "screen_context.hpp"
#include "ssh_key_types.hpp"

#include <string>
#include <vector>

namespace app {

/// SSH enrollment: pick a password-auth server, prompt for the SSH
/// password, then run `ssh_keys::enroll_key`.
///
/// State machine (internal):
///   Picker  — arrow-nav over password-auth servers, Enter to pick.
///   (Password is a pushed TextInputScreen — this screen stays on the
///    stack and handles the Submit callback, transitioning to Running
///    and invoking runEnroll synchronously in the same tick.)
///   Running — displays "Enrolling...". The enroll call itself is
///             synchronous; it dispatches toasts or modals when it
///             returns.
class SshKeyEnrollScreen : public Screen {
public:
    SshKeyEnrollScreen(ScreenContext& ctx, const ssh_keys::KeyId& id);

    void onEnter() override;
    void handleInput(const input::InputEvent& evt, ScreenStack& stack) override;
    void render(onebit::IFramebuffer& fb, const onebit::BitmapFont& font) override;

    app::SpanView<const app::KeybindHint> keybindHints() const override;

private:
    enum class State { Picker, Running, Done };

    ScreenContext& ctx_;
    ssh_keys::KeyId id_;
    State state_ = State::Picker;

    std::vector<int> eligible_;  // indices into configMgr that are password-auth
    int sel_ = 0;

    int chosen_server_idx_ = -1;

    void collectEligible();
    void beginPasswordPrompt(ScreenStack& stack);
    void runEnroll(const std::string& password);
};

} // namespace app
