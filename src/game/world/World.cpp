#include "World.h"
#include "../../Logger.h"

namespace vanguard {

    void World::init() {
        glm::ivec3 chunkPos = glm::ivec3(0, 0, 0);
        m_chunkManager.init(&chunkPos, m_loadingDistance);

        m_chunkLoader.init(&chunkPos);
    }

    void World::update(const Camera& camera) {
        m_chunkLoader.update(m_chunkManager, camera, m_renderDistance, m_loadingDistance);
    }

    std::vector<ChunkMesh*> World::getRenderableChunks(const Camera& camera) const {
        std::vector<ChunkMesh*> renderableChunks;
        for (const auto& chunk : m_chunkManager.getOctree().getChunksInFrustum(camera.getFrustum())) {
            if(!chunk->chunkMesh.getVertexBuffer().isLoaded())
                continue;
            if(chunk->getPosition().x > m_renderDistance || chunk->getPosition().x < -m_renderDistance)
                continue;
            if(chunk->getPosition().y > m_renderDistance || chunk->getPosition().y < -m_renderDistance)
                continue;
            if(chunk->getPosition().z > m_renderDistance || chunk->getPosition().z < -m_renderDistance)
                continue;
            renderableChunks.push_back(&chunk->chunkMesh);
        }
        INFO("Renderable chunks: {}", renderableChunks.size());
        return renderableChunks;
    }
}