#version 460

#include "closest_hit_common.glsl"

void main()
{
    const Hit hit = get_hit();

    payload.color = vec3(1.0, 1.0, 1.0);
    payload.emissivity = vec3(0.0);
    payload.hit_sky = false;

    const bool into = dot(gl_WorldRayDirectionEXT, hit.world_normal) < 0.0;
    const vec3 normal = into ? hit.world_normal : -hit.world_normal;
    const vec3 reflected_dir = reflect(gl_WorldRayDirectionEXT, normal);
    const float n_air = 1.0;
    const float n_glass = 1.5;
    const float n_ratio = into ? n_air / n_glass : n_glass / n_air;
    const float dir_dot_normal = dot(gl_WorldRayDirectionEXT, normal);
    const float cos2t = 1.0 - n_ratio * n_ratio * (1.0 - dir_dot_normal * dir_dot_normal);
    // Total internal reflection
    if (cos2t < 0.0)
    {
        payload.ray_origin = offset_position_along_normal(hit.world_position, normal);
        payload.ray_direction = reflected_dir;
        return;
    }

    const vec3 transmitted_dir = normalize(gl_WorldRayDirectionEXT * n_ratio - hit.world_normal *
        ((into ? 1.0 : -1.0) * (dir_dot_normal * n_ratio + sqrt(cos2t))));
    const float a = n_glass - n_air;
    const float b = n_glass + n_air;
    const float R0 = a * a / (b * b);
    const float c = 1.0 - (into ? -dir_dot_normal : dot(transmitted_dir, hit.world_normal));
    const float Re = R0 + (1.0 - R0) * c * c * c * c * c;
    const float Tr = 1.0 - Re;
    const float P = 0.25 + 0.5 * Re;
    const float RP = Re / P;
    const float TP = Tr / (1.0 - P);
    if (random(payload.rng_state) < P)
    {
        payload.reflectance_attenuation = RP;
        payload.ray_origin = offset_position_along_normal(hit.world_position, normal);
        payload.ray_direction = reflected_dir;
    }
    else
    {
        payload.reflectance_attenuation = TP;
        payload.ray_origin = offset_position_along_normal(hit.world_position, -normal);
        payload.ray_direction = transmitted_dir;
    }
}
