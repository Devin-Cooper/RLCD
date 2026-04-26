// esp32_rendering/components/app/include/animator.hpp
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace app {

inline float easeOutCubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

enum class TweenKind : uint8_t {
    FocusRect  = 0x01,
    ModalScale = 0x02,
    ToastSlide = 0x03,
};

inline constexpr uint32_t makeTag(TweenKind k, uint32_t id) {
    return (uint32_t(k) << 24) | (id & 0x00FFFFFFu);
}

struct Tween {
    int16_t  from;
    int16_t  to;
    int64_t  start_us;
    uint32_t duration_us;
    uint32_t tag;
    bool     active;
};

constexpr uint32_t kFocusRectUs  = 120'000;
constexpr uint32_t kModalScaleUs = 140'000;
constexpr uint32_t kToastSlideUs = 180'000;

/// Animator — fixed-size pool (8 slots) of active tweens.
///
/// Tag layout: high 8 bits = TweenKind, low 24 bits = per-screen /
/// per-modal id. See `makeTag()`.
///
/// Snap-on-overflow: if `start()` is called with a unique tag and all 8
/// slots are taken, the call drops silently and `value()` returns 0 for
/// that tag (so callers should gate on `inProgress()` and use the
/// "settled" value when false).
///
/// Rounding: `value()` returns the eased pixel value rounded to nearest
/// int16_t (round-half-to-even via std::lrint).
///
/// `tick(now_us)` should be called once per frame at frame start, before
/// any `value()` reads. It prunes finished tweens to free their slots.
class Animator {
public:
    static constexpr size_t kMaxTweens = 8;

    void start(uint32_t tag, int16_t from, int16_t to,
               uint32_t duration_us, int64_t now_us);
    int16_t value(uint32_t tag, int64_t now_us) const;
    bool inProgress(uint32_t tag, int64_t now_us) const;
    void cancel(uint32_t tag);
    void tick(int64_t now_us);

private:
    std::array<Tween, kMaxTweens> tweens_{};
};

} // namespace app
