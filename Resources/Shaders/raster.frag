#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertex_interp;
layout(location = 1) in vec3 normal_interp;
layout(location = 2) in vec2 uv_interp;
layout(location = 3) in flat vec3 face_normal;

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_unocclude;

void main()
{
    // https://ndotl.wordpress.com/2018/08/29/baking-artifact-free-lightmaps/
    vec3 delta_uv = max(abs(dFdx(vertex_interp)), abs(dFdy(vertex_interp)));
    float texel_size = max(delta_uv.x, max(delta_uv.y, delta_uv.z));
    texel_size *= sqrt(2.0);
    out_unocclude.xyz = face_normal;
    out_unocclude.w = texel_size;

    out_position = vec4(vertex_interp, 1.0);
    out_normal = vec4(normalize(normal_interp), 1.0);
}