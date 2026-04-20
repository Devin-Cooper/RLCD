#pragma once
#include "test_console.hpp"

namespace test_console {

/// Internal accessor used by command handlers. Set by init() and
/// non-null for the lifetime of the program thereafter.
Context* getContext();
void setContext(Context* ctx);

} // namespace test_console
