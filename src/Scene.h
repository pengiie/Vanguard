#pragma once

#include <entt/entt.hpp>
#include "graphics/FrameGraph.h"

namespace vanguard {
    class Scene {
    public:
        Scene() = default;
        virtual ~Scene() = default;

        virtual void init() = 0;
        virtual void update(float deltaTime) = 0;
        virtual void buildFrameGraph(FrameGraphBuilder& builder) = 0;
    protected:
        entt::registry m_registry;
    };
}