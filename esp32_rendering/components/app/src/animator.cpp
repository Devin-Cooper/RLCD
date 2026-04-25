#include "animator.hpp"

namespace app {

void Animator::start(uint32_t, int16_t, int16_t, uint32_t, int64_t) {}
int16_t Animator::value(uint32_t, int64_t) const { return 0; }
bool Animator::inProgress(uint32_t, int64_t) const { return false; }
void Animator::cancel(uint32_t) {}
void Animator::tick(int64_t) {}

} // namespace app
