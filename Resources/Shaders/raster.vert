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

void main() 
{
    uint triangle_idx = gl_VertexIndex / 3;
    uint triangle_subidx = gl_VertexIndex % 3;
    uvec4 vertex_indices = triangles.data[triangle_idx].indices;
    uint vertex_idx;
    if (triangle_subidx == 0) {
        vertex_idx = vertex_indices.x;
    } else if (triangle_subidx == 1) {
        vertex_idx = vertex_indices.y;
    } else {
        vertex_idx = vertex_indices.z;
    }

    gl_Position = vec4((vertices.data[vertex_idx].uv1) * 2.0 - 1.0, 0.0001, 1.0);
}