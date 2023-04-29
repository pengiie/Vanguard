#pragma once

#include "graphics/RenderSystem.h"
#include <entt/entt.hpp>

namespace vanguard {
    class Scene {
    public:
        Scene() = default;
        virtual ~Scene() = default;

        virtual void init() = 0;
        virtual void update(float deltaTime) = 0;
        virtual CommandsInfo buildCommands() = 0;
    protected:
        entt::registry m_registry;
    };
}