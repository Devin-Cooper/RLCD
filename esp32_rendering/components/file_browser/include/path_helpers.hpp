#pragma once

#include <string>
#include <string_view>

namespace fb::path {

std::string join(std::string_view a, std::string_view b);
std::string parent(std::string_view path);
std::string_view basename(std::string_view path);
std::string truncateBreadcrumb(std::string_view path, std::size_t max_chars);

}  // namespace fb::path
