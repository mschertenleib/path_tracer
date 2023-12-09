#version 460

#extension GL_EXT_ray_tracing: require
#extension GL_EXT_scalar_block_layout: require

#include "shader_common.glsl"

layout (binding = 2, scalar) buffer Vertices
{
    vec3 vertices[];
};

layout (binding = 3, scalar) buffer Indices
{
    uint indices[];
};

layout (location = 0) rayPayloadInEXT Ray_payload payload;

hitAttributeEXT vec2 attributes;

// This uses an improved technique by Carsten Wächter and
// Nikolaus Binder from "A Fast and Robust Method for Avoiding
// Self-Intersection" from Ray Tracing Gems (version 1.7, 2020).
// The normal can be negated if one wants the ray to pass through
// the surface instead.
// Source: https://nvpro-samples.github.io/vk_mini_path_tracer/extras.html#raytracingpipelines
vec3 offset_position_along_normal(vec3 world_position, vec3 normal)
{
    // Convert the normal to an integer offset.
    const ivec3 of_i = ivec3(256.0f * normal);

    // Offset each component of worldPosition using its binary representation.
    // Handle the sign bits correctly.
    const vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(world_position.x) + ((world_position.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(world_position.y) + ((world_position.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(world_position.z) + ((world_position.z < 0) ? -of_i.z : of_i.z)));

    // Use a floating-point offset instead for points near (0,0,0), the origin.
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    return vec3(
        abs(world_position.x) < origin ? world_position.x + float_scale * normal.x : p_i.x,
        abs(world_position.y) < origin ? world_position.y + float_scale * normal.y : p_i.y,
        abs(world_position.z) < origin ? world_position.z + float_scale * normal.z : p_i.z);
}

vec3 reflect_diffuse(vec3 normal, inout uint rng_state)
{
    // Random point on a unit sphere centered on the normal
    const float theta = 2.0 * pi * random(rng_state);
    const float z = 2.0 * random(rng_state) - 1.0;
    const float r = sqrt(1.0 - z * z);
    const vec3 direction = normal + vec3(r * cos(theta), r * sin(theta), z);
    return normalize(direction);
}

vec3 reflect_specular(vec3 normal, inout uint rng_state)
{
    return reflect(gl_WorldRayDirectionEXT, normal);
}

void main()
{
    const uint i0 = indices[gl_PrimitiveID * 3 + 0];
    const uint i1 = indices[gl_PrimitiveID * 3 + 1];
    const uint i2 = indices[gl_PrimitiveID * 3 + 2];
    const vec3 v0 = vertices[i0];
    const vec3 v1 = vertices[i1];
    const vec3 v2 = vertices[i2];

    const vec3 barycentrics = vec3(1.0 - attributes.x - attributes.y, attributes.x, attributes.y);
    const vec3 object_position = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    const vec3 world_position = gl_ObjectToWorldEXT * vec4(object_position, 1.0);

    const vec3 object_normal = cross(v1 - v0, v2 - v0);
    // Use the transpose of the inverse matrix for the transformation, because
    // normals are directions, not positions.
    vec3 world_normal = normalize((object_normal * gl_WorldToObjectEXT).xyz);
    world_normal = faceforward(world_normal, gl_WorldRayDirectionEXT, world_normal);

    uint rng_state = payload.rng_state;

    payload.ray_origin = offset_position_along_normal(world_position, world_normal);
    if (gl_PrimitiveID >= 22)
    {
        payload.ray_direction = reflect_specular(world_normal, rng_state);
        payload.color = vec3(0.75);
    }
    else
    {
        payload.ray_direction = reflect_diffuse(world_normal, rng_state);
        payload.color = (world_normal + vec3(1.0)) * 0.5;
    }
    if (gl_PrimitiveID == 2 || gl_PrimitiveID == 3)
    {
        payload.emissivity = vec3(12.0);
    }
    else
    {
        payload.emissivity = vec3(0.0);
    }
    payload.hit_sky = false;
}
