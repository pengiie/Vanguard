#pragma once

#include "PerlinNoise.hpp"
#include "Chunk.h"
#include <chrono>

namespace vanguard {
    class TerrainGenerator {
    public:
        TerrainGenerator() = default;

        void setSeed(siv::PerlinNoise::seed_type seed);
        void generateChunk(Chunk& chunk);
    private:
        siv::PerlinNoise::seed_type m_seed = std::chrono::system_clock::now().time_since_epoch().count();
        siv::PerlinNoise m_perlinNoise = siv::PerlinNoise(m_seed);
    };
}