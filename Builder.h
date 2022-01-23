#pragma once

#include "Model.h"
#include "LightMapperDefine.h"

struct AccelerationStructures {
    AABB bounds;
    std::vector<Vertex>  vertices;
    std::vector<Triangle>  triangles;
    std::vector<Seam>  seams;
};

AccelerationStructures* BuildAccelerationStructures(std::vector<Model>& models);