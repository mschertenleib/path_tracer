#version 460

#extension GL_EXT_ray_tracing : require

#include "shader_common.glsl"

layout(binding = 6) uniform sampler2D environment_map;

layout(location = 0) rayPayloadInEXT Ray_payload payload;

void main()
{
#if 1
    const vec3 color_up = vec3(0.75, 0.75, 0.75);
    const vec3 color_down = vec3(0.1, 0.1, 0.1);
    const vec3 sky_color = gl_WorldRayDirectionEXT.y >= 0.0 ? color_up : color_down;
    payload.color = sky_color;
    payload.emissivity = sky_color;
#else
    const float azimuth = atan(gl_WorldRayDirectionEXT.z, gl_WorldRayDirectionEXT.x);
    const float inclination = acos(gl_WorldRayDirectionEXT.y);
    const float u = (azimuth + PI) / (2.0 * PI);
    const float v = inclination / PI;
    const vec3 color = texture(environment_map, vec2(u, v)).rgb;
    payload.color = color;
    payload.emissivity = color;
#endif
    payload.hit_sky = true;
}
