#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "vec3.hpp"

struct Camera
{
    vec3 position;
    vec3 target;
    vec3 direction_x;
    vec3 direction_y;
    vec3 direction_z;
};

[[nodiscard]] Camera create_camera(const vec3 &position,
                                   const vec3 &target,
                                   const vec3 &world_up,
                                   float focal_length,
                                   float sensor_width,
                                   float sensor_height);

#endif // CAMERA_HPP
