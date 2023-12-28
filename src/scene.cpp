#include "scene.hpp"

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

Camera create_camera(const vec3 &position,
                     const vec3 &look_at,
                     const vec3 &world_up,
                     float focal_length,
                     float sensor_width,
                     float sensor_height)
{
    Camera camera {};
    camera.position = position;
    // z points into the scene
    camera.dir_z = normalize(look_at - position);
    // x points to the right
    camera.dir_x = normalize(cross(camera.dir_z, world_up));
    // y points down
    camera.dir_y = cross(camera.dir_z, camera.dir_x);
    camera.dir_z *= focal_length;
    camera.dir_x *= sensor_width * 0.5f;
    camera.dir_y *= sensor_height * 0.5f;

    return camera;
}
