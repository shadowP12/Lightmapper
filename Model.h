#pragma once

#include <Blast/Gfx/GfxDefine.h>
#include <Blast/Gfx/GfxDevice.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <vector>

class Model {
public:
    Model();

    ~Model();

    void GenerateGPUResource(blast::GfxDevice* device);

    void ReleaseGPUResource(blast::GfxDevice* device);

    void SetModelMatriax(const glm::mat4& m) { model_matriax = m; }

    glm::mat4 GetModelMatriax() { return model_matriax; }

    void SetVertexCount(uint32_t vertex_count) { this->vertex_count = vertex_count; }

    uint32_t GetVertexCount() { return vertex_count; }

    void SetIndexCount(uint32_t index_count) { this->index_count = index_count; }

    uint32_t GetIndexCount() { return index_count; }

    void SetPositionData(uint8_t* position_data) { this->position_data = position_data; }

    uint8_t* GetPositionData() { return position_data; }

    void SetNormalData(uint8_t* normal_data) { this->normal_data = normal_data; }

    uint8_t* GetNormalData() { return normal_data; }

    void SetUV0Data(uint8_t* uv0_data) { this->uv0_data = uv0_data; }

    uint8_t* GetUV0Data() { return uv0_data; }

    void SetUV1Data(uint8_t* uv1_data) { this->uv1_data = uv1_data; }

    uint8_t* GetUV1Data() { return uv1_data; }

    void ResetUV1Data(uint8_t* uv1_data);

    void SetIndexData(uint8_t* index_data) { this->index_data = index_data; }

    uint8_t* GetIndexData() { return index_data; }

    void SetIndexType(blast::IndexType index_type) { this->index_type = index_type; }

    blast::IndexType GetIndexType() { return index_type; }

    blast::GfxInputLayout* GetInputLayout() { return input_layout; }

    blast::GfxBuffer* GetVertexBuffer() { return vertex_buffer; }

    blast::GfxBuffer* GetIndexBuffer() { return index_buffer; }

private:
    glm::mat4 model_matriax;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    uint8_t* position_data = nullptr;
    uint8_t* normal_data = nullptr;
    uint8_t* uv0_data = nullptr;
    uint8_t* uv1_data = nullptr;
    uint8_t* index_data = nullptr;
    blast::IndexType index_type;
    blast::GfxInputLayout* input_layout = nullptr;
    blast::GfxBuffer* vertex_buffer = nullptr;
    blast::GfxBuffer* index_buffer = nullptr;
};