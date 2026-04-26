#include "dir_listing.hpp"
namespace fb {
bool DirListing::load(const std::string&, HiddenMode) {
    error_ = true;
    return false;
}
std::vector<FileEntry> DirListing::assembleEntries(
    std::vector<FileEntry> raw, const std::string&, HiddenMode) {
    return raw;
}
}  // namespace fb
