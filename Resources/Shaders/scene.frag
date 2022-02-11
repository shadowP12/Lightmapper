#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1000) uniform texture2D light_texture;
layout(binding = 1001) uniform texture2DArray sh_light_map;
layout(binding = 3000) uniform sampler linear_sampler;

layout(location = 0) in vec2 v_uv0;
layout(location = 1) in vec2 v_uv1;
layout(location = 2) in vec4 v_color;
layout(location = 3) in vec3 v_normal;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 lm_light_l0 = texture(sampler2DArray(sh_light_map, linear_sampler), vec3(v_uv1, 0.0)).rgb;
    vec3 lm_light_l1n1 = texture(sampler2DArray(sh_light_map, linear_sampler), vec3(v_uv1, 1.0)).rgb;
    vec3 lm_light_l1_0 = texture(sampler2DArray(sh_light_map, linear_sampler), vec3(v_uv1, 2.0)).rgb;
    vec3 lm_light_l1p1 = texture(sampler2DArray(sh_light_map, linear_sampler), vec3(v_uv1, 3.0)).rgb;

    vec3 n = v_normal;
    vec3 ambient_light = lm_light_l0 * 0.282095;
    ambient_light += lm_light_l1n1 * 0.32573 * n.y;
    ambient_light += lm_light_l1_0 * 0.32573 * n.z;
    ambient_light += lm_light_l1p1 * 0.32573 * n.x;
    out_color = vec4(ambient_light, 1.0);
    //out_color = texture(sampler2D(light_texture, linear_sampler), v_uv1);
}