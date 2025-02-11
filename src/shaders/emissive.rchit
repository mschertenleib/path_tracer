#version 460

#include "closest_hit_common.glsl"

void main()
{
    const Hit hit = get_hit();

    payload.ray_origin = offset_position_along_normal(hit.world_position, hit.world_normal);
    payload.ray_direction = reflect_diffuse(hit.world_normal, payload.rng_state);
    payload.color = vec3(0.75, 0.75, 0.75);
    payload.emissivity = vec3(5.0, 5.0, 5.0);
    payload.hit_sky = false;
}
