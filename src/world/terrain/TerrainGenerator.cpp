#include "TerrainGenerator.h"

namespace vanguard {
    void TerrainGenerator::setSeed(siv::PerlinNoise::seed_type seed) {
        m_seed = seed;
        m_perlinNoise = siv::PerlinNoise(seed);
    }

    void TerrainGenerator::generateChunk(Chunk& chunk) {
        for (int x = 0; x < CHUNK_LENGTH; x++) {
            for (int z = 0; z < CHUNK_LENGTH; z++) {
                auto noise = static_cast<float>(m_perlinNoise.noise2D(x, z));
                for(int y = 0; y < CHUNK_LENGTH; y++) {
                    if (static_cast<float>(y) < TERRAIN_MAX_HEIGHT * noise) {
                        chunk.setVoxel(x, y, z, { TerrainVoxelType::Grass });
                    }
                }
            }
        }
    }
}