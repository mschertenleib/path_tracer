#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "vec3.hpp"

struct Camera
{
    vec3 position;
    vec3 target;
    vec3 direction_x; // x points to the right
    vec3 direction_y; // y points down
    vec3 direction_z; // z points into the scene
    float yaw;
    float pitch;
};

// (target - position) and world_up must not be colinear
[[nodiscard]] Camera create_camera(const vec3 &position,
                                   const vec3 &target,
                                   float focal_length,
                                   float sensor_width,
                                   float sensor_height);

void orbital_camera_set_yaw(Camera &camera, float yaw);

void orbital_camera_set_pitch(Camera &camera, float pitch);

void orbital_camera_set_distance(Camera &camera, float distance);

#endif // CAMERA_HPP
