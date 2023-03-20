#include "TerrainGenerator.h"

namespace vanguard {
    void TerrainGenerator::setSeed(siv::PerlinNoise::seed_type seed) {
        m_seed = seed;
        m_perlinNoise = siv::PerlinNoise(seed);
    }

    void TerrainGenerator::generateChunk(Chunk& chunk) {
        for (int x = 0; x < CHUNK_LENGTH; x++) {
            for (int z = 0; z < CHUNK_LENGTH; z++) {
                auto noise = static_cast<float>(m_perlinNoise.octave2D_01(x, z, 8));
                for(int y = 0; y < CHUNK_LENGTH; y++) {
                    if (static_cast<float>(y) < noise*CHUNK_LENGTH) {
                        chunk.setVoxel(x, y, z, { TerrainVoxelType::Grass });
                    }
                }
            }
        }
    }
}