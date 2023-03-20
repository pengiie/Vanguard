#pragma once

#include "../Scene.h"
#include "Camera.h"
#include "../assets/Assets.h"
#include "terrain/Terrain.h"

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
        Camera m_camera{};

        Terrain m_terrain = Terrain(m_registry);
    };
}