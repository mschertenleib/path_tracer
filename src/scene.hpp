#ifndef SCENE_HPP
#define SCENE_HPP

#include "vec3.hpp"

#include <vector>

#include <cstdint>

struct Geometry
{
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<float> normals;
};

struct Camera
{
    vec3 position;
    vec3 dir_x;
    vec3 dir_y;
    vec3 dir_z;
};

[[nodiscard]] Geometry load_obj(const char *file_name);

[[nodiscard]] Camera create_camera(const vec3 &position,
                                   const vec3 &look_at,
                                   const vec3 &world_up,
                                   float focal_length,
                                   float sensor_width,
                                   float sensor_height);

#endif // SCENE_HPP