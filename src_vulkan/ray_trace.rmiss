#version 460

#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hit_value;

void main()
{
    const vec3 up = vec3(0.0, 1.0, 0.0);
    const float cosine_ray_angle = dot(gl_WorldRayDirectionEXT, up);
    const vec3 horizon_color = vec3(0.7, 0.7, 0.7);
    const vec3 zenith_color = vec3(0.0, 0.05, 0.15);
    const vec3 sky_color = mix(horizon_color, zenith_color, pow(cosine_ray_angle, 1.0 / 5.0));
    hit_value = cosine_ray_angle >= 0 ? sky_color : vec3(0.0);
}
