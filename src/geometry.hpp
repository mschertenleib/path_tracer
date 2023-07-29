#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <cstdint>
#include <vector>

struct Geometry_mesh
{
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<float> normals;
};

struct Geometry_instance
{
    float transform[3][4];
    std::uint32_t mesh_index;
};

struct Scene
{
    std::vector<Geometry_mesh> meshes;
    std::vector<Geometry_instance> instances;
};

[[nodiscard]] Scene load_scene(const char *file_name);

#endif // GEOMETRY_HPP
