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

void PlotTriangleIntoTriangleIndexList(int grid_size, const glm::ivec3& grid_offset, const AABB& bounds, const glm::vec3 points[3], uint32_t triangle_index, std::vector<TriangleSort>& triangles) {
    int half_size = grid_size / 2;

    for (int i = 0; i < 8; i++) {
        AABB aabb = bounds;
        aabb.size *= 0.5;
        glm::ivec3 n = grid_offset;

        if (i & 1) {
            aabb.position.x += aabb.size.x;
            n.x += half_size;
        }
        if (i & 2) {
            aabb.position.y += aabb.size.y;
            n.y += half_size;
        }
        if (i & 4) {
            aabb.position.z += aabb.size.z;
            n.z += half_size;
        }

        {
            glm::vec3 qsize = aabb.size * 0.5f;
            if (!TriangleBoxOverlap(aabb.position + qsize, qsize, points)) {
                continue;
            }
        }

        if (half_size == 1) {
            TriangleSort ts;
            ts.cell_index = n.x + (n.y * MAX_GRID_SIZE) + (n.z * MAX_GRID_SIZE * MAX_GRID_SIZE);
            ts.triangle_index = triangle_index;
            triangles.push_back(ts);
        } else {
            PlotTriangleIntoTriangleIndexList(half_size, n, aabb, points, triangle_index, triangles);
        }
    }
}

AccelerationStructures* BuildAccelerationStructures(std::vector<Model*>& models) {
    AccelerationStructures* as = new AccelerationStructures();

    for (int i = 0; i < models.size(); i++) {
        std::unordered_map<Edge, EdgeUV, MurmurHash<Edge>, EdgeEq> edges;

        glm::vec3* position_data = (glm::vec3*)models[i]->GetPositionData();
        glm::vec3* normal_data = (glm::vec3*)models[i]->GetNormalData();
        glm::vec2* uv0_data = (glm::vec2*)models[i]->GetUV0Data();
        glm::vec2* uv1_data = (glm::vec2*)models[i]->GetUV1Data();
        uint16_t* index16_data = (uint16_t*)models[i]->GetIndexData();
        uint32_t* index32_data = (uint32_t*)models[i]->GetIndexData();

        if (i == 0) {
            as->bounds.position = ((glm::vec3*)models[i]->GetPositionData())[0];
        }

        for (int j = 0; j < models[i]->GetIndexCount(); j+=3) {
            uint32_t indices[3];
            if (models[i]->GetIndexType() == blast::INDEX_TYPE_UINT16) {
                indices[0] = index16_data[j];
                indices[1] = index16_data[j+1];
                indices[2] = index16_data[j+2];
            } else {
                indices[0] = index32_data[j];
                indices[1] = index32_data[j+1];
                indices[2] = index32_data[j+2];
            }

            glm::vec3 vtxs[3] = { position_data[indices[0]], position_data[indices[1]], position_data[indices[2]] };
            glm::vec3 normals[3] = { normal_data[indices[0]], position_data[indices[1]], position_data[indices[2]] };
            glm::vec2 uvs[3] = { uv0_data[indices[0]], uv0_data[indices[1]], uv0_data[indices[2]] };
            glm::vec2 atlas_uvs[3] = { uv1_data[indices[0]], uv1_data[indices[1]], uv1_data[indices[2]] };

            AABB taabb;
            Triangle t;
            for (int k = 0; k < 3; k++) {
                as->bounds.Expand(vtxs[k]);

                Vertex v;
                v.position[0] = vtxs[k].x;
                v.position[1] = vtxs[k].y;
                v.position[2] = vtxs[k].z;
                v.uv0[0] = uvs[k].x;
                v.uv1[1] = atlas_uvs[k].y;
                v.normal[0] = normals[k].x;
                v.normal[1] = normals[k].y;
                v.normal[2] = normals[k].z;
                as->vertices.push_back(v);

                t.indices[k] = indices[k];
                if (k == 0) {
                    taabb.position = vtxs[k];
                } else {
                    taabb.Expand(vtxs[k]);
                }
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
                    seam.slice = 0;
                    as->seams.push_back(seam);
                    euv2->seam_found = true;
                }
            }

            t.min_bounds[0] = taabb.position.x;
            t.min_bounds[1] = taabb.position.y;
            t.min_bounds[2] = taabb.position.z;
            t.max_bounds[0] = taabb.position.x + glm::max(taabb.size.x, 0.0001f);
            t.max_bounds[1] = taabb.position.y + glm::max(taabb.size.y, 0.0001f);
            t.max_bounds[2] = taabb.position.z + glm::max(taabb.size.z, 0.0001f);
            // 不支持多lod烘培
            t.slice = 0;
            t.pad0 = t.pad1 = 0;
            as->triangles.push_back(t);
        }
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

        PlotTriangleIntoTriangleIndexList(128, glm::ivec3(0), as->bounds, face, i, triangle_sort);
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