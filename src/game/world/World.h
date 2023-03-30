#pragma once

#include <glm/glm.hpp>
#include "ChunkManager.h"
#include "ChunkLoader.h"

namespace vanguard {
    class World {
    public:
        void init();

        void update(const Camera& camera);

        [[nodiscard]] std::vector<ChunkMesh*> getRenderableChunks(const Camera& camera) const;
    private:
        int m_renderDistance = 14;
        int m_loadingDistance = m_renderDistance + 1;

        ChunkManager m_chunkManager;
        ChunkLoader m_chunkLoader;
    };
}