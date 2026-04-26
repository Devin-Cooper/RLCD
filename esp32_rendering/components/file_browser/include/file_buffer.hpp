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

    std::size_t lineCount() const;
    std::size_t lineOffset(std::size_t line) const;
    std::string_view line(std::size_t line) const;

    int errnoCode() const { return errno_; }

    // Mutation API (Text mode only; returns false on Hex/TooLarge/Error).
    bool insert(std::size_t pos, std::string_view text);
    bool erase(std::size_t pos, std::size_t len);

    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    // Force a line-index rebuild now. Otherwise the index is rebuilt lazily on
    // the next lineCount() / line() / lineOffset() call.
    void rebuildLineIndex();

    // CRLF convention. Sniffed during load; preserved on save.
    bool crlf() const { return crlf_; }

    static constexpr std::size_t kEditCapBytes = 256u * 1024u;

    // Single-level snapshot for undo.
    bool hasSnapshot() const { return snapshot_data_ != nullptr; }
    bool snapshot();
    bool restoreFromSnapshot();

    // Atomic save: write to <path>.tmp, fsync, rename. On failure, the .tmp
    // file is left on disk for forensics. errnoCode() carries the failing
    // POSIX errno on false return.
    bool saveAtomic(const std::string& path);

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
    bool dirty_       = false;
    bool crlf_        = false;
    bool index_dirty_ = false;  // set by insert/erase; cleared by rebuildLineIndex
    char*       snapshot_data_ = nullptr;
    std::size_t snapshot_size_ = 0;
    bool        snapshot_crlf_ = false;
    void freeBuffer();
};

}  // namespace fb
