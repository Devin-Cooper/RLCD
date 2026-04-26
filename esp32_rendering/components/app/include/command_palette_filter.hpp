#pragma once

namespace app {

/// Case-insensitive substring match. Empty needle matches anything.
bool icontains(const char* haystack, const char* needle);

} // namespace app
