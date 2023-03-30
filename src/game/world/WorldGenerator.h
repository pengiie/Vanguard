#pragma once

#include <PerlinNoise.hpp>

#include "Chunk.h"

namespace vanguard {
    class WorldGenerator {
    public:
        ChunkVoxelState generateChunk(ChunkData& chunk);
    private:
        siv::PerlinNoise m_noise;
    };
}