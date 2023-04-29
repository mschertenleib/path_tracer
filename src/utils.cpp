#include "utils.hpp"

#include <fstream>
#include <iostream>

void fatal_error(std::string_view message, const std::source_location &loc)
{
    std::ostringstream oss;
    oss << loc.file_name() << ':' << loc.line() << ": fatal error in \'"
        << loc.function_name() << "\':\n"
        << message << '\n';
    throw std::runtime_error(oss.str());
}

std::vector<std::uint32_t> read_binary_file(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path))
    {
        fatal_error("File \"" + path.string() + "\" does not exist\n");
    }

    const auto size = std::filesystem::file_size(path);
    std::vector<std::uint32_t> data((size + 3) / 4);

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        fatal_error("Failed to open file \"" + path.string() + "\"\n");
    }

    file.read(reinterpret_cast<char *>(data.data()),
              static_cast<std::streamsize>(size));

    return data;
}
