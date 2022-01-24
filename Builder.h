#pragma once

#include "Model.h"
#include "LightMapperDefine.h"

struct AccelerationStructures {
    AABB bounds;
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<Seam> seams;
    std::vector<uint32_t> triangle_indices;
    std::vector<uint32_t> grid_indices;
};

AccelerationStructures* BuildAccelerationStructures(std::vector<Model*>& models);