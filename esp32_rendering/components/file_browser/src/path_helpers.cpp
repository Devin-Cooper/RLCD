#include "path_helpers.hpp"
namespace fb::path {
std::string join(std::string_view, std::string_view) { return {}; }
std::string parent(std::string_view) { return {}; }
std::string_view basename(std::string_view p) { return p; }
std::string truncateBreadcrumb(std::string_view, std::size_t) { return {}; }
}  // namespace fb::path
