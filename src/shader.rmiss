#version 460

#extension GL_EXT_ray_tracing : require

#include "shader_common.glsl"

layout(location = 0) rayPayloadInEXT Ray_payload payload;

void main()
{
    const vec3 color_up = vec3(1.0);
    const vec3 color_down = vec3(0.0);
    payload.color = mix(color_down, color_up, gl_WorldRayDirectionEXT.y * 0.5 + 0.5);
    payload.emissivity = mix(color_down, color_up, gl_WorldRayDirectionEXT.y * 0.5 + 0.5);
    payload.hit_sky = true;
}
