#version 460

#include "closest_hit_common.glsl"


vec3 sample_sphere(inout uint rng_state)
{
    const float theta = 2.0 * pi * random(rng_state);
    const float z = 2.0 * random(rng_state) - 1.0;
    const float r = sqrt(1.0 - z * z);
    return vec3(r * cos(theta), r * sin(theta), z);
}

vec3 reflect_diffuse(vec3 normal, inout uint rng_state)
{
    return normalize(normal + sample_sphere(rng_state));
}

void main()
{
    const Hit hit = get_hit();

    payload.ray_origin = offset_position_along_normal(hit.world_position, hit.world_normal);
    payload.ray_direction = reflect_diffuse(hit.world_normal, payload.rng_state);
    payload.color = (hit.world_normal + vec3(1.0)) * 0.5;
    payload.emissivity = vec3(0.0);
    payload.hit_sky = false;
}
