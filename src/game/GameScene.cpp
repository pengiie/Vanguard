#include "GameScene.h"

#include "../Application.h"

static const std::vector<std::string> assets = {
    "shaders/gbuffer.vert.glsl",
    "shaders/gbuffer.frag.glsl",
    "shaders/march.comp.glsl"
};

namespace vanguard {
    void GameScene::init() {
        for (const auto& asset: assets) {
            Application::Get().getAssets().load(asset);
        }

        m_camera.init();
        m_world.init();

        Application::Get().getScheduler().scheduleRepeatingTask([&] {
            uint32_t currentFrameCount = Application::Get().getRenderSystem().getFrameCount();
            uint32_t fps = currentFrameCount - m_lastFrame;
            INFO("FPS: {}", fps);
            m_lastFrame = currentFrameCount;
        }, std::chrono::milliseconds(0), std::chrono::milliseconds(1000));

        Application::Get().getAssets().finishLoading();
    }

    void GameScene::update(float deltaTime) {
        FTIMER();
        if(Input::isKeyPressed(Key::Escape)) {
            Application::Get().stop();
        }
        if(Input::isKeyPressed(Key::F1)) {
            Application::Get().getWindow().toggleCursor();
        }
        if(Input::isKeyPressed(Key::F2)) {
            auto& assetManager = Application::Get().getAssets();
            assetManager.unload("shaders/march.comp.glsl");
            assetManager.load("shaders/march.comp.glsl");
            assetManager.finishLoading();

            FrameGraphBuilder builder;
            buildFrameGraph(builder);
            Application::Get().getRenderSystem().bake(std::move(builder));
        }

        m_camera.update(deltaTime);
        //m_world.update(m_camera);
    }

    void GameScene::buildFrameGraph(FrameGraphBuilder& builder) {
        auto& assets = Application::Get().getAssets();

        const ResourceRef cameraUniform = builder.createUniformBuffer("CameraUniform", {
            .buffer = m_camera.getCameraBuffer(),
            .frequency = UniformFrequency::PerFrame,
            .binding = 0
        });
        const ResourceRef diffuse = builder.createImage("GBufferDiffuse", {
            .format = vk::Format::eR8G8B8A8Unorm,
            .clearColor = { 0.0f, 1.0f, 1.0f },
        });
        const ResourceRef gBufferStencil = builder.createStencil("GBufferStencil", { .format = vk::Format::eD24UnormS8Uint });

        // GBuffer pass
        RenderPassInfo gBufferPass{};
        gBufferPass.name = "TerrainGBuffer";
        gBufferPass.inputs = { cameraUniform };
        gBufferPass.outputs = { diffuse, gBufferStencil };
        gBufferPass.vertexInput = VoxelVertex::getVertexInputData();
        gBufferPass.vertexShader = assets.get<SpirVShaderCode>("shaders/gbuffer.vert.glsl");
        gBufferPass.fragmentShader = assets.get<SpirVShaderCode>("shaders/gbuffer.frag.glsl");
        gBufferPass.execution = [this](const vk::CommandBuffer& cmd) {
            for (const ChunkMesh* mesh : m_world.getRenderableChunks(m_camera)) {
                mesh->render(cmd);
            }
        };
        builder.addRenderPass(gBufferPass);

        const ResourceRef marchImage = builder.createImage("MarchImage", {
            .format = vk::Format::eR8G8B8A8Unorm,
        });
        const ResourceRef marchImageUniform = builder.createUniformImage("MarchImageUniform", {
            .image = marchImage,
            .type = UniformImageType::Storage,
            .frequency = UniformFrequency::PerFrame,
            .binding = 1
        });
        ComputePassInfo computePass{};
        computePass.name = "RayMarchCompute";
        computePass.outputs = { marchImage };
        computePass.computeShader = assets.get<SpirVShaderCode>("shaders/march.comp.glsl");
        computePass.execution = [this](const vk::CommandBuffer& cmd) {
            auto& window = Application::Get().getWindow();
            cmd.dispatch(ceil((float) window.getWidth() / 16.0f), ceil((float) window.getHeight() / 16.0f), 1);
        };
        builder.addComputePass(computePass);

        builder.setBackBuffer(marchImage);
    }
}