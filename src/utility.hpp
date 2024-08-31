#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <cassert>
#include <concepts>
#include <cstdint>
#include <string>
#include <vector>

// alignment must be a power of 2
template <std::unsigned_integral U>
[[nodiscard]] constexpr U align_up(U value, U alignment) noexcept
{
    assert((alignment & (alignment - 1)) == 0);
    return (value + (alignment - 1)) & ~(alignment - 1);
}

[[nodiscard]] std::vector<std::uint32_t>
read_binary_file(const char *file_name);

// On failure, returns an error message. On success, returns an empty string.
[[nodiscard]] std::string write_png(const char *file_name,
                                    const std::uint8_t *data,
                                    int width,
                                    int height);

#endif // UTILITY_HPP
