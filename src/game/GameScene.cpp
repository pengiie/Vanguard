#include "GameScene.h"

#include "../Application.h"
#include "../graphics/FrameGraph.h"

static const std::vector<std::string> assets = {
    "shaders/gbuffer.vert.glsl",
    "shaders/gbuffer.frag.glsl",
    "shaders/skybox.vert.glsl",
    "shaders/skybox.frag.glsl",
    "shaders/colormap.comp.glsl",
    "bunnyuv.obj",
    "bunnyimg.jpg",
    "grass.jpg",
    "skybox/top.jpg",
    "skybox/bottom.jpg",
    "skybox/left.jpg",
    "skybox/right.jpg",
    "skybox/front.jpg",
    "skybox/back.jpg",
  //  "shaders/march.comp.glsl"
};

namespace vanguard {
    void GameScene::init() {
        for (const auto& asset: assets) {
            Application::Get().getAssets().load(asset);
        }

        m_camera.init();

        Application::Get().getScheduler().scheduleRepeatingTask([&] {
            uint32_t currentFrameCount = Application::Get().getRenderSystem().getFrameCount();
            uint32_t fps = currentFrameCount - m_lastFrame;
            INFO("FPS: {}", fps);
            m_lastFrame = currentFrameCount;
        }, std::chrono::milliseconds(0), std::chrono::milliseconds(1000));

        Application::Get().getAssets().finishLoading();

        auto bunny = Application::Get().getAssets().get<Mesh>("bunnyuv.obj");
        m_vb.create<Vertex>(bunny.vertices);
        m_vbc = bunny.vertices.size();

        TextureData test{
            .width = 2,
            .height = 2,
            .channels = 3,
        };
        for(int i = 0; i < 4; i++) {
            test.data.push_back(255);
            test.data.push_back(0);
            test.data.push_back(0);
        }

        m_texture.create(ASSETS.get<TextureData>("bunnyimg.jpg"));

        m_skybox.init();
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
    }

    CommandsInfo GameScene::buildCommands() {
        FrameGraphBuilder builder;

        auto cameraUniform = builder.addUniformBuffer(0, 0, &m_camera.getCameraBuffer());
        auto textureUniform = builder.addUniformSampledImage(0, 1, &m_texture);

        auto sceneImage = builder.createImage();
        auto depth = builder.createDepthStencil();

        m_skybox.addSkyboxPass(builder, sceneImage, cameraUniform);

        builder.addRenderPass(FGBRenderPassInfo{
            .vertexShaderPath = "shaders/gbuffer.vert.glsl",
            .fragmentShaderPath = "shaders/gbuffer.frag.glsl",
            .inputs = {cameraUniform,textureUniform},
            .outputs = {sceneImage,depth},
            .callback = [&](vk::CommandBuffer cmd, ResourceRef pipeline, std::unordered_map<uint32_t, FrameGraph::DescriptorSet> sets) {
                sets.at(0).bindGraphics(pipeline, cmd);
                m_vb.bind(cmd);
                cmd.draw(m_vbc, 1, 0, 0);
                INFO("Draw!");
            },
            .vertexInputData = getMeshVertexData(),
        });

        auto backbuffer = builder.createImage();
        auto sceneStorageImage = builder.addUniformStorageImage(1, 0, sceneImage);
        auto backbufferStorageImage = builder.addUniformStorageImage(1, 1, backbuffer);

        builder.addComputePass(FGBComputePassInfo{
            .computeShaderPath = "shaders/colormap.comp.glsl",
            .inputs = { sceneStorageImage },
            .outputs = { backbufferStorageImage },
            .callback = [&](vk::CommandBuffer cmd, ResourceRef pipeline, std::unordered_map<uint32_t, FrameGraph::DescriptorSet> sets) {
                sets.at(1).bindCompute(pipeline, cmd);
                cmd.dispatch(1, 1, 1);
            },
        });

        builder.setBackbuffer(backbuffer);
        m_frameGraph = builder.bake();
        return m_frameGraph.getCommands();
    }
}