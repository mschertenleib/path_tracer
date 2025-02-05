#version 460

#include "closest_hit_common.glsl"

void main()
{
    const Hit hit = get_hit();

    payload.ray_origin = offset_position_along_normal(hit.world_position, hit.world_normal);
    payload.ray_direction = reflect(gl_WorldRayDirectionEXT, hit.world_normal);
    payload.color = (hit.world_normal + vec3(1.0)) * 0.5;
    payload.emissivity = vec3(0.0);
    payload.hit_sky = false;
}
