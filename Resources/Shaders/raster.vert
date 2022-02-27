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

layout(location = 0) out vec3 vertex_interp;
layout(location = 1) out vec3 normal_interp;
layout(location = 2) out vec2 uv_interp;
layout(location = 3) out vec3 barycentric;
layout(location = 4) flat out uvec3 vertex_indices;
layout(location = 5) flat out vec3 face_normal;

layout(push_constant) uniform RasterParams {
    vec2 atlas_size;
    vec2 uv_offset;
} params;

void main() 
{
    uint triangle_idx = gl_VertexIndex / 3;
    uint triangle_subidx = gl_VertexIndex % 3;
    vertex_indices = triangles.data[triangle_idx].indices.xyz;
    uint vertex_idx;
    if (triangle_subidx == 0) {
        vertex_idx = vertex_indices.x;
        barycentric = vec3(1, 0, 0);
    } else if (triangle_subidx == 1) {
        vertex_idx = vertex_indices.y;
        barycentric = vec3(0, 1, 0);
    } else {
        vertex_idx = vertex_indices.z;
        barycentric = vec3(0, 0, 1);
    }

    vertex_interp = vertices.data[vertex_idx].position.xyz;
    uv_interp = vertices.data[vertex_idx].uv1;
    normal_interp = vertices.data[vertex_idx].normal.xyz;
    face_normal = normalize(cross((vertices.data[vertex_indices.x].position.xyz - vertices.data[vertex_indices.y].position.xyz), (vertices.data[vertex_indices.x].position.xyz - vertices.data[vertex_indices.z].position.xyz)));

    vec2 texel_size = vec2(1.0 / params.atlas_size.x, 1.0 / params.atlas_size.y);
    vec2 half_texel_size = texel_size * 1.5;
    vec2 uv_offset = vec2(params.uv_offset.x * half_texel_size.x, params.uv_offset.y * half_texel_size.y);

    gl_Position = vec4((vertices.data[vertex_idx].uv1 + uv_offset) * 2.0 - 1.0, 0.0001, 1.0);
}