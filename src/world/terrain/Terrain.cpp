#include "Terrain.h"
#include "../../Application.h"
#include "Voxel.h"

namespace vanguard {
    Terrain::Terrain(entt::registry& registry) : m_registry(registry) {}

    void Terrain::init() {
        FTIMER();
        auto& renderSystem = Application::Get().getRenderSystem();

        m_terrainColorPaletteBuffer = renderSystem.createUniformBuffer(sizeof(glm::vec3) * 256);
        renderSystem.updateBuffer(m_terrainColorPaletteBuffer, color::terrainPalette.data(), sizeof(glm::vec3) * 256, false);

//        const int radius = 1;
//        for(int x = -radius; x <= radius; x++) {
//            for(int z = -radius; z <= radius; z++) {
//                generateChunk({ x, 0, z });
//            }
//        }
        generateChunk({ 0, 0, 0 });
    }

    void Terrain::generateChunk(const glm::ivec3& position) {
        if(m_chunks.find(position) != m_chunks.end()) {
            return;
        }

        Chunk chunk(position);
        m_generator.generateChunk(chunk);
        chunk.generateVoxelMesh();

        auto chunkEntity = m_registry.create();
        m_chunks[position] = chunkEntity;
        m_registry.emplace<Chunk>(chunkEntity, std::move(chunk));
    }
}