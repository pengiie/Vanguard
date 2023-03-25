#pragma once

#include "../Scene.h"
#include "Camera.h"
#include "../assets/Assets.h"
#include "terrain/Terrain.h"

#include <mutex>

namespace vanguard {
    class WorldScene : public Scene {
    public:
        WorldScene() = default;

        void init() override;
        void update(float deltaTime) override;
        void buildRenderGraph(RenderGraphBuilder& builder) override;
    private:
        static void loadAssets(Assets& assets);
    private:
        std::mutex m_registryMutex;

        Camera m_camera{};

        Terrain m_terrain = Terrain(m_registry, m_registryMutex);

        uint32_t m_lastFrame = 0;
    };
}