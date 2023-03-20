#pragma once

#include "glm/vec3.hpp"
#include "../../graphics/RenderGraph.h"

namespace vanguard {
    enum class NormalDirection : uint32_t {
        Up = 0,
        Down = 1,
        Right = 2,
        Left = 3,
        Forward = 4,
        Backward = 5
    };

    enum class TerrainColorPalette : uint32_t {
        Grass = 0,
        Dirt = 1,
        Stone = 2
    };


    namespace color {
        static const std::vector<glm::vec3> terrainPalette = {
                { 0.0f, 1.0f, 0.0f }, // Grass
                { 0.5f, 0.35f, 0.05f }, // Dirt
                { 0.5f, 0.5f, 0.5f } // Stone
        };
    }

    typedef uint8_t ColorPaletteIndex;
    struct TerrainVoxelVertex {
        glm::vec3 position;
        NormalDirection direction;
        ColorPaletteIndex colorPaletteIndex;

        static VertexInputData getVertexInputData();
    };

    enum class TerrainVoxelType {
        Air,
        Grass,
        Dirt,
        Stone
    };

    struct TerrainVoxel {
        TerrainVoxelType type;
    };
}