#pragma once

#include "PerlinNoise.hpp"
#include "glm/fwd.hpp"
#include <chrono>

namespace vanguard {
    struct Chunk;
    class TerrainGenerator {
    public:
        TerrainGenerator() = default;

        void setSeed(siv::PerlinNoise::seed_type seed);
        void generateChunk(const glm::ivec3& chunkPos, Chunk& chunk);
    private:
        siv::PerlinNoise::seed_type m_seed = std::chrono::system_clock::now().time_since_epoch().count();
        siv::PerlinNoise m_perlinNoise = siv::PerlinNoise(m_seed);
    };
}