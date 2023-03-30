#pragma once

#include <future>
#include <variant>

#include "../../util/Hash.h"
#include "glm/ext.hpp"
#include "WorldGenerator.h"
#include "ChunkManager.h"
#include "../Camera.h"

namespace vanguard {
    enum class JobType {
        LOAD_CHUNK,
        MESH_CHUNK
    };
    typedef std::future<std::optional<ChunkData>> ChunkJobFuture;
    struct ChunkJob {
        glm::ivec3 position;
        JobType type;
        ChunkMesh chunkMesh;
        ChunkJobFuture future;
    };

    class ChunkLoader {
    public:
        void init(const glm::ivec3* cameraPosition);
        void update(ChunkManager& chunkManager, const Camera& camera, int renderDistance, int loadingDistance);
    private:
        void loadChunk(ChunkManager& chunkManager, const glm::ivec3& position);
        bool tryLoadChunk(ChunkManager& chunkManager, const glm::ivec3& position);

        void meshChunk(ChunkManager& chunkManager, const glm::ivec3& position);
        bool tryMeshChunk(ChunkManager& chunkManager, const glm::ivec3& position);
    private:
        const glm::ivec3* m_cameraPosition;
        WorldGenerator m_worldGenerator;

        std::unordered_map<glm::ivec3, ChunkJob> m_chunkJobs;
        std::unordered_set<glm::ivec3> m_pendingChunks;
        std::unordered_set<glm::ivec3> m_pendingLoads;
    };
}