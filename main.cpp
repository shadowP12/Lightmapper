#include "LightMapperDefine.h"
#include "Importer.h"
#include "Model.h"
#include "Builder.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Blast/Gfx/GfxDevice.h>
#include <Blast/Gfx/Vulkan/VulkanDevice.h>
#include <Blast/Utility/ShaderCompiler.h>
#include <Blast/Utility/VulkanShaderCompiler.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <xatlas.h>

#include <iostream>
#include <fstream>
#include <sstream>

static std::string ProjectDir(PROJECT_DIR);

static std::string ReadFileData(const std::string& path) {
    std::istream* stream = &std::cin;
    std::ifstream file;

    file.open(path, std::ios_base::binary);
    stream = &file;
    if (file.fail()) {
        BLAST_LOGW("cannot open input file %s \n", path.c_str());
        return std::string("");
    }
    return std::string((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
}

static std::pair<blast::GfxShader*, blast::GfxShader*> CompileShaderProgram(const std::string& vs_path, const std::string& fs_path);

static blast::GfxShader* CompileComputeShader(const std::string& cs_path);

static void RefreshSwapchain(void* window, uint32_t width, uint32_t height);

static void CursorPositionCallback(GLFWwindow* window, double pos_x, double pos_y);

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

static void MouseScrollCallback(GLFWwindow* window, double offset_x, double offset_y);

blast::ShaderCompiler* g_shader_compiler = nullptr;
blast::GfxDevice* g_device = nullptr;
blast::GfxSwapChain* g_swapchain = nullptr;
blast::GfxTexture* position_tex = nullptr;
blast::GfxTexture* normal_tex = nullptr;
blast::GfxTexture* unocclude_tex = nullptr;
blast::GfxRenderPass* raster_renderpass = nullptr;
blast::GfxTexture* scene_color_tex = nullptr;
blast::GfxTexture* scene_depth_tex = nullptr;
blast::GfxRenderPass* scene_renderpass = nullptr;
blast::GfxShader* blit_vert_shader = nullptr;
blast::GfxShader* blit_frag_shader = nullptr;
blast::GfxPipeline* blit_pipeline = nullptr;
blast::GfxShader* scene_vert_shader = nullptr;
blast::GfxShader* scene_frag_shader = nullptr;
blast::GfxPipeline* scene_pipeline = nullptr;
blast::GfxShader* raster_vert_shader = nullptr;
blast::GfxShader* raster_frag_shader = nullptr;
blast::GfxPipeline* raster_line_pipeline = nullptr;
blast::GfxPipeline* raster_triangle_pipeline = nullptr;
blast::GfxShader* direct_light_shader = nullptr;
blast::GfxBuffer* object_ub = nullptr;

// Acceleration Structures Begin
blast::GfxBuffer* vertex_buffer = nullptr;
blast::GfxBuffer* triangle_buffer = nullptr;
blast::GfxBuffer* seam_buffer = nullptr;
blast::GfxBuffer* triangle_index_buffer = nullptr;
blast::GfxTexture* grid_tex = nullptr;
// Acceleration Structures End

// LightMap Begin
blast::GfxSampler* linear_sampler = nullptr;
blast::GfxSampler* nearest_sampler = nullptr;
blast::GfxBuffer* light_buffer = nullptr;
blast::GfxTexture* source_light_tex = nullptr;
blast::GfxTexture* dest_light_tex = nullptr;
// LightMap End

Model* quad_model = nullptr;
bool is_baked = false;

struct ObjectUniforms {
    glm::mat4 model_matrix;
    glm::mat4 view_matrix;
    glm::mat4 proj_matrix;
};

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec2 start_point = glm::vec2(-1.0f, -1.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool grabbing = false;
} camera;

struct LightmapParam {
    uint32_t width;
    uint32_t height;
} lightmap_param;

struct BakeParam {
    glm::vec4 bound_size;
    glm::vec4 to_cell_offset;
    glm::vec4 to_cell_size;
    glm::ivec2 atlas_size;
    float bias;
    uint32_t ray_count;
    uint32_t ray_to;
    uint32_t ray_from;
    uint32_t light_count;
    uint32_t grid_size;
} bake_param;

int main() {
    g_shader_compiler = new blast::VulkanShaderCompiler();

    g_device = new blast::VulkanDevice();

    // 加载shader资源
    {
        auto shaders = CompileShaderProgram(ProjectDir + "/Resources/Shaders/blit.vert", ProjectDir + "/Resources/Shaders/blit.frag");
        blit_vert_shader = shaders.first;
        blit_frag_shader = shaders.second;
    }
    {
        auto shaders = CompileShaderProgram(ProjectDir + "/Resources/Shaders/scene.vert", ProjectDir + "/Resources/Shaders/scene.frag");
        scene_vert_shader = shaders.first;
        scene_frag_shader = shaders.second;
    }
    {
        auto shaders = CompileShaderProgram(ProjectDir + "/Resources/Shaders/raster.vert", ProjectDir + "/Resources/Shaders/raster.frag");
        raster_vert_shader = shaders.first;
        raster_frag_shader = shaders.second;
    }
    {
        direct_light_shader = CompileComputeShader(ProjectDir + "/Resources/Shaders/direct_light.comp");
    }

    // 设置灯光
    std::vector<Light> lights;
    Light dir_lit;
    dir_lit.position = glm::vec3(0.0f, 100.0f, 0.0f);
    dir_lit.type = LIGHT_TYPE_DIRECTIONAL;
    dir_lit.direction_energy = glm::vec4(0.0f, -1.0f, 0.5f, 1.0f);
    dir_lit.color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    lights.push_back(dir_lit);

    // 加载场景资源
    std::vector<ObjectUniforms> object_storages;
    std::vector<Model*> builtin_scene = ImportScene(ProjectDir + "/Resources/Scenes/builtin.gltf");
    for (uint32_t i = 0; i < builtin_scene.size(); ++i) {
        builtin_scene[i]->GenerateGPUResource(g_device);
    }
    quad_model = builtin_scene[0];
    object_storages.push_back({});

    std::vector<Model*> display_scene = ImportScene(ProjectDir + "/Resources/Scenes/test.gltf");

    // 生成atlas并为场景模型分配atlas uv
    xatlas::Atlas* atlas = xatlas::Create();
    for (uint32_t i = 0; i < display_scene.size(); ++i) {
        xatlas::MeshDecl mesh_decl;
        mesh_decl.vertexCount = display_scene[i]->GetVertexCount();
        mesh_decl.vertexPositionData = display_scene[i]->GetPositionData();
        mesh_decl.vertexPositionStride = sizeof(glm::vec3);
        mesh_decl.indexData = display_scene[i]->GetIndexData();
        mesh_decl.indexCount = display_scene[i]->GetIndexCount();
        mesh_decl.indexFormat = display_scene[i]->GetIndexType() == blast::INDEX_TYPE_UINT16 ? xatlas::IndexFormat::UInt16 : xatlas::IndexFormat::UInt32;
        xatlas::AddMeshError ret = xatlas::AddMesh(atlas, mesh_decl);
    }
    xatlas::Generate(atlas);
    lightmap_param.width = atlas->width;
    lightmap_param.height = atlas->height;
    for (uint32_t i = 0; i < atlas->meshCount; ++i) {
        xatlas::Mesh& atlas_mesh = atlas->meshes[i];
        uint8_t* uv_data = new uint8_t[sizeof(glm::vec2) * atlas_mesh.vertexCount];
        float* uvs = (float*)uv_data;
        for (uint32_t j = 0; j < atlas_mesh.vertexCount; ++j) {
            uint32_t xref = atlas_mesh.vertexArray[j].xref;
            uvs[xref * 2] = atlas_mesh.vertexArray[j].uv[0] / atlas->width;
            uvs[xref * 2 + 1] = atlas_mesh.vertexArray[j].uv[1] / atlas->height;
            //printf("%d\n", atlas_mesh.vertexArray[j].xref);
        }
        for (int j = 0; j < display_scene[i]->GetVertexCount(); ++j) {
//            printf("x  %f\n", uvs[j*2]);
//            printf("y  %f\n", uvs[j*2+1]);
        }
        display_scene[i]->ResetUV1Data(uv_data);
    }

    xatlas::Destroy(atlas);

    for (uint32_t i = 0; i < display_scene.size(); ++i) {
        display_scene[i]->GenerateGPUResource(g_device);
        object_storages.push_back({});
    }

    // 创建光栅化RenderPass
    {
        blast::GfxTextureDesc texture_desc;
        texture_desc.width = lightmap_param.width;
        texture_desc.height = lightmap_param.height;
        texture_desc.format = blast::FORMAT_R32G32B32A32_FLOAT;
        texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_RENDER_TARGET | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
        texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        position_tex = g_device->CreateTexture(texture_desc);
        normal_tex = g_device->CreateTexture(texture_desc);
        unocclude_tex = g_device->CreateTexture(texture_desc);

        blast::GfxRenderPassDesc renderpass_desc = {};
        renderpass_desc.attachments.push_back(blast::RenderPassAttachment::RenderTarget(position_tex, -1, blast::LOAD_CLEAR));
        renderpass_desc.attachments.push_back(blast::RenderPassAttachment::RenderTarget(normal_tex, -1, blast::LOAD_CLEAR));
        renderpass_desc.attachments.push_back(blast::RenderPassAttachment::RenderTarget(unocclude_tex, -1, blast::LOAD_CLEAR));
        raster_renderpass = g_device->CreateRenderPass(renderpass_desc);
    }

    // 加载GPU Buffer
    {
        blast::GfxBufferDesc buffer_desc = {};
        buffer_desc.size = sizeof(ObjectUniforms) * object_storages.size();
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_UNIFORM_BUFFER;
        object_ub = g_device->CreateBuffer(buffer_desc);
    }

    // Acceleration Structures
    blast::GfxCommandBuffer* copy_cmd = g_device->RequestCommandBuffer(blast::QUEUE_COPY);
    AccelerationStructures* as = BuildAccelerationStructures(display_scene);
    {
        blast::GfxBufferBarrier buffer_barriers[5] = {};
        blast::GfxTextureBarrier texture_barrier = {};

        blast::GfxTextureDesc texture_desc;
        texture_desc.width = MAX_GRID_SIZE;
        texture_desc.height = MAX_GRID_SIZE;
        texture_desc.depth = MAX_GRID_SIZE;
        texture_desc.format = blast::FORMAT_R32G32_UINT;
        texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
        grid_tex = g_device->CreateTexture(texture_desc);

        texture_barrier.texture = grid_tex;
        texture_barrier.new_state = blast::RESOURCE_STATE_COPY_DEST;
        g_device->SetBarrier(copy_cmd, 0, nullptr, 1, &texture_barrier);

        g_device->UpdateTexture(copy_cmd, grid_tex, as->grid_indices.data());

        blast::GfxBufferDesc buffer_desc = {};
        buffer_desc.size = sizeof(Vertex) * as->vertices.size();
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
        vertex_buffer = g_device->CreateBuffer(buffer_desc);
        g_device->UpdateBuffer(copy_cmd, vertex_buffer, as->vertices.data(), sizeof(Vertex) * as->vertices.size());

        buffer_desc.size = sizeof(Triangle) * as->triangles.size();
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
        triangle_buffer = g_device->CreateBuffer(buffer_desc);
        g_device->UpdateBuffer(copy_cmd, triangle_buffer, as->triangles.data(), sizeof(Triangle) * as->triangles.size());

        if (as->seams.size() != 0) {
            buffer_desc.size = sizeof(Seam) * as->seams.size();
            buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
            buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
            seam_buffer = g_device->CreateBuffer(buffer_desc);
            g_device->UpdateBuffer(copy_cmd, seam_buffer, as->seams.data(), sizeof(Seam) * as->seams.size());
        } else {
            buffer_desc.size = sizeof(Seam);
            buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
            buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
            seam_buffer = g_device->CreateBuffer(buffer_desc);
        }

        buffer_desc.size = sizeof(uint32_t) * as->triangle_indices.size();
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
        triangle_index_buffer = g_device->CreateBuffer(buffer_desc);
        g_device->UpdateBuffer(copy_cmd, triangle_index_buffer, as->triangle_indices.data(), sizeof(uint32_t) * as->triangle_indices.size());

        buffer_desc.size = sizeof(Light) * lights.size();
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
        light_buffer = g_device->CreateBuffer(buffer_desc);
        g_device->UpdateBuffer(copy_cmd, light_buffer, lights.data(), sizeof(Light) * lights.size());

        buffer_barriers[0].buffer = vertex_buffer;
        buffer_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE | blast::RESOURCE_STATE_UNORDERED_ACCESS;
        buffer_barriers[1].buffer = triangle_buffer;
        buffer_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE | blast::RESOURCE_STATE_UNORDERED_ACCESS;
        buffer_barriers[2].buffer = seam_buffer;
        buffer_barriers[2].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE | blast::RESOURCE_STATE_UNORDERED_ACCESS;
        buffer_barriers[3].buffer = triangle_index_buffer;
        buffer_barriers[3].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE | blast::RESOURCE_STATE_UNORDERED_ACCESS;
        buffer_barriers[4].buffer = light_buffer;
        buffer_barriers[4].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE | blast::RESOURCE_STATE_UNORDERED_ACCESS;
        texture_barrier.texture = grid_tex;
        texture_barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;

        g_device->SetBarrier(copy_cmd, 4, buffer_barriers, 1, &texture_barrier);
    }

    // LightMap
    {

//        blast::GfxBufferBarrier buffer_barrier = {};
//        buffer_barrier.buffer = light_buffer;
//        buffer_barrier.new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
//        g_device->SetBarrier(copy_cmd, 1, &buffer_barrier, 0, nullptr);

        blast::GfxTextureDesc texture_desc;
        texture_desc.width = lightmap_param.width;
        texture_desc.height = lightmap_param.height;
        texture_desc.format = blast::FORMAT_R32G32B32A32_FLOAT;
        texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
        source_light_tex = g_device->CreateTexture(texture_desc);
        dest_light_tex = g_device->CreateTexture(texture_desc);

        blast::GfxSamplerDesc sampler_desc = {};
        linear_sampler = g_device->CreateSampler(sampler_desc);
        sampler_desc.min_filter = blast::FILTER_NEAREST;
        sampler_desc.mag_filter = blast::FILTER_NEAREST;
        nearest_sampler = g_device->CreateSampler(sampler_desc);
    }

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lightmapper", nullptr, nullptr);
    glfwSetCursorPosCallback(window, CursorPositionCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, MouseScrollCallback);

    int frame_width = 0, frame_height = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);

        if (window_width == 0 || window_height == 0) {
            continue;
        }

        if (frame_width != window_width || frame_height != window_height) {
            frame_width = window_width;
            frame_height = window_height;
            RefreshSwapchain(glfwGetWin32Window(window), frame_width, frame_height);
        }

        blast::GfxCommandBuffer* cmd = g_device->RequestCommandBuffer(blast::QUEUE_GRAPHICS);

        // 烘培
        if (!is_baked) {
            is_baked = true;

            // raster
            blast::GfxTextureBarrier texture_barriers[3];
            texture_barriers[0].texture = position_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            texture_barriers[1].texture = normal_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            texture_barriers[2].texture = unocclude_tex;
            texture_barriers[2].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            g_device->SetBarrier(cmd, 0, nullptr, 3, texture_barriers);

            g_device->RenderPassBegin(cmd, raster_renderpass);

            blast::Viewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.w = lightmap_param.width;
            viewport.h = lightmap_param.height;
            g_device->BindViewports(cmd, 1, &viewport);

            blast::Rect rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = lightmap_param.width;
            rect.bottom = lightmap_param.height;
            g_device->BindScissorRects(cmd, 1, &rect);

            g_device->BindPipeline(cmd, raster_triangle_pipeline);

            g_device->BindUAV(cmd, vertex_buffer, 0);

            g_device->BindUAV(cmd, triangle_buffer, 1);

            g_device->Draw(cmd, as->triangles.size() * 3, 0);

            g_device->BindPipeline(cmd, raster_line_pipeline);

            g_device->BindUAV(cmd, vertex_buffer, 0);

            g_device->BindUAV(cmd, triangle_buffer, 1);

            //g_device->Draw(cmd, as->triangles.size() * 3, 0);

            g_device->RenderPassEnd(cmd);

            texture_barriers[0].texture = position_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[1].texture = normal_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[2].texture = unocclude_tex;
            texture_barriers[2].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(cmd, 0, nullptr, 3, texture_barriers);

            // ray trace
            bake_param.atlas_size = glm::ivec2(lightmap_param.width, lightmap_param.height);
            bake_param.grid_size = MAX_GRID_SIZE;
            bake_param.bias = 0.0f;
            bake_param.light_count = lights.size();
            bake_param.bound_size = glm::vec4(as->bounds.size, 0.0f);
            bake_param.to_cell_offset = glm::vec4(as->bounds.position, 0.0f);
            bake_param.to_cell_size.x = (1.0f / as->bounds.size.x) * float(MAX_GRID_SIZE);
            bake_param.to_cell_size.y = (1.0f / as->bounds.size.y) * float(MAX_GRID_SIZE);
            bake_param.to_cell_size.z = (1.0f / as->bounds.size.z) * float(MAX_GRID_SIZE);

            // step1
            texture_barriers[0].texture = source_light_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
            g_device->SetBarrier(cmd, 0, nullptr, 1, texture_barriers);

            g_device->BindComputeShader(cmd, direct_light_shader);

            g_device->BindUAV(cmd, vertex_buffer, 0);

            g_device->BindUAV(cmd, triangle_buffer, 1);

            g_device->BindUAV(cmd, triangle_index_buffer, 2);

            g_device->BindUAV(cmd, light_buffer, 3);

            g_device->BindUAV(cmd, source_light_tex, 4);

            g_device->BindSampler(cmd, linear_sampler, 0);

            g_device->BindSampler(cmd, nearest_sampler, 1);

            g_device->BindResource(cmd, position_tex, 0);

            g_device->BindResource(cmd, normal_tex, 1);

            g_device->BindResource(cmd, grid_tex, 2);

            g_device->PushConstants(cmd, &bake_param, sizeof(BakeParam));

            g_device->Dispatch(cmd, std::max(1u, (uint32_t)(lightmap_param.width) / 16), std::max(1u, (uint32_t)(lightmap_param.height) / 16), 1);

            texture_barriers[0].texture = source_light_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(cmd, 0, nullptr, 1, texture_barriers);

            // step2
        }

        // 更新Object Uniform
        object_storages[0].model_matrix = glm::toMat4(glm::quat(glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f)));
        object_storages[0].view_matrix = glm::mat4(1.0f);
        object_storages[0].proj_matrix = glm::mat4(1.0f);

        glm::mat4 view_matrix = glm::lookAt(camera.position, camera.position + camera.front, camera.up);

        float fov = glm::radians(45.0);
        float aspect = frame_width / frame_height;
        glm::mat4 proj_matrix = glm::perspective(fov, aspect, 0.1f, 100.0f);
        proj_matrix[1][1] *= -1;

        for (uint32_t i = 0; i < display_scene.size(); ++i) {
            object_storages[i + 1].model_matrix = glm::mat4(1.0f);//display_scene[i]->GetModelMatriax();
            object_storages[i + 1].view_matrix = view_matrix;
            object_storages[i + 1].proj_matrix = proj_matrix;
        }
        g_device->UpdateBuffer(cmd, object_ub, object_storages.data(), sizeof(ObjectUniforms) * object_storages.size());

        // 绘制场景
        {
            blast::GfxTextureBarrier texture_barriers[2];
            texture_barriers[0].texture = scene_color_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            texture_barriers[1].texture = scene_depth_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_DEPTH_WRITE;
            g_device->SetBarrier(cmd, 0, nullptr, 2, texture_barriers);

            g_device->RenderPassBegin(cmd, scene_renderpass);

            for (uint32_t i = 0; i < display_scene.size(); ++i) {
                blast::Viewport viewport;
                viewport.x = 0;
                viewport.y = 0;
                viewport.w = frame_width;
                viewport.h = frame_height;
                g_device->BindViewports(cmd, 1, &viewport);

                blast::Rect rect;
                rect.left = 0;
                rect.top = 0;
                rect.right = frame_width;
                rect.bottom = frame_height;
                g_device->BindScissorRects(cmd, 1, &rect);

                g_device->BindPipeline(cmd, scene_pipeline);

                g_device->BindResource(cmd, source_light_tex, 0);

                g_device->BindSampler(cmd, linear_sampler, 0);

                g_device->BindConstantBuffer(cmd, object_ub, 0, sizeof(ObjectUniforms), (i + 1) * sizeof(ObjectUniforms));

                blast::GfxBuffer* vertex_buffers[] = {display_scene[i]->GetVertexBuffer()};
                uint64_t vertex_offsets[] = {0};
                g_device->BindVertexBuffers(cmd, vertex_buffers, 0, 1, vertex_offsets);

                g_device->BindIndexBuffer(cmd, display_scene[i]->GetIndexBuffer(), display_scene[i]->GetIndexType(), 0);

                g_device->DrawIndexed(cmd, display_scene[i]->GetIndexCount(), 0, 0);
            }

            g_device->RenderPassEnd(cmd);

            texture_barriers[0].texture = scene_color_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[1].texture = scene_depth_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(cmd, 0, nullptr, 2, texture_barriers);
        }

        g_device->RenderPassBegin(cmd, g_swapchain);
        {
            g_device->BindPipeline(cmd, blit_pipeline);

            blast::Viewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.w = frame_width;
            viewport.h = frame_height;
            g_device->BindViewports(cmd, 1, &viewport);

            blast::Rect rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = frame_width;
            rect.bottom = frame_height;
            g_device->BindScissorRects(cmd, 1, &rect);

            g_device->BindResource(cmd, scene_color_tex, 0);

            g_device->BindSampler(cmd, linear_sampler, 0);

            g_device->BindConstantBuffer(cmd, object_ub, 0, sizeof(ObjectUniforms), 0);

            blast::GfxBuffer* vertex_buffers[] = {quad_model->GetVertexBuffer()};
            uint64_t vertex_offsets[] = {0};
            g_device->BindVertexBuffers(cmd, vertex_buffers, 0, 1, vertex_offsets);

            g_device->BindIndexBuffer(cmd, quad_model->GetIndexBuffer(), quad_model->GetIndexType(), 0);

            g_device->DrawIndexed(cmd, quad_model->GetIndexCount(), 0, 0);

            viewport.x = frame_width * 0.75f;
            viewport.y = frame_height * 0.75f;
            viewport.w = frame_width * 0.25f;
            viewport.h = frame_height * 0.25f;
            g_device->BindViewports(cmd, 1, &viewport);

            rect.left = 0;
            rect.top = 0;
            rect.right = frame_width;
            rect.bottom = frame_height;
            g_device->BindScissorRects(cmd, 1, &rect);

            g_device->BindResource(cmd, source_light_tex, 0);

            g_device->DrawIndexed(cmd, quad_model->GetIndexCount(), 0, 0);
        }
        g_device->RenderPassEnd(cmd);

        g_device->SubmitAllCommandBuffer();
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    // 清除场景资源
    for (uint32_t i = 0; i < builtin_scene.size(); ++i) {
        builtin_scene[i]->ReleaseGPUResource(g_device);
        SAFE_DELETE(builtin_scene[i]);
    }
    for (uint32_t i = 0; i < display_scene.size(); ++i) {
        display_scene[i]->ReleaseGPUResource(g_device);
        SAFE_DELETE(display_scene[i]);
    }

    // 清空shader资源
    g_device->DestroyShader(blit_vert_shader);
    g_device->DestroyShader(blit_frag_shader);
    g_device->DestroyShader(scene_vert_shader);
    g_device->DestroyShader(scene_frag_shader);
    g_device->DestroyShader(raster_vert_shader);
    g_device->DestroyShader(raster_frag_shader);
    g_device->DestroyShader(direct_light_shader);

    // 清除光栅化RenderPass资源
    g_device->DestroyTexture(position_tex);
    g_device->DestroyTexture(normal_tex);
    g_device->DestroyTexture(unocclude_tex);
    g_device->DestroyRenderPass(raster_renderpass);

    // 销毁GPU Buffer
    g_device->DestroyBuffer(object_ub);

    // Acceleration Structures
    SAFE_DELETE(as);
    g_device->DestroyBuffer(seam_buffer);
    g_device->DestroyBuffer(vertex_buffer);
    g_device->DestroyBuffer(triangle_buffer);
    g_device->DestroyBuffer(triangle_index_buffer);
    g_device->DestroyTexture(grid_tex);

    // LightMap
    g_device->DestroySampler(linear_sampler);
    g_device->DestroySampler(nearest_sampler);
    g_device->DestroyBuffer(light_buffer);
    g_device->DestroyTexture(source_light_tex);
    g_device->DestroyTexture(dest_light_tex);

    if (scene_renderpass) {
        g_device->DestroyTexture(scene_color_tex);
        g_device->DestroyTexture(scene_depth_tex);
        g_device->DestroyRenderPass(scene_renderpass);
    }

    // 清空管线资源
    if (blit_pipeline) {
        g_device->DestroyPipeline(blit_pipeline);
    }

    if (scene_pipeline) {
        g_device->DestroyPipeline(scene_pipeline);
    }
    if (raster_line_pipeline) {
        g_device->DestroyPipeline(raster_line_pipeline);
    }

    if (raster_triangle_pipeline) {
        g_device->DestroyPipeline(raster_triangle_pipeline);
    }

    g_device->DestroySwapChain(g_swapchain);

    SAFE_DELETE(g_device);
    SAFE_DELETE(g_shader_compiler);
    return 0;
}

void RefreshSwapchain(void* window, uint32_t width, uint32_t height) {
    if (scene_renderpass) {
        g_device->DestroyTexture(scene_color_tex);
        g_device->DestroyTexture(scene_depth_tex);
        g_device->DestroyRenderPass(scene_renderpass);
    }
    blast::GfxTextureDesc texture_desc = {};
    texture_desc.width = width;
    texture_desc.height = height;
    texture_desc.sample_count = blast::SAMPLE_COUNT_1;
    texture_desc.format = blast::FORMAT_R8G8B8A8_UNORM;
    texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_RENDER_TARGET;
    texture_desc.clear.color[0] = 0.0f;
    texture_desc.clear.color[1] = 0.0f;
    texture_desc.clear.color[2] = 0.0f;
    texture_desc.clear.color[3] = 0.0f;
    // 默认深度值为1
    texture_desc.clear.depthstencil.depth = 1.0f;
    scene_color_tex = g_device->CreateTexture(texture_desc);

    texture_desc.format = blast::FORMAT_D24_UNORM_S8_UINT;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_DEPTH_STENCIL;
    scene_depth_tex = g_device->CreateTexture(texture_desc);

    blast::GfxRenderPassDesc renderpass_desc = {};
    renderpass_desc.attachments.push_back(blast::RenderPassAttachment::RenderTarget(scene_color_tex, -1, blast::LOAD_CLEAR));
    renderpass_desc.attachments.push_back(
            blast::RenderPassAttachment::DepthStencil(
                    scene_depth_tex,
                    -1,
                    blast::LOAD_CLEAR,
                    blast::STORE_STORE
            )
    );
    scene_renderpass = g_device->CreateRenderPass(renderpass_desc);

    blast::GfxSwapChainDesc swapchain_desc;
    swapchain_desc.window = window;
    swapchain_desc.width = width;
    swapchain_desc.height = height;
    swapchain_desc.clear_color[0] = 1.0f;
    g_swapchain = g_device->CreateSwapChain(swapchain_desc, g_swapchain);

    blast::GfxInputLayout input_layout = {};
    blast::GfxInputLayout::Element input_element;
    input_element.semantic = blast::SEMANTIC_POSITION;
    input_element.format = blast::FORMAT_R32G32B32_FLOAT;
    input_element.binding = 0;
    input_element.location = 0;
    input_element.offset = offsetof(MeshVertex, position);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_NORMAL;
    input_element.format = blast::FORMAT_R32G32B32_FLOAT;
    input_element.binding = 0;
    input_element.location = 1;
    input_element.offset = offsetof(MeshVertex, normal);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_TEXCOORD0;
    input_element.format = blast::FORMAT_R32G32_FLOAT;
    input_element.binding = 0;
    input_element.location = 2;
    input_element.offset = offsetof(MeshVertex, uv0);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_TEXCOORD1;
    input_element.format = blast::FORMAT_R32G32_FLOAT;
    input_element.binding = 0;
    input_element.location = 3;
    input_element.offset = offsetof(MeshVertex, uv1);
    input_layout.elements.push_back(input_element);

    blast::GfxBlendState blend_state = {};
    blend_state.rt[0].src_factor = blast::BLEND_ONE;
    blend_state.rt[0].dst_factor = blast::BLEND_ZERO;
    blend_state.rt[0].src_factor_alpha = blast::BLEND_ONE;
    blend_state.rt[0].dst_factor_alpha = blast::BLEND_ZERO;

    blast::GfxDepthStencilState depth_stencil_state = {};
    depth_stencil_state.depth_test = true;
    depth_stencil_state.depth_write = true;

    blast::GfxRasterizerState rasterizer_state = {};
    rasterizer_state.cull_mode = blast::CULL_NONE;
    rasterizer_state.front_face = blast::FRONT_FACE_CW;
    rasterizer_state.fill_mode = blast::FILL_SOLID;

    // 创建blit管线
    {
        if (blit_pipeline) {
            g_device->DestroyPipeline(blit_pipeline);
        }

        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.sc = g_swapchain;
        pipeline_desc.vs = blit_vert_shader;
        pipeline_desc.fs = blit_frag_shader;
        pipeline_desc.il = &input_layout;
        pipeline_desc.bs = &blend_state;
        pipeline_desc.rs = &rasterizer_state;
        pipeline_desc.dss = &depth_stencil_state;
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
        blit_pipeline = g_device->CreatePipeline(pipeline_desc);
    }

    // 创建scene管线
    {
        if (scene_pipeline) {
            g_device->DestroyPipeline(scene_pipeline);
        }

        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.rp = scene_renderpass;
        pipeline_desc.vs = scene_vert_shader;
        pipeline_desc.fs = scene_frag_shader;
        pipeline_desc.il = &input_layout;
        pipeline_desc.bs = &blend_state;
        pipeline_desc.rs = &rasterizer_state;
        pipeline_desc.dss = &depth_stencil_state;
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
        scene_pipeline = g_device->CreatePipeline(pipeline_desc);
    }

    // raster pipeline
    {
        if (raster_line_pipeline) {
            g_device->DestroyPipeline(raster_line_pipeline);
        }

        if (raster_triangle_pipeline) {
            g_device->DestroyPipeline(raster_triangle_pipeline);
        }

        blast::GfxInputLayout null_input_layout = {};
        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.rp = raster_renderpass;
        pipeline_desc.vs = raster_vert_shader;
        pipeline_desc.fs = raster_frag_shader;
        pipeline_desc.il = &null_input_layout;
        pipeline_desc.bs = &blend_state;
        pipeline_desc.rs = &rasterizer_state;
        pipeline_desc.dss = &depth_stencil_state;
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_LINE_LIST;
        raster_line_pipeline = g_device->CreatePipeline(pipeline_desc);
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
        raster_triangle_pipeline = g_device->CreatePipeline(pipeline_desc);
    }
}

static std::pair<blast::GfxShader*, blast::GfxShader*> CompileShaderProgram(const std::string& vs_path, const std::string& fs_path) {
    blast::GfxShader* vert_shader = nullptr;
    blast::GfxShader* frag_shader = nullptr;
    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(vs_path);
        compile_desc.stage = blast::SHADER_STAGE_VERT;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_VERT;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        vert_shader = g_device->CreateShader(shader_desc);
    }

    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(fs_path);
        compile_desc.stage = blast::SHADER_STAGE_FRAG;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_FRAG;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        frag_shader = g_device->CreateShader(shader_desc);
    }

    return std::make_pair(vert_shader, frag_shader);
}

static blast::GfxShader* CompileComputeShader(const std::string& cs_path) {
    blast::GfxShader* comp_shader = nullptr;
    blast::ShaderCompileDesc compile_desc;
    compile_desc.code = ReadFileData(cs_path);
    compile_desc.stage = blast::SHADER_STAGE_COMP;
    blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
    blast::GfxShaderDesc shader_desc;
    shader_desc.stage = blast::SHADER_STAGE_COMP;
    shader_desc.bytecode = compile_result.bytes.data();
    shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
    comp_shader = g_device->CreateShader(shader_desc);
    return comp_shader;
}

static void CursorPositionCallback(GLFWwindow* window, double pos_x, double pos_y) {
    if (!camera.grabbing) {
        return;
    }

    glm::vec2 offset = camera.start_point - glm::vec2(pos_x, pos_y);
    camera.start_point = glm::vec2(pos_x, pos_y);

    camera.yaw -= offset.x * 0.1f;
    camera.pitch += offset.y * 0.1f;
    glm::vec3 front;
    front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    front.y = sin(glm::radians(camera.pitch));
    front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    camera.front = glm::normalize(front);
    camera.right = glm::normalize(glm::cross(camera.front, glm::vec3(0, 1, 0)));
    camera.up = glm::normalize(glm::cross(camera.right, camera.front));
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    switch (action) {
        case 0:
            // Button Up
            if (button == 1) {
                camera.grabbing = false;
            }
            break;
        case 1:
            // Button Down
            if (button == 1) {
                camera.grabbing = true;
                camera.start_point = glm::vec2(x, y);
            }
            break;
        default:
            break;
    }
}

static void MouseScrollCallback(GLFWwindow* window, double offset_x, double offset_y) {
    camera.position += camera.front * (float)offset_y * 0.1f;
}