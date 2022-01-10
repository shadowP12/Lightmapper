#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/matrix_transform.hpp>

#define SAFE_DELETE(x) \
    { \
        delete x; \
        x = nullptr; \
    }

#define SAFE_DELETE_ARRAY(x) \
    { \
        delete[] x; \
        x = nullptr; \
    }

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv0;
    glm::vec2 uv1;
};