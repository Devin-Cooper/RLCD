#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fb {

class FileBuffer {
public:
    enum class Mode { Text, Hex, TooLarge, Error };

    FileBuffer() = default;
    ~FileBuffer();
    FileBuffer(const FileBuffer&) = delete;
    FileBuffer& operator=(const FileBuffer&) = delete;

    bool load(const std::string& path);

    Mode mode() const { return mode_; }
    const char* data() const { return data_; }
    std::size_t size() const { return size_; }
    std::size_t fileSize() const { return file_size_; }

    std::size_t lineCount() const { return line_offsets_.size(); }
    std::size_t lineOffset(std::size_t line) const;
    std::string_view line(std::size_t line) const;

    int errnoCode() const { return errno_; }

    // Search (text mode). Returns line numbers of matches (case-insensitive).
    std::vector<std::uint32_t> findAll(std::string_view needle) const;

    // Hex helper: format bytes [offset, offset+16) into the canonical 16B row.
    static void formatHexRow(const char* base, std::size_t base_size,
                             std::uint32_t offset, char out[80]);

private:
    Mode mode_ = Mode::Error;
    char* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t file_size_ = 0;
    std::vector<std::uint32_t> line_offsets_;
    int errno_ = 0;
    void freeBuffer();
};

}  // namespace fb
