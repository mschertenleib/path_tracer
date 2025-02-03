#include "camera.hpp"

#include <cmath>

namespace
{

/*[[nodiscard]] mat4x4
make_perspective(float vertical_fov, float aspect, float z_near)
{
    const auto range = std::tan(vertical_fov * 0.5f) * z_near;

    mat4x4 perspective {};
    perspective.m[0][0] = z_near / (range * aspect);
    perspective.m[1][1] = z_near / range;
    perspective.m[2][2] = -1.0f;
    perspective.m[2][3] = -1.0f;
    perspective.m[3][2] = -z_near;

    return perspective;
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
    camera.position =
        camera.target - camera.distance * normalize(camera.direction_z);
    camera.pitch = pitch;
}*/

} // namespace

Camera create_camera(const vec3 &position,
                     const vec3 &target,
                     float sensor_distance,
                     float sensor_half_width,
                     float sensor_half_height)
{
    constexpr vec3 world_up {0.0f, 1.0f, 0.0f};

    Camera camera {};
    camera.distance = norm(target - position);
    camera.position = position;
    camera.target = target;
    camera.direction_z = normalize(target - position);
    camera.direction_x = normalize(cross(camera.direction_z, world_up));
    camera.direction_y =
        normalize(cross(camera.direction_z, camera.direction_x));

    camera.sensor_distance = sensor_distance;
    camera.sensor_half_width = sensor_half_width;
    camera.sensor_half_height = sensor_half_height;

    camera.focus_distance = camera.distance;
    camera.aperture_radius = 0.0f;

    return camera;
}

void camera_set_distance(Camera &camera, float distance)
{
    camera.distance = distance;
    camera.position = camera.target - distance * normalize(camera.direction_z);
}
