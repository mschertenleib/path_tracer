#version 460

#extension GL_EXT_ray_tracing : require

#include "shader_common.glsl"

layout(location = 0) rayPayloadInEXT Ray_payload payload;

void main()
{
    payload.color = gl_WorldRayDirectionEXT.y >= 0.0 ? vec3(0.75) : vec3(0.0);
    payload.hit_sky = true;
}
