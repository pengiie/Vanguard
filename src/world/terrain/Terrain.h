#pragma once

#include "glm/vec3.hpp"
#include "../../graphics/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "TerrainGenerator.h"
#include "Chunk.h"
#include "../Camera.h"

#include <future>
#include <atomic>

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
        explicit Terrain(entt::registry& registry, std::mutex& registryMutex);
        ~Terrain();

        void init(const Camera* camera);
        void update(const glm::vec3& position);
        void updateChunkThread(const Camera* camera);

        entt::entity generateChunk(const glm::ivec3& position);

        inline void setVoxel(int x, int y, int z, const TerrainVoxel& voxel) {
            const glm::vec3 pos = glm::vec3(x, y, z);
            const glm::ivec3 chunkPosition = pos / (float) CHUNK_LENGTH;
            const glm::ivec3 localPosition = { x % CHUNK_LENGTH, y % CHUNK_LENGTH, z % CHUNK_LENGTH };
            if(m_chunks.find(chunkPosition) == m_chunks.end()) {
                generateChunk(chunkPosition);
            }
            m_registry.get<Chunk>(m_chunks[chunkPosition]).setVoxel(localPosition.x, localPosition.y, localPosition.z, voxel);
        }
        [[nodiscard]] inline TerrainVoxel getVoxel(int x, int y, int z) {
            const glm::vec3 pos = glm::vec3(x, y, z);
            const glm::ivec3 chunkPosition = glm::ivec3(floor(pos.x / (float) CHUNK_LENGTH), floor(pos.y / (float) CHUNK_LENGTH), floor(pos.z / (float) CHUNK_LENGTH));
            const glm::ivec3 modPosition = { x % CHUNK_LENGTH, y % CHUNK_LENGTH, z % CHUNK_LENGTH };
            const glm::ivec3 localPosition = { modPosition.x < 0 ? modPosition.x + CHUNK_LENGTH : modPosition.x, modPosition.y < 0 ? modPosition.y + CHUNK_LENGTH : modPosition.y, modPosition.z < 0 ? modPosition.z + CHUNK_LENGTH : modPosition.z };
            if(m_chunks.find(chunkPosition) == m_chunks.end()) {
                generateMinimumChunk(chunkPosition);
            }
            m_registryMutex.lock();
            Chunk& chunk = m_registry.get<Chunk>(m_chunks.at(chunkPosition));
            bool isLoaded = chunk.isVoxelsLoaded();

            auto voxel = chunk.getVoxel(localPosition.x, localPosition.y, localPosition.z);
            m_registryMutex.unlock();

            if(!chunk.isVoxelsLoaded()) {
                chunk.generateVoxels(m_generator);
                return chunk.getVoxel(localPosition.x, localPosition.y, localPosition.z);
            }
            return voxel;
        }

        [[nodiscard]] inline BufferRef getTerrainColorPaletteBuffer() const { return m_terrainColorPaletteBuffer; }
    private:
        void findNextChunks(const Camera* camera);
        void generateMinimumChunk(const glm::ivec3& position);
    private:
        entt::registry& m_registry;
        std::mutex& m_registryMutex;

        TerrainGenerator m_generator;

        BufferRef m_terrainColorPaletteBuffer = UNDEFINED_REFERENCE;
        int m_renderDistance = 8;

        std::unordered_map<glm::ivec3, entt::entity> m_chunks;
        std::mutex m_chunksMutex;
        std::vector<entt::entity> m_chunkTasks;
        std::vector<entt::entity> m_chunksReady;
        std::thread m_chunkThread;
        std::atomic<bool> m_chunkThreadRunning = false;
    };
}