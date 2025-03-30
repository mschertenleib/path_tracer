#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Image_deleter
{
    void operator()(void *ptr) const;
};

template <typename T>
struct Image
{
    std::unique_ptr<T, Image_deleter> data;
    std::uint32_t width;
    std::uint32_t height;
};

// alignment must be a power of 2
template <std::unsigned_integral U>
[[nodiscard]] constexpr U align_up(U value, U alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0);
    return (value + (alignment - 1)) & ~(alignment - 1);
}

[[nodiscard]] std::vector<std::uint32_t>
read_binary_file(const char *file_name);

[[nodiscard]] Image<std::uint8_t> read_image(const char *file_name);

[[nodiscard]] Image<float> read_hdr_image(const char *file_name);

// On failure, returns an error message. On success, returns an empty string.
[[nodiscard]] std::string write_png(const char *file_name,
                                    const std::uint8_t *data,
                                    int width,
                                    int height);

#endif // UTILITY_HPP
