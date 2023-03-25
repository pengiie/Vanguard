#include "TerrainGenerator.h"

#include "Chunk.h"
#include "../../util/Timer.h"

namespace vanguard {
    void TerrainGenerator::setSeed(siv::PerlinNoise::seed_type seed) {
        m_seed = seed;
        m_perlinNoise = siv::PerlinNoise(seed);
    }

    void TerrainGenerator::generateChunk(const glm::ivec3& chunkPos, Chunk& chunk) {
        FTIMER();
        int worldX = chunkPos.x * CHUNK_LENGTH;
        int worldY = chunkPos.y * CHUNK_LENGTH;
        int worldZ = chunkPos.z * CHUNK_LENGTH;
        for (int x = 0; x < CHUNK_LENGTH; x++) {
            int px = worldX + x;
            for (int z = 0; z < CHUNK_LENGTH; z++) {
                int pz = worldZ + z;
                for(int y = 0; y < CHUNK_LENGTH; y++) {
                    int py = worldY + y;

                    double density = m_perlinNoise.noise3D_01(static_cast<float>(px) * 0.01f,
                                                                  static_cast<float>(py) * 0.01f,
                                                                  static_cast<float>(pz) * 0.01f);

                    double squashRateOfChange = 0.5 / TERRAIN_MAX_HEIGHT;
                    double squash = 1.0 - ((py - TERRAIN_MAX_HEIGHT) * squashRateOfChange);

                    density *= squash;

                    if (density > 0.5) {
                        chunk.setVoxel(x, y, z, { TerrainVoxelType::Grass });
                    }
                }
            }
        }
    }
}