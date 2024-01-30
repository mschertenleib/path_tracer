#include "camera.hpp"

Camera create_camera(const vec3 &position,
                     const vec3 &target,
                     const vec3 &world_up,
                     float focal_length,
                     float sensor_width,
                     float sensor_height)
{
    Camera camera {};
    camera.position = position;
    camera.target = target;
    // z points into the scene
    camera.direction_z = normalize(target - position);
    // x points to the right
    camera.direction_x = normalize(cross(camera.direction_z, world_up));
    // y points down
    camera.direction_y = cross(camera.direction_z, camera.direction_x);
    camera.direction_z *= focal_length;
    camera.direction_x *= sensor_width * 0.5f;
    camera.direction_y *= sensor_height * 0.5f;

    return camera;
}
