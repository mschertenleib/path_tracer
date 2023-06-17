#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 2, scalar) buffer Vertices
{
    vec3 vertices[];
};

layout(binding = 3, scalar) buffer Indices
{
    uint indices[];
};

layout(location = 0) rayPayloadInEXT vec3 hit_value;

hitAttributeEXT vec3 attributes;

void main()
{
    hit_value = vec3(0.2, 0.5, 0.5);
}
