#include "Voxel.h"

namespace vanguard {
    VertexInputData TerrainVoxelVertex::getVertexInputData() {
        VertexInputData data = VertexInputData::createVertexInputData<TerrainVoxelVertex>();
        data.setAttribute(0, offsetof(TerrainVoxelVertex, TerrainVoxelVertex::position), vk::Format::eR32G32B32Sfloat);
        data.setAttribute(1, offsetof(TerrainVoxelVertex, TerrainVoxelVertex::direction), vk::Format::eR8Uint);
        data.setAttribute(2, offsetof(TerrainVoxelVertex, TerrainVoxelVertex::colorPaletteIndex), vk::Format::eR8Uint);
        return data;
    }
}