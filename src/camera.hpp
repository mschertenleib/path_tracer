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

struct mat4x4
{
    float m[4][4];

    [[nodiscard]] constexpr bool
    operator==(const mat4x4 &other) const noexcept = default;
    [[nodiscard]] constexpr bool
    operator!=(const mat4x4 &other) const noexcept = default;
};

[[nodiscard]] constexpr mat4x4
invert_rigid_transform(const mat4x4 &matrix) noexcept
{
    mat4x4 result {};

    result.m[0][0] = matrix.m[0][0];
    result.m[0][1] = matrix.m[1][0];
    result.m[0][2] = matrix.m[2][0];
    result.m[0][3] = 0.0f;
    result.m[1][0] = matrix.m[0][1];
    result.m[1][1] = matrix.m[1][1];
    result.m[1][2] = matrix.m[2][1];
    result.m[1][3] = 0.0f;
    result.m[2][0] = matrix.m[0][2];
    result.m[2][1] = matrix.m[1][2];
    result.m[2][2] = matrix.m[2][2];
    result.m[2][3] = 0.0f;
    const vec3 t {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
    result.m[3][0] =
        -dot(vec3 {result.m[0][0], result.m[1][0], result.m[2][0]}, t);
    result.m[3][1] =
        -dot(vec3 {result.m[0][1], result.m[1][1], result.m[2][1]}, t);
    result.m[3][2] =
        -dot(vec3 {result.m[0][2], result.m[1][2], result.m[2][2]}, t);
    result.m[3][3] = 1.0f;

    return result;
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
