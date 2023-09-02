#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

layout(binding = 2, scalar) buffer Vertices
{
    vec3 vertices[];
};

layout(binding = 3, scalar) buffer Indices
{
    uint indices[];
};

layout(binding = 4, scalar) buffer Normals
{
    vec3 normals[];
};

layout(push_constant) uniform Push_constants
{
    uint rng_seed;
};

layout(location = 0) rayPayloadInEXT vec3 hit_value;

hitAttributeEXT vec2 attributes;

uint hash(uint x)
{
    x += x << 10;
    x ^= x >> 6;
    x += x << 3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

float random(inout uint rng_state)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return float(rng_state) * (1.0 / 4294967295.0);
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

    const vec3 pos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    const vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    // FIXME
    vec3 normal;
    if (normals.length() > 0)
    {
        const vec3 n0 = normals[i0];
        const vec3 n1 = normals[i1];
        const vec3 n2 = normals[i2];
        normal = n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z;
    }
    else
    {
        normal = cross(v1 - v0, v2 - v0);
    }
    const vec3 world_normal = normalize(vec3(gl_ObjectToWorldEXT * vec4(normal, 1.0)));

    hit_value = (world_normal + vec3(1.0)) * 0.5;
}
