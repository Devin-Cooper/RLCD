#include "animator.hpp"
#include <cmath>

namespace app {

namespace {
int slotForTag(const std::array<Tween, Animator::kMaxTweens>& tweens, uint32_t tag) {
    for (size_t i = 0; i < tweens.size(); ++i) {
        if (tweens[i].active && tweens[i].tag == tag) return static_cast<int>(i);
    }
    return -1;
}
int firstFreeSlot(const std::array<Tween, Animator::kMaxTweens>& tweens) {
    for (size_t i = 0; i < tweens.size(); ++i) {
        if (!tweens[i].active) return static_cast<int>(i);
    }
    return -1;
}
} // namespace

void Animator::start(uint32_t tag, int16_t from, int16_t to,
                     uint32_t duration_us, int64_t now_us) {
    int idx = slotForTag(tweens_, tag);
    if (idx < 0) idx = firstFreeSlot(tweens_);
    if (idx < 0) return;
    tweens_[idx] = Tween{from, to, now_us, duration_us, tag, true};
}

int16_t Animator::value(uint32_t tag, int64_t now_us) const {
    int idx = slotForTag(tweens_, tag);
    if (idx < 0) return 0;
    const auto& t = tweens_[idx];
    int64_t elapsed = now_us - t.start_us;
    if (elapsed <= 0) return t.from;
    if (elapsed >= static_cast<int64_t>(t.duration_us)) return t.to;
    float u = static_cast<float>(elapsed) / static_cast<float>(t.duration_us);
    float eased = easeOutCubic(u);
    float v = static_cast<float>(t.from)
            + eased * static_cast<float>(t.to - t.from);
    return static_cast<int16_t>(std::lrint(v));
}

bool Animator::inProgress(uint32_t tag, int64_t now_us) const {
    int idx = slotForTag(tweens_, tag);
    if (idx < 0) return false;
    int64_t elapsed = now_us - tweens_[idx].start_us;
    return elapsed < static_cast<int64_t>(tweens_[idx].duration_us);
}

void Animator::cancel(uint32_t tag) {
    int idx = slotForTag(tweens_, tag);
    if (idx >= 0) tweens_[idx].active = false;
}

void Animator::tick(int64_t now_us) {
    for (auto& t : tweens_) {
        if (t.active) {
            int64_t elapsed = now_us - t.start_us;
            if (elapsed >= static_cast<int64_t>(t.duration_us)) {
                t.active = false;
            }
        }
    }
}

} // namespace app
