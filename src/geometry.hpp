#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <vector>

#include <cstdint>

struct Geometry
{
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<float> normals;
};

[[nodiscard]] Geometry load_obj(const char *file_name);

#endif // GEOMETRY_HPP
