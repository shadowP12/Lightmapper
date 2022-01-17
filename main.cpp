#include "LightMapperDefine.h"
#include "Importer.h"
#include "Model.h"

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
blast::GfxBuffer* object_ub = nullptr;
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
            uvs[j * 2] = atlas_mesh.vertexArray[j].uv[0] / atlas->width;
            uvs[j * 2 + 1] = atlas_mesh.vertexArray[j].uv[1]/ atlas->height;
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

            blast::GfxTextureBarrier texture_barriers[3];
            texture_barriers[0].texture = position_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            texture_barriers[1].texture = normal_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            texture_barriers[2].texture = unocclude_tex;
            texture_barriers[2].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            g_device->SetBarrier(cmd, 0, nullptr, 3, texture_barriers);



            texture_barriers[0].texture = position_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[1].texture = normal_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[2].texture = unocclude_tex;
            texture_barriers[2].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(cmd, 0, nullptr, 3, texture_barriers);
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
        g_device->RenderPassBegin(cmd, g_swapchain);
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

            g_device->BindConstantBuffer(cmd, object_ub, 0, sizeof(ObjectUniforms), (i + 1) * sizeof(ObjectUniforms));

            blast::GfxBuffer* vertex_buffers[] = {display_scene[i]->GetVertexBuffer()};
            uint64_t vertex_offsets[] = {0};
            g_device->BindVertexBuffers(cmd, vertex_buffers, 0, 1, vertex_offsets);

            g_device->BindIndexBuffer(cmd, display_scene[i]->GetIndexBuffer(), display_scene[i]->GetIndexType(), 0);

            g_device->DrawIndexed(cmd, display_scene[i]->GetIndexCount(), 0, 0);
        }

        // 绘制平面
        {
            blast::Viewport viewport;
            viewport.x = frame_width * 0.75f;
            viewport.y = frame_height * 0.75f;
            viewport.w = frame_width * 0.25f;
            viewport.h = frame_height * 0.25f;
            g_device->BindViewports(cmd, 1, &viewport);

            blast::Rect rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = frame_width;
            rect.bottom = frame_height;
            g_device->BindScissorRects(cmd, 1, &rect);

            g_device->BindPipeline(cmd, blit_pipeline);

            g_device->BindConstantBuffer(cmd, object_ub, 0, sizeof(ObjectUniforms), 0);

            blast::GfxBuffer* vertex_buffers[] = {quad_model->GetVertexBuffer()};
            uint64_t vertex_offsets[] = {0};
            g_device->BindVertexBuffers(cmd, vertex_buffers, 0, 1, vertex_offsets);

            g_device->BindIndexBuffer(cmd, quad_model->GetIndexBuffer(), quad_model->GetIndexType(), 0);

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

    // 清除光栅化RenderPass资源
    g_device->DestroyTexture(position_tex);
    g_device->DestroyTexture(normal_tex);
    g_device->DestroyTexture(unocclude_tex);
    g_device->DestroyRenderPass(raster_renderpass);

    // 销毁GPU Buffer
    g_device->DestroyBuffer(object_ub);

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
    input_element.offset = offsetof(Vertex, position);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_NORMAL;
    input_element.format = blast::FORMAT_R32G32B32_FLOAT;
    input_element.binding = 0;
    input_element.location = 1;
    input_element.offset = offsetof(Vertex, normal);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_TEXCOORD0;
    input_element.format = blast::FORMAT_R32G32_FLOAT;
    input_element.binding = 0;
    input_element.location = 2;
    input_element.offset = offsetof(Vertex, uv0);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_TEXCOORD1;
    input_element.format = blast::FORMAT_R32G32_FLOAT;
    input_element.binding = 0;
    input_element.location = 3;
    input_element.offset = offsetof(Vertex, uv1);
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
        pipeline_desc.sc = g_swapchain;
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
        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.rp = raster_renderpass;
        pipeline_desc.vs = raster_vert_shader;
        pipeline_desc.fs = raster_frag_shader;
        pipeline_desc.il = &input_layout;
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