#include "file_buffer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifndef RLCD_HOST_TEST
#include "esp_heap_caps.h"
#endif

namespace fb {

namespace {
constexpr std::size_t kMaxFileBytes = 4u * 1024 * 1024;
constexpr std::size_t kSampleBytes  = 1024;
constexpr std::size_t kLineClip     = 4096;

void* psramAlloc(std::size_t n) {
#ifdef RLCD_HOST_TEST
    return std::malloc(n);
#else
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
}
void psramFree(void* p) {
#ifdef RLCD_HOST_TEST
    std::free(p);
#else
    heap_caps_free(p);
#endif
}

bool detectText(const char* sample, std::size_t n) {
    if (n == 0) return true;
    std::size_t nul = 0;
    for (std::size_t i = 0; i < n; ++i) if (sample[i] == 0) ++nul;
    if (nul * 100 > n * 5) return false;
    // Quick UTF-8 leading-byte sanity: count obvious-bad bytes (0xC0/0xC1/0xF5+).
    std::size_t bad = 0;
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)sample[i];
        if (c == 0xC0 || c == 0xC1 || c >= 0xF5) ++bad;
    }
    if (bad * 100 > n * 5) return false;
    return true;
}
}  // namespace

void FileBuffer::freeBuffer() {
    if (data_) { psramFree(data_); data_ = nullptr; }
    size_ = 0;
    file_size_ = 0;
    line_offsets_.clear();
    dirty_ = false;
    crlf_ = false;
    index_dirty_ = false;
}

FileBuffer::~FileBuffer() { freeBuffer(); }

bool FileBuffer::load(const std::string& path) {
    freeBuffer();
    mode_ = Mode::Error;
    errno_ = 0;

    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
        errno_ = errno;
        return false;
    }
    auto fsize = (std::size_t)st.st_size;
    if (fsize >= kMaxFileBytes) {
        // Load only the head + tail (first 32 KB + last 32 KB). Skip body.
        constexpr std::size_t kChunk = 32 * 1024;
        std::size_t want = (fsize > 2 * kChunk) ? 2 * kChunk : fsize;
        data_ = (char*)psramAlloc(want);
        if (!data_) { errno_ = ENOMEM; return false; }
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) { errno_ = errno; freeBuffer(); return false; }
        std::fread(data_, 1, kChunk, f);
        std::fseek(f, (long)(fsize - kChunk), SEEK_SET);
        std::fread(data_ + kChunk, 1, kChunk, f);
        std::fclose(f);
        size_ = want;
        file_size_ = fsize;
        mode_ = Mode::TooLarge;
        return true;
    }

    data_ = (char*)psramAlloc(fsize == 0 ? 1 : fsize);
    if (!data_) { errno_ = ENOMEM; return false; }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { errno_ = errno; freeBuffer(); return false; }
    std::size_t got = std::fread(data_, 1, fsize, f);
    std::fclose(f);
    if (got != fsize) { errno_ = EIO; freeBuffer(); return false; }
    size_ = fsize;
    file_size_ = fsize;

    mode_ = detectText(data_, std::min(size_, kSampleBytes)) ? Mode::Text : Mode::Hex;
    if (mode_ == Mode::Text) {
        // CRLF sniff (first 1 KB scan) — preserved verbatim on save.
        crlf_ = false;
        {
            std::size_t scan = std::min(size_, std::size_t{1024});
            for (std::size_t i = 0; i + 1 < scan; ++i) {
                if (data_[i] == '\r' && data_[i + 1] == '\n') { crlf_ = true; break; }
            }
        }
        // Build line offsets — start of each line.
        if (size_ > 0) {
            line_offsets_.push_back(0);
            for (std::size_t i = 0; i < size_; ++i) {
                if (data_[i] == '\n' && i + 1 < size_) line_offsets_.push_back((std::uint32_t)(i + 1));
            }
        }
    }
    return true;
}

std::size_t FileBuffer::lineCount() const {
    // Lazy rebuild idiom: insert/erase set index_dirty_ without touching
    // line_offsets_; the next read accessor pays the rebuild cost.
    if (index_dirty_) const_cast<FileBuffer*>(this)->rebuildLineIndex();
    return line_offsets_.size();
}

std::size_t FileBuffer::lineOffset(std::size_t line) const {
    if (index_dirty_) const_cast<FileBuffer*>(this)->rebuildLineIndex();
    if (line >= line_offsets_.size()) return size_;
    return line_offsets_[line];
}

std::string_view FileBuffer::line(std::size_t line) const {
    if (index_dirty_) const_cast<FileBuffer*>(this)->rebuildLineIndex();
    if (line >= line_offsets_.size()) return {};
    std::size_t a = line_offsets_[line];
    std::size_t b = (line + 1 < line_offsets_.size()) ? line_offsets_[line + 1]
                                                       : size_;
    if (b > 0 && b <= size_ && data_[b - 1] == '\n') --b;
    if (b > 0 && b <= size_ && data_[b - 1] == '\r') --b;
    std::string_view raw(data_ + a, b - a);

    // Tab expansion + clip into a thread_local buffer (host tests are
    // single-threaded; on target the screen render thread is the only caller).
    thread_local static char buf[kLineClip + 1];
    std::size_t out = 0;
    for (std::size_t i = 0; i < raw.size() && out < kLineClip; ++i) {
        char c = raw[i];
        if (c == '\t') {
            std::size_t pad = 4 - (out % 4);
            for (std::size_t k = 0; k < pad && out < kLineClip; ++k) buf[out++] = ' ';
        } else {
            buf[out++] = c;
        }
    }
    return std::string_view(buf, out);
}

std::vector<std::uint32_t> FileBuffer::findAll(std::string_view needle) const {
    if (index_dirty_) const_cast<FileBuffer*>(this)->rebuildLineIndex();
    std::vector<std::uint32_t> out;
    if (needle.empty() || mode_ != Mode::Text || size_ == 0) return out;

    auto to_lower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    };

    for (std::uint32_t li = 0; li < line_offsets_.size(); ++li) {
        std::size_t a = line_offsets_[li];
        std::size_t b = (li + 1 < line_offsets_.size()) ? line_offsets_[li + 1] : size_;
        if (b - a < needle.size()) continue;
        for (std::size_t i = a; i + needle.size() <= b; ++i) {
            bool eq = true;
            for (std::size_t k = 0; k < needle.size(); ++k) {
                if (to_lower(data_[i + k]) != to_lower(needle[k])) { eq = false; break; }
            }
            if (eq) { out.push_back(li); break; }
        }
    }
    return out;
}

void FileBuffer::formatHexRow(const char* base, std::size_t base_size,
                              std::uint32_t offset, char out[80]) {
    // IMPORTANT: do NOT use snprintf for the field writes — its NUL
    // terminator would land in the middle of `out` (e.g. at offset 54 after
    // the 16th hex column), truncating the C-string before the trailing
    // '|' and ASCII gutter when handed to drawBitmapText. Hand-format with
    // an offset-only snprintf at the head and direct byte writes after.
    static const char kHex[] = "0123456789ABCDEF";
    std::memset(out, ' ', 79); out[79] = 0;

    // Address column: write 4 hex digits at out[0..3] without snprintf's
    // trailing NUL clobbering out[4].
    out[0] = kHex[(offset >> 12) & 0xF];
    out[1] = kHex[(offset >>  8) & 0xF];
    out[2] = kHex[(offset >>  4) & 0xF];
    out[3] = kHex[(offset >>  0) & 0xF];
    // out[4] and out[5] stay as the two-space separator from memset.

    // 16 hex columns at out[6..53] (3 chars per byte: "XX ").
    for (int i = 0; i < 16; ++i) {
        std::size_t idx = (std::size_t)offset + i;
        char* slot = out + 6 + i * 3;
        if (idx < base_size) {
            unsigned char b = (unsigned char)base[idx];
            slot[0] = kHex[(b >> 4) & 0xF];
            slot[1] = kHex[b & 0xF];
        }
        // slot[2] stays as the ' ' from memset.
    }
    // Midpoint extra-space already a ' ' from memset; explicit assignment
    // preserved for clarity.
    out[6 + 8 * 3 - 1] = ' ';
    out[54] = ' ';  // ensure no NUL lingers between hex columns and '|'.
    out[55] = '|';
    for (int i = 0; i < 16; ++i) {
        std::size_t idx = (std::size_t)offset + i;
        unsigned char c = (idx < base_size) ? (unsigned char)base[idx] : 0;
        out[56 + i] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    out[72] = '|';
    out[73] = 0;
}

bool FileBuffer::insert(std::size_t pos, std::string_view text) {
    if (mode_ != Mode::Text) return false;
    if (pos > size_) return false;
    if (text.empty()) return true;
    if (size_ + text.size() > kEditCapBytes) return false;

    std::size_t new_size = size_ + text.size();
    char* nb = (char*)psramAlloc(new_size);
    if (!nb) return false;
    if (pos > 0) std::memcpy(nb, data_, pos);
    std::memcpy(nb + pos, text.data(), text.size());
    if (pos < size_) std::memcpy(nb + pos + text.size(), data_ + pos, size_ - pos);
    psramFree(data_);
    data_ = nb;
    size_ = new_size;
    dirty_ = true;
    index_dirty_ = true;
    return true;
}

bool FileBuffer::erase(std::size_t pos, std::size_t len) {
    if (mode_ != Mode::Text) return false;
    if (pos > size_) return false;
    if (len == 0) return true;
    if (pos + len > size_) len = size_ - pos;

    std::size_t new_size = size_ - len;
    if (new_size == 0) {
        psramFree(data_);
        data_ = (char*)psramAlloc(1);  // sentinel non-null
        if (!data_) { size_ = 0; return false; }
        size_ = 0;
        dirty_ = true; index_dirty_ = true;
        return true;
    }
    char* nb = (char*)psramAlloc(new_size);
    if (!nb) return false;
    if (pos > 0) std::memcpy(nb, data_, pos);
    if (pos + len < size_) std::memcpy(nb + pos, data_ + pos + len, size_ - pos - len);
    psramFree(data_);
    data_ = nb;
    size_ = new_size;
    dirty_ = true;
    index_dirty_ = true;
    return true;
}

void FileBuffer::rebuildLineIndex() {
    line_offsets_.clear();
    if (size_ > 0) {
        line_offsets_.push_back(0);
        for (std::size_t i = 0; i < size_; ++i) {
            if (data_[i] == '\n' && i + 1 < size_) {
                line_offsets_.push_back((std::uint32_t)(i + 1));
            }
        }
    }
    index_dirty_ = false;
}

}  // namespace fb
