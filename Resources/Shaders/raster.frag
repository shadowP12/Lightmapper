#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex {
    vec4 position;
    vec4 normal;
    vec2 uv0;
    vec2 uv1;
};

layout(set = 0, binding = 2000, std430) restrict readonly buffer Vertices {
    Vertex data[];
} vertices;

struct Triangle {
    uvec4 indices;
    vec4 min_bounds;
    vec4 max_bounds;
};

layout(set = 0, binding = 2001, std430) restrict readonly buffer Triangles {
    Triangle data[];
} triangles;

layout(location = 0) in vec3 vertex_interp;
layout(location = 1) in vec3 normal_interp;
layout(location = 2) in vec2 uv_interp;
layout(location = 3) in vec3 barycentric;
layout(location = 4) in flat uvec3 vertex_indices;
layout(location = 5) in flat vec3 face_normal;

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_unocclude;

void main()
{
    vec3 vertex_pos = vertex_interp;

    {
        vec3 pos_a = vertices.data[vertex_indices.x].position.xyz;
        vec3 pos_b = vertices.data[vertex_indices.y].position.xyz;
        vec3 pos_c = vertices.data[vertex_indices.z].position.xyz;
        vec3 center = (pos_a + pos_b + pos_c) * 0.3333333;
        vec3 norm_a = vertices.data[vertex_indices.x].normal.xyz;
        vec3 norm_b = vertices.data[vertex_indices.y].normal.xyz;
        vec3 norm_c = vertices.data[vertex_indices.z].normal.xyz;

        {
            vec3 dir_a = normalize(pos_a - center);
            float d_a = dot(dir_a, norm_a);
            if (d_a < 0) {
                norm_a = normalize(norm_a - dir_a * d_a);
            }
        }
        {
            vec3 dir_b = normalize(pos_b - center);
            float d_b = dot(dir_b, norm_b);
            if (d_b < 0) {
                norm_b = normalize(norm_b - dir_b * d_b);
            }
        }
        {
            vec3 dir_c = normalize(pos_c - center);
            float d_c = dot(dir_c, norm_c);
            if (d_c < 0) {
                norm_c = normalize(norm_c - dir_c * d_c);
            }
        }

        float d_a = dot(norm_a, pos_a);
        float d_b = dot(norm_b, pos_b);
        float d_c = dot(norm_c, pos_c);

        vec3 proj_a = vertex_pos - norm_a * (dot(norm_a, vertex_pos) - d_a);
        vec3 proj_b = vertex_pos - norm_b * (dot(norm_b, vertex_pos) - d_b);
        vec3 proj_c = vertex_pos - norm_c * (dot(norm_c, vertex_pos) - d_c);

        vec3 smooth_position = proj_a * barycentric.x + proj_b * barycentric.y + proj_c * barycentric.z;

        if (dot(face_normal, smooth_position) > dot(face_normal, vertex_pos)) {
            vertex_pos = smooth_position;
        }
    }

    // https://ndotl.wordpress.com/2018/08/29/baking-artifact-free-lightmaps/
    vec3 delta_uv = max(abs(dFdx(vertex_interp)), abs(dFdy(vertex_interp)));
    float texel_size = max(delta_uv.x, max(delta_uv.y, delta_uv.z));
    texel_size *= sqrt(2.0);
    out_unocclude.xyz = face_normal;
    out_unocclude.w = texel_size;

    out_position = vec4(vertex_pos, 1.0);
    out_normal = vec4(normalize(normal_interp), 1.0);
}