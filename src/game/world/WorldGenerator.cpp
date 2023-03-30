#include "WorldGenerator.h"

#include "../../util/Hash.h"
#include "../../Logger.h"

namespace vanguard {

    ChunkVoxelState WorldGenerator::generateChunk(ChunkData& chunk) {
        uint32_t count = 0;

        auto chunkWorldPos = chunk.getPosition() * CHUNK_LENGTH;
        for (int x = 0; x < CHUNK_LENGTH; x++) {
            for (int z = 0; z < CHUNK_LENGTH; z++) {
                for(int y = 0; y < CHUNK_LENGTH; y++) {
                    glm::ivec3 pos = glm::ivec3(x, y, z);
                    glm::ivec3 worldPos = chunkWorldPos + pos;

//                    double density = m_noise.noise3D_01(worldPos.x * 0.1, worldPos.y * 0.0125, worldPos.z * 0.1);
//                    if(density > 0.2 && density < 0.8) {
//                        chunk.setVoxel(pos, true);
//                    }
                    double height = m_noise.noise2D_01(worldPos.x * 0.1, worldPos.z * 0.1);
                    if(worldPos.y < height * 32) {
                        chunk.setVoxel(pos, true);
                        count++;
                    }
                }
            }
        }
        return ChunkVoxelState::Mixed;

        if(count == 0) {
            return ChunkVoxelState::Empty;
        } else if(count == CHUNK_VOLUME) {
            return ChunkVoxelState::Solid;
        } else {
            return ChunkVoxelState::Mixed;
        }
    }
}