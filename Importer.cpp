#include "Importer.h"
#include "Model.h"
#include "LightMapperDefine.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <Blast/Gfx/GfxDefine.h>

glm::mat4 GetLocalMatrix(cgltf_node* node) {
    glm::vec3 translation = glm::vec3(0.0f);
    if (node->has_translation) {
        translation.x = node->translation[0];
        translation.y = node->translation[1];
        translation.z = node->translation[2];
    }

    glm::quat rotation = glm::quat(1, 0, 0, 0);
    if (node->has_rotation) {
        rotation.x = node->rotation[0];
        rotation.y = node->rotation[1];
        rotation.z = node->rotation[2];
        rotation.w = node->rotation[3];
    }

    glm::vec3 scale = glm::vec3(1.0f);
    if (node->has_scale) {
        scale.x = node->scale[0];
        scale.y = node->scale[1];
        scale.z = node->scale[2];
    }

    glm::mat4 r, t, s;
    r = glm::toMat4(rotation);
    t = glm::translate(glm::mat4(1.0), translation);
    s = glm::scale(glm::mat4(1.0), scale);
    return t * r * s;
}

glm::mat4 GetWorldMatrix(cgltf_node* node) {
    cgltf_node* curNode = node;
    glm::mat4 out = GetLocalMatrix(curNode);

    while (curNode->parent != nullptr) {
        curNode = node->parent;
        out = GetLocalMatrix(curNode) * out;
    }
    return out;
}

cgltf_attribute* GetGltfAttribute(cgltf_primitive* primitive, cgltf_attribute_type type) {
    for (int i = 0; i < primitive->attributes_count; i++) {
        cgltf_attribute* att = &primitive->attributes[i];
        if(att->type == type) {
            return att;
        }
    }
    return nullptr;
}

std::vector<Model*> ImportScene(const std::string& file_path) {
    cgltf_options options = {static_cast<cgltf_file_type>(0)};
    cgltf_data* data = NULL;
    cgltf_result ret = cgltf_parse_file(&options, file_path.c_str(), &data);
    if (ret == cgltf_result_success) {
        ret = cgltf_load_buffers(&options, data, file_path.c_str());
    }

    if (ret == cgltf_result_success) {
        ret = cgltf_validate(data);
    }

    std::vector<Model*> models;

    for (size_t i = 0; i < data->nodes_count; ++i) {
        cgltf_node* cnode = &data->nodes[i];
        cgltf_mesh* cmesh = cnode->mesh;

        if(!cmesh) {
            continue;
        }

        cgltf_primitive* cprimitive = &cmesh->primitives[0];
        cgltf_material* cmaterial = cprimitive->material;

        uint32_t vertex_count = GetGltfAttribute(cprimitive, cgltf_attribute_type_position)->data->count;

        // 获取顶点数据
        uint8_t* positionData = nullptr;
        uint8_t* uvData = nullptr;
        uint8_t* normalData = nullptr;

        cgltf_attribute* positionAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_position);
        cgltf_accessor* posAccessor = positionAttribute->data;
        cgltf_buffer_view* posView = posAccessor->buffer_view;
        positionData = (uint8_t*)(posView->buffer->data) + posAccessor->offset + posView->offset;

        if (GetGltfAttribute(cprimitive, cgltf_attribute_type_texcoord)) {
            cgltf_attribute* texcoordAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_texcoord);
            cgltf_accessor* texcoordAccessor = texcoordAttribute->data;
            cgltf_buffer_view* texcoordView = texcoordAccessor->buffer_view;
            uvData = (uint8_t *) (texcoordView->buffer->data) + texcoordAccessor->offset + texcoordView->offset;
        }

        if (GetGltfAttribute(cprimitive, cgltf_attribute_type_normal)) {
            cgltf_attribute* normalAttribute = GetGltfAttribute(cprimitive, cgltf_attribute_type_normal);
            cgltf_accessor* normalAccessor = normalAttribute->data;
            cgltf_buffer_view* normalView = normalAccessor->buffer_view;
            normalData = (uint8_t*)(normalView->buffer->data) + normalAccessor->offset + normalView->offset;
        }

        // 填充空缺数据
        bool uv_data_empty = false;
        bool normal_data_empty = false;
        if (!uvData || normalData) {
            float* uvs = nullptr;
            if (!uvData) {
                uv_data_empty = true;
                uvData = new uint8_t[vertex_count * sizeof(glm::vec2)];
                uvs = (float*)normalData;
            }

            float* normals = nullptr;
            if (!normalData) {
                normal_data_empty = true;
                normalData = new uint8_t[vertex_count * sizeof(glm::vec3)];
                normals = (float*)normalData;
            }

            for (int j = 0; j < vertex_count; ++j) {
                if (!uvData) {
                    uvs[j * 2] = 0.0f;
                    uvs[j * 2 + 1] = 0.0f;
                }

                if (!normalData) {
                    normals[j * 3] = 1.0f;
                    normals[j * 3 + 1] = 0.0f;
                    normals[j * 3 + 2] = 0.0f;
                }
            }
        }

        // Indices
        cgltf_accessor* cIndexAccessor = cprimitive->indices;
        cgltf_buffer_view* cIndexBufferView = cIndexAccessor->buffer_view;
        cgltf_buffer* cIndexBuffer = cIndexBufferView->buffer;

        uint32_t indexCount = cIndexAccessor->count;
        blast::IndexType indexType;
        if (cIndexAccessor->component_type == cgltf_component_type_r_16u) {
            indexType = blast::INDEX_TYPE_UINT16;
        } else if (cIndexAccessor->component_type == cgltf_component_type_r_32u) {
            indexType = blast::INDEX_TYPE_UINT32;
        }
        uint8_t* indexData = (uint8_t*)cIndexBuffer->data + cIndexAccessor->offset + cIndexBufferView->offset;

        Model* model = new Model();
        model->SetModelMatriax(GetWorldMatrix(cnode));
        model->SetVertexCount(vertex_count);
        model->SetIndexCount(indexCount);
        model->SetIndexType(indexType);

        uint8_t* position_data = new uint8_t[vertex_count * sizeof(glm::vec3)];
        memcpy(position_data, positionData, vertex_count * sizeof(glm::vec3));
        uint8_t* normal_data = new uint8_t[vertex_count * sizeof(glm::vec3)];
        memcpy(normal_data, normalData, vertex_count * sizeof(glm::vec3));
        uint8_t* uv0_data = new uint8_t[vertex_count * sizeof(glm::vec2)];
        memcpy(uv0_data, uvData, vertex_count * sizeof(glm::vec2));
        uint8_t* uv1_data = new uint8_t[vertex_count * sizeof(glm::vec2)];
        memcpy(uv0_data, uvData, vertex_count * sizeof(glm::vec2));

        uint8_t* index_data = nullptr;
        if (indexType == blast::INDEX_TYPE_UINT16) {
            index_data = new uint8_t[indexCount * sizeof(uint16_t)];
            memcpy(index_data, indexData, indexCount * sizeof(uint16_t));
        } else {
            index_data = new uint8_t[indexCount * sizeof(uint32_t)];
            memcpy(index_data, indexData, indexCount * sizeof(uint32_t));
        }

        // 模型矩阵作用于顶点数据
        glm::mat4 model_matrix = GetWorldMatrix(cnode);
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model_matrix)));
        float* positions = (float*)position_data;
        float* normals = (float*)normal_data;
        for (uint32_t j = 0; j < vertex_count; ++j) {
            glm::vec4 pos = glm::vec4(positions[j*3], positions[j*3+1], positions[j*3+2], 1.0);
            pos = model_matrix * pos;
            pos /= pos.w;
            positions[j*3] = pos.x;
            positions[j*3+1] = pos.y;
            positions[j*3+2] = pos.z;

            glm::vec3 normal = normal_matrix * glm::vec3(normals[j*3], normals[j*3+1], normals[j*3+2]);
            normals[j*3] = normal.x;
            normals[j*3+1] = normal.y;
            normals[j*3+2] = normal.z;
        }

        model->SetPositionData(position_data);
        model->SetNormalData(normal_data);
        model->SetUV0Data(uv0_data);
        model->SetUV1Data(uv1_data);
        model->SetIndexData(index_data);

        models.push_back(model);

        // 释放临时内存
        if (uv_data_empty) {
            SAFE_DELETE(uvData);
        }
        if (normal_data_empty) {
            SAFE_DELETE(normalData);
        }
    }

    cgltf_free(data);
    return models;
}