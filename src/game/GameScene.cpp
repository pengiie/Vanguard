#include "GameScene.h"

#include "../Application.h"

namespace vanguard {
    void GameScene::init() {
        loadAssets(Application::Get().getAssets());

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

    void GameScene::loadAssets(Assets& assets) {
        assets.load("assets/shaders/gbuffer.vert.glsl");
        assets.load("assets/shaders/gbuffer.frag.glsl");
    }

    void GameScene::update(float deltaTime) {
        FTIMER();
        if(Input::isKeyPressed(Key::Escape)) {
            Application::Get().stop();
        }
        if(Input::isKeyPressed(Key::F1)) {
            Application::Get().getWindow().toggleCursor();
        }

        m_camera.update(deltaTime);
        m_world.update(m_camera);
    }

    void GameScene::buildRenderGraph(RenderGraphBuilder& builder) {
        auto& assets = Application::Get().getAssets();

        const ResourceRef cameraUniform = builder.createUniform("CameraUniform", {
            .buffer = m_camera.getCameraBuffer(),
            .frequency = UniformFrequency::PerFrame,
            .binding = 0
        });
        const ResourceRef diffuse = builder.createImage("GBufferDiffuse", { .format = vk::Format::eR8G8B8A8Unorm });
        const ResourceRef gBufferStencil = builder.createStencil("GBufferStencil", { .format = vk::Format::eD24UnormS8Uint });

        // TODO: Definition of additional render pass info like which to load/store, etc.
        builder.addRenderPass({
            .name = "TerrainGBuffer",
            .inputs = {
                cameraUniform,
            },
            .outputs = {
                diffuse,
                gBufferStencil
            },
            .vertexInput = VoxelVertex::getVertexInputData(),
            .clearColor = { 0.0f, 1.0f, 1.0f, 1.0f },
            .vertexShader = assets.get<SpirVShaderCode>("assets/shaders/gbuffer.vert.glsl"),
            .fragmentShader = assets.get<SpirVShaderCode>("assets/shaders/gbuffer.frag.glsl"),
            .execution = [this](const vk::CommandBuffer& cmd, uint32_t frameIndex) {
                for (const ChunkMesh* mesh: m_world.getRenderableChunks(m_camera)) {
                    mesh->render(cmd);
                }
            }
        });
        builder.setBackBuffer(diffuse);
    }


}