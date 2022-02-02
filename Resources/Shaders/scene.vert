#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv0;
layout(location = 3) in vec2 a_uv1;

layout(location = 0) out vec2 v_uv0;
layout(location = 1) out vec2 v_uv1;

layout(std140, set = 0, binding = 0) uniform ObjectUniforms {
    mat4 model_matrix;
    mat4 view_matrix;
    mat4 proj_matrix;
} object_uniforms;

void main() 
{
    v_uv0 = a_uv0;
    v_uv1 = a_uv1;
    gl_Position = object_uniforms.proj_matrix * object_uniforms.view_matrix * object_uniforms.model_matrix * vec4(a_position, 1.0);
}