#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

namespace app {

// Minimal span polyfill: a view over a contiguous range of T. C++17-safe.
// Used in place of std::span (C++20). API surface is intentionally tiny.
template <typename T>
class SpanView {
public:
    constexpr SpanView() noexcept : data_(nullptr), size_(0) {}
    constexpr SpanView(T* data, std::size_t n) noexcept : data_(data), size_(n) {}
    template <std::size_t N>
    constexpr SpanView(T (&arr)[N]) noexcept : data_(arr), size_(N) {}
    template <std::size_t N>
    constexpr SpanView(std::array<T, N>& arr) noexcept : data_(arr.data()), size_(N) {}
    template <std::size_t N>
    constexpr SpanView(const std::array<typename std::remove_const<T>::type, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    constexpr T* data() const noexcept { return data_; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end()   const noexcept { return data_ + size_; }
    constexpr T& operator[](std::size_t i) const noexcept { return data_[i]; }

private:
    T* data_;
    std::size_t size_;
};

} // namespace app
