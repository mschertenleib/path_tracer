#include "geometry.hpp"

#include "tiny_obj_loader.h"

#include <stdexcept>

Scene load_scene(const char *file_name)
{
    tinyobj::ObjReader reader;
    reader.ParseFromFile(file_name);
    if (!reader.Valid())
    {
        throw std::runtime_error(reader.Error());
    }

    const auto &shapes = reader.GetShapes();
    if (shapes.size() != 1)
    {
        throw std::runtime_error("OBJ file contains more than one shape");
    }

    const auto &indices = shapes.front().mesh.indices;

    std::vector<std::uint32_t> vertex_indices(indices.size());
    for (std::size_t i {}; i < indices.size(); ++i)
    {
        vertex_indices[i] = static_cast<std::uint32_t>(indices[i].vertex_index);
    }

    const auto &obj_normals = reader.GetAttrib().normals;
    std::vector<float> normals(reader.GetAttrib().vertices.size());
    for (const auto index : indices)
    {
        const auto vertex_index = static_cast<std::size_t>(index.vertex_index);
        const auto normal_index = static_cast<std::size_t>(index.normal_index);
        normals[vertex_index * 3 + 0] = obj_normals[normal_index * 3 + 0];
        normals[vertex_index * 3 + 1] = obj_normals[normal_index * 3 + 1];
        normals[vertex_index * 3 + 2] = obj_normals[normal_index * 3 + 2];
    }

    const Geometry_mesh mesh {.vertices = reader.GetAttrib().vertices,
                              .indices = vertex_indices,
                              .normals = normals};

    Geometry_instance instance {};
    instance.transform[0][0] = 1.0f;
    instance.transform[1][1] = 1.0f;
    instance.transform[2][2] = 1.0f;
    instance.mesh_index = 0;

    return {.meshes = {mesh}, .instances = {instance}};
}