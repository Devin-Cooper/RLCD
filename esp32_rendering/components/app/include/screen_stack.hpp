#pragma once

#include "screen.hpp"
#include <memory>
#include <vector>
#include <cassert>
#include <functional>

namespace app {

class ScreenStack {
public:
    ScreenStack() = default;

    ScreenStack(const ScreenStack&) = delete;
    ScreenStack& operator=(const ScreenStack&) = delete;

    /// Push immediately: runs onEnter() before returning. New top is
    /// ready to render this frame.
    void push(std::unique_ptr<Screen> s);

    /// Deferred pop: marks the top for removal; actual pop runs in
    /// applyPending() at the end of input drain.
    void pop();

    /// Deferred: pops then pushes atomically in applyPending().
    void replace(std::unique_ptr<Screen> s);

    /// Deferred: pop everything above the bottom-most screen, then push.
    /// Used by pairing entry ("clear to base then push PairingScreen").
    void clearToBaseAndPush(std::unique_ptr<Screen> s);

    /// Apply deferred ops. Call once per frame between input drain and
    /// renderAll. Runs onExit() on popped screens and onEnter() on
    /// replacement pushes.
    void applyPending();

    /// Renders bottom-up starting from the lowest opaque screen.
    /// Clears fb once before the lowest opaque renders. Sets an internal
    /// in-render-phase flag that push/pop/replace assert against.
    void renderAll(onebit::IFramebuffer& fb,
                   const onebit::BitmapFont& font);

    Screen* top();
    size_t depth() const { return stack_.size(); }

    /// Index accessor — added per plan Amendment I so callers can walk
    /// the stack to find a specific Screen type with dynamic_cast.
    Screen* at(size_t i) const {
        return i < stack_.size() ? stack_[i].get() : nullptr;
    }

    struct BypassToken {
    private:
        BypassToken() = default;
        friend class KeyboardGateModal;
        friend class PairingScreen;
    public:
#ifdef RLCD_HOST_TEST
        // Test-only seam — production code must construct via the friended
        // classes (KeyboardGateModal, PairingScreen).
        static BypassToken forTest() { return BypassToken{}; }
#endif
    };

    void pushBypassingGate(BypassToken, std::unique_ptr<Screen> s);
    void replaceBypassingGate(BypassToken, std::unique_ptr<Screen> s);

    using GatePolicy =
        std::function<bool(Screen& candidate,
                           std::unique_ptr<Screen>& deferred_inout)>;
    void setGatePolicy(GatePolicy p);

private:
    std::vector<std::unique_ptr<Screen>> stack_;

    enum class PendingKind { None, Pop, Replace, ClearToBaseAndPush };
    PendingKind pending_kind_ = PendingKind::None;
    std::unique_ptr<Screen> pending_push_;
    int pending_pop_count_ = 0;

    bool in_render_phase_ = false;

    void assertNotInRenderPhase(const char* op) const;

    GatePolicy gate_policy_;
};

} // namespace app
