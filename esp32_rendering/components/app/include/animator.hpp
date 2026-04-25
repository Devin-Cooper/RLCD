// esp32_rendering/components/app/include/animator.hpp
#pragma once
#include <array>
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
