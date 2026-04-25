#include "screen_stack.hpp"
#include <esp_log.h>

static const char* TAG = "screen_stack";

namespace app {

void ScreenStack::assertNotInRenderPhase(const char* op) const {
    if (in_render_phase_) {
        ESP_LOGE(TAG, "ScreenStack::%s called during render phase — "
                      "screens must only mutate stack from handleInput", op);
        assert(false && "ScreenStack mutation during render phase");
    }
}

void ScreenStack::push(std::unique_ptr<Screen> s) {
    assertNotInRenderPhase("push");
    if (gate_policy_) {
        std::unique_ptr<Screen> deferred = std::move(s);
        if (gate_policy_(*deferred, deferred)) {
            // Policy intercepted; deferred has been moved-from or rerouted.
            return;
        }
        // Policy declined; deferred still holds the original Screen.
        s = std::move(deferred);
    }
    s->onEnter();
    stack_.push_back(std::move(s));
}

void ScreenStack::pushBypassingGate(BypassToken, std::unique_ptr<Screen> s) {
    assertNotInRenderPhase("pushBypassingGate");
    s->onEnter();
    stack_.push_back(std::move(s));
}

void ScreenStack::replaceBypassingGate(BypassToken, std::unique_ptr<Screen> s) {
    assertNotInRenderPhase("replaceBypassingGate");
    pending_kind_ = PendingKind::Replace;
    pending_push_ = std::move(s);
    pending_pop_count_ = 1;
}

void ScreenStack::setGatePolicy(GatePolicy p) {
    gate_policy_ = std::move(p);
}

void ScreenStack::pop() {
    assertNotInRenderPhase("pop");
    if (stack_.size() <= 1) {
        ESP_LOGW(TAG, "pop refused: would empty stack");
        return;
    }
    if (pending_kind_ == PendingKind::None) {
        pending_kind_ = PendingKind::Pop;
        pending_pop_count_ = 1;
    } else if (pending_kind_ == PendingKind::Pop) {
        pending_pop_count_++;
    } else {
        ESP_LOGW(TAG, "pop during pending %d — ignored",
                 static_cast<int>(pending_kind_));
    }
}

void ScreenStack::replace(std::unique_ptr<Screen> s) {
    assertNotInRenderPhase("replace");
    pending_kind_ = PendingKind::Replace;
    pending_push_ = std::move(s);
    pending_pop_count_ = 1;
}

void ScreenStack::clearToBaseAndPush(std::unique_ptr<Screen> s) {
    assertNotInRenderPhase("clearToBaseAndPush");
    pending_kind_ = PendingKind::ClearToBaseAndPush;
    pending_push_ = std::move(s);
}

void ScreenStack::applyPending() {
    switch (pending_kind_) {
        case PendingKind::None:
            return;
        case PendingKind::Pop:
            for (int i = 0; i < pending_pop_count_ && stack_.size() > 1; ++i) {
                stack_.back()->onExit();
                stack_.pop_back();
            }
            break;
        case PendingKind::Replace:
            if (!stack_.empty()) {
                stack_.back()->onExit();
                stack_.pop_back();
            }
            if (pending_push_) {
                pending_push_->onEnter();
                stack_.push_back(std::move(pending_push_));
            }
            break;
        case PendingKind::ClearToBaseAndPush:
            while (stack_.size() > 1) {
                stack_.back()->onExit();
                stack_.pop_back();
            }
            if (pending_push_) {
                pending_push_->onEnter();
                stack_.push_back(std::move(pending_push_));
            }
            break;
    }
    pending_kind_ = PendingKind::None;
    pending_push_.reset();
    pending_pop_count_ = 0;
}

void ScreenStack::renderAll(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font) {
    renderAll(fb, font, /*now_us=*/0);
}

void ScreenStack::renderAll(onebit::IFramebuffer& fb,
                            const onebit::BitmapFont& font,
                            int64_t /*now_us*/) {
    in_render_phase_ = true;
    int first = static_cast<int>(stack_.size()) - 1;
    while (first > 0 && stack_[first]->isTransparent()) --first;
    fb.clear(onebit::WHITE);
    for (size_t i = static_cast<size_t>(first); i < stack_.size(); ++i) {
        stack_[i]->render(fb, font);
    }
    in_render_phase_ = false;
}

Screen* ScreenStack::top() {
    if (stack_.empty()) return nullptr;
    return stack_.back().get();
}

} // namespace app
