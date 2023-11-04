#include "utility.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOCRYPT
#define NOUSER
#define NOGDI
#define NOSERVICE
#define NOMCX
#define NOIME
#include <windows.h>
#endif

#include <filesystem>
#include <fstream>
#include <iostream>

std::vector<std::uint32_t> read_binary_file(const char *file_name)
{
    const std::filesystem::path path(file_name);
    if (!std::filesystem::exists(path))
    {
        std::ostringstream oss;
        oss << "File " << path << " does not exist";
        throw std::runtime_error(oss.str());
    }

    const auto file_size = std::filesystem::file_size(path);
    const auto buffer_length =
        align_up(file_size, sizeof(std::uint32_t)) / sizeof(std::uint32_t);

    std::vector<std::uint32_t> buffer(buffer_length);

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::ostringstream oss;
        oss << "Failed to open file " << path;
        throw std::runtime_error(oss.str());
    }

    file.read(
        reinterpret_cast<char *>(buffer.data()),
        static_cast<std::streamsize>(buffer_length * sizeof(std::uint32_t)));
    if (file.eof())
    {
        std::ostringstream oss;
        oss << "End-of-file reached while reading file " << path;
        throw std::runtime_error(oss.str());
    }

    return buffer;
}

std::ostream &operator<<(std::ostream &os, Text_color color)
{
#ifdef _WIN32
    const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    switch (color)
    {
    case Text_color::reset: SetConsoleTextAttribute(handle, 0); break;
    case Text_color::green: SetConsoleTextAttribute(handle, 2); break;
    case Text_color::yellow: SetConsoleTextAttribute(handle, 6); break;
    case Text_color::red: SetConsoleTextAttribute(handle, 4); break;
    }
#else
    switch (color)
    {
    case Text_color::reset: os << "\033[0m"; break;
    case Text_color::green: os << "\033[32m"; break;
    case Text_color::yellow: os << "\033[33m"; break;
    case Text_color::red: os << "\033[31m"; break;
    }
#endif
    return os;
}
