#include "file_buffer.hpp"
#include <cstring>
namespace fb {
FileBuffer::~FileBuffer() { freeBuffer(); }
bool FileBuffer::load(const std::string&) { mode_ = Mode::Error; return false; }
std::size_t FileBuffer::lineOffset(std::size_t) const { return 0; }
std::string_view FileBuffer::line(std::size_t) const { return {}; }
std::vector<std::uint32_t> FileBuffer::findAll(std::string_view) const { return {}; }
void FileBuffer::formatHexRow(const char*, std::size_t, std::uint32_t, char out[80]) {
    std::memset(out, ' ', 79); out[79] = 0;
}
void FileBuffer::freeBuffer() {}
}  // namespace fb
