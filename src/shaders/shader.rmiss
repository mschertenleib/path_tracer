#version 460

#extension GL_EXT_ray_tracing : require

#include "shader_common.glsl"

layout(location = 0) rayPayloadInEXT Ray_payload payload;

void main()
{
    const vec3 color_up = vec3(0.75, 0.75, 0.75);
    const vec3 color_down = vec3(0.1, 0.1, 0.1);
    const vec3 sky_color = gl_WorldRayDirectionEXT.y >= 0.0 ? color_up : color_down;
    payload.color = sky_color;
    payload.emissivity = sky_color;
    payload.hit_sky = true;
}
