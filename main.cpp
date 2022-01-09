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

static void RefreshSwapchain(void* window, uint32_t width, uint32_t height);

blast::ShaderCompiler* g_shader_compiler = nullptr;
blast::GfxDevice* g_device = nullptr;
blast::GfxSwapChain* g_swapchain = nullptr;
blast::GfxShader* blit_vert_shader = nullptr;
blast::GfxShader* blit_frag_shader = nullptr;
blast::GfxPipeline* blit_pipeline = nullptr;
Model* quad_model = nullptr;

int main() {
    g_shader_compiler = new blast::VulkanShaderCompiler();

    g_device = new blast::VulkanDevice();

    // 加载shader资源
    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(ProjectDir + "/Resources/Shaders/blit.vert");
        compile_desc.stage = blast::SHADER_STAGE_VERT;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_VERT;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        blit_vert_shader = g_device->CreateShader(shader_desc);
    }

    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(ProjectDir + "/Resources/Shaders/blit.frag");
        compile_desc.stage = blast::SHADER_STAGE_FRAG;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_FRAG;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        blit_frag_shader = g_device->CreateShader(shader_desc);
    }

    // 加载场景资源
    std::vector<Model*> builtin_scene = ImportScene(ProjectDir + "/Resources/Scenes/builtin.gltf");
    for (uint32_t i = 0; i < builtin_scene.size(); ++i) {
        builtin_scene[i]->GenerateGPUResource(g_device);
    }
    quad_model = builtin_scene[0];

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lightmapper", nullptr, nullptr);
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

    }
    glfwDestroyWindow(window);
    glfwTerminate();

    // 清除场景资源
    for (uint32_t i = 0; i < builtin_scene.size(); ++i) {
        builtin_scene[i]->ReleaseGPUResource(g_device);
        SAFE_DELETE(builtin_scene[i]);
    }

    // 清空shader资源
    g_device->DestroyShader(blit_vert_shader);
    g_device->DestroyShader(blit_frag_shader);

    // 清空管线资源
    if (blit_pipeline) {
        g_device->DestroyPipeline(blit_pipeline);
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

    // 创建blit管线
    {
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

        if (blit_pipeline) {
            g_device->DestroyPipeline(blit_pipeline);
        }

        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.sc = g_swapchain;
        pipeline_desc.vs = blit_vert_shader;
        pipeline_desc.fs = blit_frag_shader;
        pipeline_desc.il = quad_model->GetInputLayout();
        pipeline_desc.bs = &blend_state;
        pipeline_desc.rs = &rasterizer_state;
        pipeline_desc.dss = &depth_stencil_state;
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
        blit_pipeline = g_device->CreatePipeline(pipeline_desc);
    }
}