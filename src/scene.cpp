#include "scene.hpp"

Camera create_camera(const vec3 &position,
                     const vec3 &look_at,
                     const vec3 &world_up,
                     float focal_length,
                     float sensor_width,
                     float sensor_height)
{
    Camera camera {};
    camera.position = position;
    camera.look_at = look_at;
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
