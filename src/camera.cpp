#include "camera.hpp"

#include <cmath>

Camera create_camera(const vec3 &position,
                     const vec3 &target,
                     float focal_length,
                     float sensor_width,
                     float sensor_height)
{
    constexpr vec3 world_up {0.0f, 1.0f, 0.0f};

    Camera camera {};
    camera.position = position;
    camera.target = target;
    camera.direction_z = normalize(target - position);
    camera.direction_x = normalize(cross(camera.direction_z, world_up));
    camera.direction_y = cross(camera.direction_z, camera.direction_x);
    camera.direction_z *= focal_length;
    camera.direction_x *= sensor_width * 0.5f;
    camera.direction_y *= sensor_height * 0.5f;

    // FIXME: this assumes world Y is up
    camera.yaw = std::atan2(-camera.direction_z.z, camera.direction_z.x);
    camera.pitch =
        std::atan2(camera.direction_z.y,
                   std::hypot(camera.direction_z.x, camera.direction_z.z));

    return camera;
}

void orbital_camera_set_yaw(Camera &camera, float yaw)
{
    const auto angle = yaw - camera.yaw;
    const auto cos_angle = std::cos(angle);
    const auto sin_angle = std::sin(angle);
    const auto rotate = [cos_angle, sin_angle](const vec3 &v)
    {
        return vec3 {cos_angle * v.x + sin_angle * v.z,
                     v.y,
                     -sin_angle * v.x + cos_angle * v.z};
    };
    camera.direction_x = rotate(camera.direction_x);
    camera.direction_y = rotate(camera.direction_y);
    camera.direction_z = rotate(camera.direction_z);
    camera.position = rotate(camera.position - camera.target) + camera.target;
    camera.yaw = yaw;
}

void orbital_camera_set_pitch(Camera &camera, float pitch)
{
    const auto angle = pitch - camera.pitch;
    const auto cos_angle = std::cos(angle);
    const auto sin_angle = std::sin(angle);
    const auto norm_direction_y = norm(camera.direction_y);
    const auto norm_direction_z = norm(camera.direction_z);
    const auto normalized_direction_y =
        camera.direction_y * (1.0f / norm_direction_y);
    const auto normalized_direction_z =
        camera.direction_z * (1.0f / norm_direction_z);
    camera.direction_y = (cos_angle * normalized_direction_y +
                          sin_angle * normalized_direction_z) *
                         norm_direction_y;
    camera.direction_z = (cos_angle * normalized_direction_z -
                          sin_angle * normalized_direction_y) *
                         norm_direction_z;
    camera.position = camera.target - norm(camera.target - camera.position) *
                                          normalize(camera.direction_z);
    camera.pitch = pitch;
}

void orbital_camera_set_distance(Camera &camera, float distance)
{
    camera.position = camera.target - distance * normalize(camera.direction_z);
}