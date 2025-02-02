#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "vec3.hpp"

struct Camera
{
    float distance;
    vec3 position;
    vec3 target;
    vec3 direction_x; // x points to the right
    vec3 direction_y; // y points down
    vec3 direction_z; // z points forward
    float yaw;
    float pitch;
    float focus_distance;
    float aperture_radius;
};

constexpr void invert_transform(float dst[4][4], const float src[4][4]) noexcept
{
    dst[0][0] = src[0][0];
    dst[0][1] = src[1][0];
    dst[0][2] = src[2][0];
    dst[0][3] = 0.0f;
    dst[1][0] = src[0][1];
    dst[1][1] = src[1][1];
    dst[1][2] = src[2][1];
    dst[1][3] = 0.0f;
    dst[2][0] = src[0][2];
    dst[2][1] = src[1][2];
    dst[2][2] = src[2][2];
    dst[2][3] = 0.0f;
    const vec3 t {src[3][0], src[3][1], src[3][2]};
    dst[3][0] = -dot(vec3 {dst[0][0], dst[1][0], dst[2][0]}, t);
    dst[3][1] = -dot(vec3 {dst[0][1], dst[1][1], dst[2][1]}, t);
    dst[3][2] = -dot(vec3 {dst[0][2], dst[1][2], dst[2][2]}, t);
    dst[3][3] = 1.0f;
}

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
