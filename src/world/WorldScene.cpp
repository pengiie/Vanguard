#include "WorldScene.h"

#include "../Application.h"
#include "terrain/Terrain.h"
#include "terrain/Voxel.h"

namespace vanguard {
    void WorldScene::init() {
        loadAssets(Application::Get().getAssets());

        m_camera.init();
        m_terrain.init(&m_camera);

        Application::Get().getScheduler().scheduleRepeatingTask([&] {
            uint32_t currentFrameCount = Application::Get().getRenderSystem().getFrameCount();
            uint32_t fps = currentFrameCount - m_lastFrame;
            INFO("FPS: {}", fps);
            m_lastFrame = currentFrameCount;
        }, std::chrono::milliseconds(0), std::chrono::milliseconds(1000));

        Application::Get().getAssets().finishLoading();
    }

    void WorldScene::loadAssets(Assets& assets) {
        assets.load("assets/shaders/gbuffer.vert.glsl");
        assets.load("assets/shaders/gbuffer.frag.glsl");
    }

    void WorldScene::update(float deltaTime) {
        FTIMER();
        if(Input::isKeyPressed(Key::Escape)) {
            Application::Get().stop();
        }
        if(Input::isKeyPressed(Key::F1)) {
            Application::Get().getWindow().toggleCursor();
        }

        m_camera.update(deltaTime);
        m_terrain.update(m_camera.getPosition());
    }

    void WorldScene::buildRenderGraph(RenderGraphBuilder& builder) {
        auto& assets = Application::Get().getAssets();

        const ResourceRef cameraUniform = builder.createUniform("CameraUniform", {
            .buffer = m_camera.getCameraBuffer(),
            .frequency = UniformFrequency::PerFrame,
            .binding = 0
        });
        const ResourceRef terrainColorPalette = builder.createUniform("TerrainColorPaletteUniform", {
            .buffer = m_terrain.getTerrainColorPaletteBuffer(),
            .frequency = UniformFrequency::PerFrame,
            .binding = 1
        });
        const ResourceRef diffuse = builder.createImage("GBufferDiffuse", { .format = vk::Format::eR8G8B8A8Unorm });
        const ResourceRef gBufferStencil = builder.createStencil("GBufferStencil", { .format = vk::Format::eD24UnormS8Uint });

        // TODO: Definition of additional render pass info like which to load/store, etc.
        builder.addRenderPass({
            .name = "TerrainGBuffer",
            .inputs = {
                cameraUniform,
                terrainColorPalette
            },
            .outputs = {
                diffuse,
                gBufferStencil
            },
            .vertexInput = TerrainVoxelVertex::getVertexInputData(),
            .clearColor = { 0.0f, 1.0f, 1.0f, 1.0f },
            .vertexShader = assets.get<SpirVShaderCode>("assets/shaders/gbuffer.vert.glsl"),
            .fragmentShader = assets.get<SpirVShaderCode>("assets/shaders/gbuffer.frag.glsl"),
            .execution = [this](const vk::CommandBuffer& cmd, uint32_t frameIndex) {
                std::lock_guard regLock(m_registryMutex);
                auto view = m_registry.view<const Chunk>();
                view.each([&](const entt::entity& entity, const Chunk& chunk) {
                    if(chunk.getVoxelBuffer() == UNDEFINED_REFERENCE)
                        return;

                    std::lock_guard lock(Application::Get().getRenderSystem().getBufferMutex());
                    auto& buffer = Application::Get().getRenderSystem().getBufferInternal(chunk.getVoxelBuffer(), frameIndex);

                    cmd.bindVertexBuffers(0, *buffer.buffer, { 0 }, {});
                    cmd.draw(chunk.getVertexCount(), 1, 0, 0);
                });
            }
        });
        builder.setBackBuffer(diffuse);
    }


}