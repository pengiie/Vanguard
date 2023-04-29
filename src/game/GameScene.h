#pragma once

#include "../Scene.h"
#include "Camera.h"
#include "../assets/Assets.h"
#include "../graphics/FrameGraph.h"
#include "../assets/Mesh.h"
#include "Skybox.h"

#include <mutex>

namespace vanguard {
    class GameScene : public Scene {
    public:
        GameScene() = default;

        void init() override;
        void update(float deltaTime) override;
        CommandsInfo buildCommands() override;
    private:
        FrameGraph m_frameGraph;
        Camera m_camera{};

        VertexBuffer m_vb{};
        uint32_t m_vbc = 0;
        Texture2D m_texture{};

        Skybox m_skybox{};

        uint32_t m_lastFrame = 0;
    };
}