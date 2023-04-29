#version 450

layout(binding = 0) uniform sampler2D render_target;

layout(location = 0) in vec2 in_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(render_target, in_tex_coord);
}
