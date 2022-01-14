#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_unocclude;

void main()
{
    out_position = vec4(0.0, 0.0, 1.0, 1.0);
    out_normal = vec4(0.0, 0.0, 1.0, 1.0);
    out_unocclude = vec4(0.0, 0.0, 1.0, 1.0);
}