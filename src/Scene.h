#pragma once

#include <entt/entt.hpp>
#include "graphics/RenderGraph.h"

namespace vanguard {
    class Scene {
    public:
        Scene() = default;
        virtual ~Scene() = default;

        virtual void init() = 0;
        virtual void update(float deltaTime) = 0;
        virtual void buildRenderGraph(RenderGraphBuilder& builder) = 0;
    protected:
        entt::registry m_registry;
    };
}