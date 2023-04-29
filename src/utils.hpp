#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdint>
#include <filesystem>
#include <source_location>
#include <vector>

[[noreturn]] void
fatal_error(std::string_view message,
            const std::source_location &loc = std::source_location::current());

[[nodiscard]] std::vector<std::uint32_t>
read_binary_file(const std::filesystem::path &path);

#endif // UTILS_HPP
