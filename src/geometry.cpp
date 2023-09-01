#include "geometry.hpp"

#include "tiny_obj_loader.h"

#include <stdexcept>

Geometry load_obj(const char *file_name)
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

    std::vector<float> normals;
    if (const auto &obj_normals = reader.GetAttrib().normals;
        !obj_normals.empty())
    {
        normals.resize(reader.GetAttrib().vertices.size());
        for (const auto index : indices)
        {
            const auto vertex_index =
                static_cast<std::size_t>(index.vertex_index);
            const auto normal_index =
                static_cast<std::size_t>(index.normal_index);
            normals[vertex_index * 3 + 0] = obj_normals[normal_index * 3 + 0];
            normals[vertex_index * 3 + 1] = obj_normals[normal_index * 3 + 1];
            normals[vertex_index * 3 + 2] = obj_normals[normal_index * 3 + 2];
        }
    }

    return Geometry {.vertices = reader.GetAttrib().vertices,
                     .indices = vertex_indices,
                     .normals = normals};
}