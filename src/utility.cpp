#include "utility.hpp"

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <filesystem>
#include <fstream>
#include <sstream>

void Image_deleter::operator()(void *ptr) const
{
    stbi_image_free(ptr);
}

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

Image<std::uint8_t> read_image(const char *file_name)
{
    int width {};
    int height {};
    int channels_in_file {};
    constexpr int desired_channels {4};
    auto *const pixels = stbi_load(
        file_name, &width, &height, &channels_in_file, desired_channels);
    if (!pixels)
    {
        // FIXME
        throw std::runtime_error(stbi_failure_reason());
    }

    return {.data = std::unique_ptr<std::uint8_t, Image_deleter>(pixels),
            .width = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height)};
}

Image<float> read_hdr_image(const char *file_name)
{
    int width {};
    int height {};
    int channels_in_file {};
    constexpr int desired_channels {4};
    auto *const pixels = stbi_loadf(
        file_name, &width, &height, &channels_in_file, desired_channels);
    if (!pixels)
    {
        // FIXME
        throw std::runtime_error(stbi_failure_reason());
    }

    return {.data = std::unique_ptr<float, Image_deleter>(pixels),
            .width = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height)};
}

std::string write_png(const char *file_name,
                      const std::uint8_t *data,
                      int width,
                      int height)
{
    const auto write_result =
        stbi_write_png(file_name, width, height, 4, data, width * 4);
    if (write_result == 0)
    {
        std::ostringstream message;
        message << "Failed to write PNG image to \"" << file_name << '\"';
        return message.str();
    }
    else
    {
        return {};
    }
}
