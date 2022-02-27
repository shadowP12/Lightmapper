#include "Builder.h"

#include <Blast/Gfx/GfxDefine.h>

#include <algorithm>
#include <unordered_map>
#include <functional>

// 使用SAT算法
/*======================== X-tests ========================*/
#define AXISTEST_X01(a, b, fa, fb)                 \
	p0 = a * v0.y - b * v0.z;                      \
	p2 = a * v2.y - b * v2.z;                      \
	if (p0 < p2) {                                 \
		min = p0;                                  \
		max = p2;                                  \
	} else {                                       \
		min = p2;                                  \
		max = p0;                                  \
	}                                              \
	rad = fa * boxhalfsize.y + fb * boxhalfsize.z; \
	if (min > rad || max < -rad) {                 \
		return false;                              \
	}

#define AXISTEST_X2(a, b, fa, fb)                  \
	p0 = a * v0.y - b * v0.z;                      \
	p1 = a * v1.y - b * v1.z;                      \
	if (p0 < p1) {                                 \
		min = p0;                                  \
		max = p1;                                  \
	} else {                                       \
		min = p1;                                  \
		max = p0;                                  \
	}                                              \
	rad = fa * boxhalfsize.y + fb * boxhalfsize.z; \
	if (min > rad || max < -rad) {                 \
		return false;                              \
	}

/*======================== Y-tests ========================*/
#define AXISTEST_Y02(a, b, fa, fb)                 \
	p0 = -a * v0.x + b * v0.z;                     \
	p2 = -a * v2.x + b * v2.z;                     \
	if (p0 < p2) {                                 \
		min = p0;                                  \
		max = p2;                                  \
	} else {                                       \
		min = p2;                                  \
		max = p0;                                  \
	}                                              \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.z; \
	if (min > rad || max < -rad) {                 \
		return false;                              \
	}

#define AXISTEST_Y1(a, b, fa, fb)                  \
	p0 = -a * v0.x + b * v0.z;                     \
	p1 = -a * v1.x + b * v1.z;                     \
	if (p0 < p1) {                                 \
		min = p0;                                  \
		max = p1;                                  \
	} else {                                       \
		min = p1;                                  \
		max = p0;                                  \
	}                                              \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.z; \
	if (min > rad || max < -rad) {                 \
		return false;                              \
	}

/*======================== Z-tests ========================*/

#define AXISTEST_Z12(a, b, fa, fb)                 \
	p1 = a * v1.x - b * v1.y;                      \
	p2 = a * v2.x - b * v2.y;                      \
	if (p2 < p1) {                                 \
		min = p2;                                  \
		max = p1;                                  \
	} else {                                       \
		min = p1;                                  \
		max = p2;                                  \
	}                                              \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.y; \
	if (min > rad || max < -rad) {                 \
		return false;                              \
	}

#define AXISTEST_Z0(a, b, fa, fb)                  \
	p0 = a * v0.x - b * v0.y;                      \
	p1 = a * v1.x - b * v1.y;                      \
	if (p0 < p1) {                                 \
		min = p0;                                  \
		max = p1;                                  \
	} else {                                       \
		min = p1;                                  \
		max = p0;                                  \
	}                                              \
	rad = fa * boxhalfsize.x + fb * boxhalfsize.y; \
	if (min > rad || max < -rad) {                 \
		return false;                              \
	}

#define FINDMINMAX(x0, x1, x2, min, max) \
	min = max = x0;                      \
	if (x1 < min) {                      \
		min = x1;                        \
	}                                    \
	if (x1 > max) {                      \
		max = x1;                        \
	}                                    \
	if (x2 < min) {                      \
		min = x2;                        \
	}                                    \
	if (x2 > max) {                      \
		max = x2;                        \
	}

static bool TriangleBoxOverlap(const glm::vec3 &boxcenter, const glm::vec3 boxhalfsize, const glm::vec3 *triverts) {
    glm::vec3 v0, v1, v2;
    float min, max, d, p0, p1, p2, rad, fex, fey, fez;
    glm::vec3 normal, e0, e1, e2;

    // 以box中心作为坐标系原点
    v0 = triverts[0] - boxcenter;
    v1 = triverts[1] - boxcenter;
    v2 = triverts[2] - boxcenter;

    e0 = v1 - v0;
    e1 = v2 - v1;
    e2 = v0 - v2;

    fex = glm::abs(e0.x);
    fey = glm::abs(e0.y);
    fez = glm::abs(e0.z);
    AXISTEST_X01(e0.z, e0.y, fez, fey);
    AXISTEST_Y02(e0.z, e0.x, fez, fex);
    AXISTEST_Z12(e0.y, e0.x, fey, fex);

    fex = glm::abs(e1.x);
    fey = glm::abs(e1.y);
    fez = glm::abs(e1.z);
    AXISTEST_X01(e1.z, e1.y, fez, fey);
    AXISTEST_Y02(e1.z, e1.x, fez, fex);
    AXISTEST_Z0(e1.y, e1.x, fey, fex);

    fex = glm::abs(e2.x);
    fey = glm::abs(e2.y);
    fez = glm::abs(e2.z);
    AXISTEST_X2(e2.z, e2.y, fez, fey);
    AXISTEST_Y1(e2.z, e2.x, fez, fex);
    AXISTEST_Z12(e2.y, e2.x, fey, fex);

    FINDMINMAX(v0.x, v1.x, v2.x, min, max);
    if (min > boxhalfsize.x || max < -boxhalfsize.x) {
        return false;
    }

    FINDMINMAX(v0.y, v1.y, v2.y, min, max);
    if (min > boxhalfsize.y || max < -boxhalfsize.y) {
        return false;
    }

    FINDMINMAX(v0.z, v1.z, v2.z, min, max);
    if (min > boxhalfsize.z || max < -boxhalfsize.z) {
        return false;
    }

    auto PlaneBoxOverlap = [&](glm::vec3 normal, float d, glm::vec3 maxbox)->bool {
        glm::vec3 vmin, vmax;
        for (uint32_t q = 0; q <= 2; q++) {
            if (normal[q] > 0.0f) {
                vmin[q] = -maxbox[q];
                vmax[q] = maxbox[q];
            } else {
                vmin[q] = maxbox[q];
                vmax[q] = -maxbox[q];
            }
        }
        if (glm::dot(normal, vmin) + d > 0.0f) {
            return false;
        }

        if (glm::dot(normal, vmax) + d >= 0.0f) {
            return true;
        }

        return false;
    };

    normal = glm::cross(e0, e1);
    d = -glm::dot(normal, v0);

    return PlaneBoxOverlap(normal, d, boxhalfsize);
}

static bool first_debug = true;

void PlotTriangleIntoTriangleIndexList(int grid_size, const glm::ivec3& grid_offset, const AABB& bounds, const glm::vec3 points[3], uint32_t triangle_index, std::vector<TriangleSort>& triangles) {
    if (first_debug) {
        glm::vec3 bbox_size = bounds.GetSize();
        //printf("bbox pos:  %f   %f   %f\n", bounds.min.x, bounds.min.y, bounds.min.z);
        //printf("bbox size:  %f   %f   %f\n", bbox_size.x, bbox_size.y, bbox_size.z);
    }

    int half_size = grid_size / 2;

    for (int i = 0; i < 8; i++) {
        glm::vec3 aabb_position = bounds.min;
        glm::vec3 aabb_size = bounds.GetSize() * 0.5f;
        glm::ivec3 n = grid_offset;

        if (i & 1) {
            aabb_position.x += aabb_size.x;
            n.x += half_size;
        }
        if (i & 2) {
            aabb_position.y += aabb_size.y;
            n.y += half_size;
        }
        if (i & 4) {
            aabb_position.z += aabb_size.z;
            n.z += half_size;
        }

        {
            glm::vec3 qsize = aabb_size * 0.5f;
            if (!TriangleBoxOverlap(aabb_position + qsize, qsize, points)) {
                continue;
            }
        }

        if (half_size == 1) {
            if (first_debug) {
                //printf("final %d   %d    %d\n", n.x, n.y, n.z);
            }
            TriangleSort ts;
            ts.cell_index = n.x + (n.y * MAX_GRID_SIZE) + (n.z * MAX_GRID_SIZE * MAX_GRID_SIZE);
            ts.triangle_index = triangle_index;
            triangles.push_back(ts);
        } else {
            if (first_debug) {
                //printf("sub %d   %d    %d\n", n.x, n.y, n.z);
            }
            AABB aabb;
            aabb.min = aabb_position;
            aabb.max = aabb_position + aabb_size;
            PlotTriangleIntoTriangleIndexList(half_size, n, aabb, points, triangle_index, triangles);
        }
    }
}

AccelerationStructures* BuildAccelerationStructures(std::vector<Model*>& models) {
    AccelerationStructures* as = new AccelerationStructures();

    for (int i = 0; i < models.size(); i++) {
        glm::vec3* position_data = (glm::vec3*)models[i]->GetPositionData();
        glm::vec3* normal_data = (glm::vec3*)models[i]->GetNormalData();
        glm::vec2* uv0_data = (glm::vec2*)models[i]->GetUV0Data();
        glm::vec2* uv1_data = (glm::vec2*)models[i]->GetUV1Data();

        for (int j = 0; j < models[i]->GetVertexCount(); ++j) {
            as->bounds.Expand(position_data[j]);

            Vertex v;
            v.position.x = position_data[j].x;
            v.position.y = position_data[j].y;
            v.position.z = position_data[j].z;
            v.normal.x = normal_data[j].x;
            v.normal.y = normal_data[j].y;
            v.normal.z = normal_data[j].z;
            v.uv0.x = uv0_data[j].x;
            v.uv0.y = uv0_data[j].y;
            v.uv1.x = uv1_data[j].x;
            v.uv1.y = uv1_data[j].y;
            as->vertices.push_back(v);
        }
    }

    int vertex_offset = 0;
    for (int i = 0; i < models.size(); i++) {
        std::unordered_map<Edge, EdgeUV, MurmurHash<Edge>, EdgeEq> edges;
        uint16_t* index16_data = (uint16_t*)models[i]->GetIndexData();
        uint32_t* index32_data = (uint32_t*)models[i]->GetIndexData();

        for (int j = 0; j < models[i]->GetIndexCount(); j+=3) {
            uint32_t indices[3];
            if (models[i]->GetIndexType() == blast::INDEX_TYPE_UINT16) {
                indices[0] = index16_data[j] + vertex_offset;
                indices[1] = index16_data[j+1] + vertex_offset;
                indices[2] = index16_data[j+2] + vertex_offset;
            } else {
                indices[0] = index32_data[j] + vertex_offset;
                indices[1] = index32_data[j+1] + vertex_offset;
                indices[2] = index32_data[j+2] + vertex_offset;
            }

            glm::vec3 vtxs[3] = { as->vertices[indices[0]].position, as->vertices[indices[1]].position, as->vertices[indices[2]].position };
            glm::vec3 normals[3] = { as->vertices[indices[0]].normal, as->vertices[indices[1]].normal, as->vertices[indices[2]].normal };
            glm::vec2 uvs[3] = { as->vertices[indices[0]].uv0, as->vertices[indices[1]].uv0, as->vertices[indices[2]].uv0 };
            glm::vec2 atlas_uvs[3] = { as->vertices[indices[0]].uv1, as->vertices[indices[1]].uv1, as->vertices[indices[2]].uv1 };

            AABB taabb;
            Triangle t;
            for (int k = 0; k < 3; k++) {
                t.indices[k] = indices[k];
                taabb.Expand(vtxs[k]);
            }

            // 计算seam
            for (int k = 0; k < 3; k++) {
                int n = (k + 1) % 3;

                Edge edge(vtxs[k], vtxs[n], normals[k], normals[n]);
                glm::ivec2 edge_indices(t.indices[k], t.indices[n]);
                EdgeUV uv2(atlas_uvs[k], atlas_uvs[n], edge_indices);

                if (edge.b == edge.a) {
                    continue;
                }

                EdgeUV* euv2 = nullptr;
                auto iter = edges.find(edge);
                if (iter != edges.end()) {
                    euv2 = &iter->second;
                }

                if (!euv2) {
                    edges[edge] = uv2;
                } else {
                    if (*euv2 == uv2) {
                        continue;
                    }
                    if (euv2->seam_found) {
                        continue;
                    }

                    Seam seam;
                    seam.a = edge_indices;
                    seam.b = euv2->indices;
                    as->seams.push_back(seam);
                    euv2->seam_found = true;
                }
            }

            t.min_bounds[0] = taabb.min.x;
            t.min_bounds[1] = taabb.min.y;
            t.min_bounds[2] = taabb.min.z;
            t.max_bounds[0] = taabb.max.x;
            t.max_bounds[1] = taabb.max.y;
            t.max_bounds[2] = taabb.max.z;
            as->triangles.push_back(t);
        }

        vertex_offset += models[i]->GetVertexCount();
    }

    // 为了避免数值错误稍微扩充下包围盒
    as->bounds.Grow(0.1f);

    // 构建八叉树
    // 为了加速查询将节点使用数组的形式保存
    std::vector<TriangleSort> triangle_sort;
    for (uint32_t i = 0; i < as->triangles.size(); i++) {
        const Triangle& t = as->triangles[i];
        glm::vec3 face[3] = {
                as->vertices[t.indices[0]].position,
                as->vertices[t.indices[1]].position,
                as->vertices[t.indices[2]].position,
        };

        if (first_debug) {
            printf("v0:  %f   %f   %f\n", face[0].x, face[0].y, face[0].z);
            printf("v1:  %f   %f   %f\n", face[1].x, face[1].y, face[1].z);
            printf("v2:  %f   %f   %f\n", face[2].x, face[2].y, face[2].z);
        }

        PlotTriangleIntoTriangleIndexList(MAX_GRID_SIZE, glm::ivec3(0), as->bounds, face, i, triangle_sort);

        if (first_debug) {
            first_debug = false;
        }
    }

    // 排序
    std::sort(triangle_sort.begin(), triangle_sort.end());

    as->triangle_indices.resize(triangle_sort.size());
    as->grid_indices.resize(MAX_GRID_SIZE * MAX_GRID_SIZE * MAX_GRID_SIZE * 2);
    memset(as->grid_indices.data(), 0, as->grid_indices.size() * sizeof(uint32_t));

    uint32_t* triangle_indices_data = as->triangle_indices.data();
    uint32_t* grid_indices_data = as->grid_indices.data();
    uint32_t last_cell = 0xFFFFFFFF;
    for (uint32_t i = 0; i < triangle_sort.size(); i++) {
        uint32_t cell = triangle_sort[i].cell_index;
        if (cell != last_cell) {
            // 第二位记录同个cell第一个的三角形索引
            grid_indices_data[cell*2+1] = i;
        }
        triangle_indices_data[i] = triangle_sort[i].triangle_index;
        // 第一位记录同个cell内的三角形数量
        grid_indices_data[cell*2]++;
        last_cell = cell;
    }

    return as;
}