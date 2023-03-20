#pragma once

#include "glm/vec3.hpp"
#include "../../graphics/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "TerrainGenerator.h"

template <>
struct std::hash<glm::ivec3> {
    std::size_t operator()(const glm::ivec3& key) const {
        return ((std::hash<int>()(key.x)
                 ^ (std::hash<int>()(key.y) << 1)) >> 1)
               ^ (std::hash<int>()(key.z) << 1);
    }
};

namespace vanguard {
    class Terrain {
    public:
        explicit Terrain(entt::registry& registry);

        void init();

        void generateChunk(const glm::ivec3& position);

        [[nodiscard]] inline BufferRef getTerrainColorPaletteBuffer() const { return m_terrainColorPaletteBuffer; }
    private:
        entt::registry& m_registry;
        TerrainGenerator m_generator;

        BufferRef m_terrainColorPaletteBuffer = UNDEFINED_REFERENCE;

        std::unordered_map<glm::ivec3, entt::entity> m_chunks;
    };
}