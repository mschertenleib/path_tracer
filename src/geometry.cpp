#include "geometry.hpp"

#include "tiny_obj_loader.h"

#include <stdexcept>

#include <cmath>

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
    /*
    // z points into the scene
    camera.dir_z = normalize(look_at - position);
    // x points to the right
    camera.dir_x = normalize(cross(camera_z, world_up));
    // y points down
    camera.dir_y = cross(camera_z, camera_x);
    camera.dir_z *= focal_length;
    camera.dir_x *= sensor_width * 0.5f;
    camera.dir_y *= sensor_height * 0.5f;
     */

    // FIXME: this is temporary

    const auto normalize = [](const vec3 &v)
    {
        const auto norm = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return vec3 {v.x / norm, v.y / norm, v.z / norm};
    };

    const auto sub = [](const vec3 &u, const vec3 &v) {
        return vec3 {u.x - v.x, u.y - v.y, u.z - v.z};
    };

    const auto mul = [](const vec3 &u, float f) {
        return vec3 {u.x * f, u.y * f, u.z * f};
    };

    const auto cross = [](const vec3 &u, const vec3 &v)
    {
        return vec3 {u.y * v.z - u.z * v.y,
                     u.z * v.x - u.x * v.z,
                     u.x * v.y - u.y * v.x};
    };

    Camera camera {};
    camera.position = position;
    camera.dir_z = normalize(sub(look_at, position));
    camera.dir_x = normalize(cross(camera.dir_z, world_up));
    camera.dir_y = cross(camera.dir_z, camera.dir_x);
    camera.dir_z = mul(camera.dir_z, focal_length);
    camera.dir_x = mul(camera.dir_x, sensor_width * 0.5f);
    camera.dir_y = mul(camera.dir_y, sensor_height * 0.5f);

    return camera;
}
