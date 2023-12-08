#version 460

#extension GL_EXT_ray_tracing: require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_debug_printf: enable

layout (binding = 2, scalar) buffer Vertices
{
    vec3 vertices[];
};

layout (binding = 3, scalar) buffer Indices
{
    uint indices[];
};

layout (location = 0) rayPayloadInEXT vec3 hit_value;

hitAttributeEXT vec2 attributes;

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

    const vec3 normal = cross(v1 - v0, v2 - v0);
    const vec3 world_normal = normalize(vec3(gl_ObjectToWorldEXT * vec4(normal, 1.0)));

    hit_value = (world_normal + vec3(1.0)) * 0.5;
}
