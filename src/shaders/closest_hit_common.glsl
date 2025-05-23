#extension GL_EXT_ray_tracing: require
#extension GL_EXT_scalar_block_layout: require

#include "shader_common.glsl"


layout (binding = 2, scalar) restrict readonly buffer Vertices { vec3 vertices[]; };
layout (binding = 3, scalar) restrict readonly buffer Indices { uint indices[]; };
layout (binding = 5, scalar) restrict readonly buffer Normals { vec3 normals[]; };

layout (location = 0) rayPayloadInEXT Ray_payload payload;

hitAttributeEXT vec2 attributes;

struct Hit
{
    vec3 world_position;
    vec3 world_normal;
};


// By Carsten Wächter and Nikolaus Binder from "A Fast and Robust
// Method for Avoiding Self-Intersection" from Ray Tracing Gems
// (version 1.7, 2020).
vec3 offset_position_along_normal(vec3 position, vec3 normal)
{
    // Convert the normal to an integer offset.
    const ivec3 of_i = ivec3(256.0 * normal);

    // Offset each component of position using its binary representation.
    // Handle the sign bits correctly.
    const vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(position.x) + ((position.x < 0.0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(position.y) + ((position.y < 0.0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(position.z) + ((position.z < 0.0) ? -of_i.z : of_i.z)));

    // Use a floating-point offset instead for points near (0,0,0), the origin.
    const float origin = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    return vec3(
        abs(position.x) < origin ? position.x + float_scale * normal.x : p_i.x,
        abs(position.y) < origin ? position.y + float_scale * normal.y : p_i.y,
        abs(position.z) < origin ? position.z + float_scale * normal.z : p_i.z);
}

vec3 sample_sphere(inout uint rng_state)
{
    const float theta = 2.0 * PI * random(rng_state);
    const float z = 2.0 * random(rng_state) - 1.0;
    const float r = sqrt(1.0 - z * z);
    return vec3(r * cos(theta), r * sin(theta), z);
}

vec3 reflect_diffuse(vec3 normal, inout uint rng_state)
{
    return normalize(normal + sample_sphere(rng_state));
}

Hit get_hit()
{
    const uint i0 = indices[gl_PrimitiveID * 3 + 0];
    const uint i1 = indices[gl_PrimitiveID * 3 + 1];
    const uint i2 = indices[gl_PrimitiveID * 3 + 2];
    const vec3 v0 = vertices[i0];
    const vec3 v1 = vertices[i1];
    const vec3 v2 = vertices[i2];
    const vec3 n0 = normals[i0];
    const vec3 n1 = normals[i1];
    const vec3 n2 = normals[i2];

    const vec3 barycentrics = vec3(1.0 - attributes.x - attributes.y, attributes.x, attributes.y);
    const vec3 object_position = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
#if 1
    const vec3 object_normal = n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z;
#else
    const vec3 object_normal = cross(v1 - v0, v2 - v0);
#endif

    // TODO: properly understand this
    // Use the transpose of the inverse matrix for the transformation, because
    // normals are directions, not positions.
    const vec3 world_normal = normalize((object_normal * gl_WorldToObjectEXT).xyz);
    
    Hit hit;
    hit.world_position = gl_ObjectToWorldEXT * vec4(object_position, 1.0);
    hit.world_normal = faceforward(world_normal, gl_WorldRayDirectionEXT, world_normal);

    return hit;
}
