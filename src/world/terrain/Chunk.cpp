#include "Chunk.h"

#include "../../Application.h"
#include "TerrainConstants.h"

static const std::vector<glm::vec3> topFace = {
    { 0.0f, 1.0f, 0.0f },
    { 1.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 1.0f },
    { 0.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 0.0f },
    { 1.0f, 1.0f, 1.0f }
};

static const std::vector<glm::vec3> bottomFace = {
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 1.0f, 0.0f, 1.0f }
};

static const std::vector<glm::vec3> rightFace = {
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 0.0f },
    { 1.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f }
};

static const std::vector<glm::vec3> leftFace = {
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 1.0f }
};

static const std::vector<glm::vec3> forwardFace = {
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 1.0f },
    { 1.0f, 0.0f, 1.0f },
    { 1.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f }
};

static const std::vector<glm::vec3> backwardFace = {
    { 0.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 1.0f, 0.0f }
};

namespace vanguard {
    Chunk::Chunk(const glm::ivec3& position) : m_position(position) {

    }

    static uint8_t toColorIndex(TerrainVoxelType type) {
        switch(type) {
            case TerrainVoxelType::Grass:
                return static_cast<uint8_t>(TerrainColorPalette::Grass);
            case TerrainVoxelType::Dirt:
                return static_cast<uint8_t>(TerrainColorPalette::Dirt);
            case TerrainVoxelType::Stone:
                return static_cast<uint8_t>(TerrainColorPalette::Stone);
            default:
                return 0;
        }
    }

    void Chunk::generateVoxelMesh() {
        std::vector<TerrainVoxelVertex> voxelVertices;
        for (int i = 0; i < CHUNK_VOLUME; i++) {
            glm::vec3 localPosition = getLocalPosition(i) * m_position;
            TerrainVoxel voxel = m_voxels[i];
            if(voxel.type == TerrainVoxelType::Air) continue;

            for (const auto& pos: topFace) {
                voxelVertices.push_back(TerrainVoxelVertex{
                    .position = glm::vec3(localPosition.x + pos.x, localPosition.y + pos.y, localPosition.z + pos.z) * VOXEL_SIZE,
                    .direction = NormalDirection::Up,
                    .colorPaletteIndex = toColorIndex(voxel.type)
                });
            }
            for(const auto& pos: bottomFace) {
                voxelVertices.push_back(TerrainVoxelVertex{
                    .position = glm::vec3(localPosition.x + pos.x, localPosition.y + pos.y, localPosition.z + pos.z) * VOXEL_SIZE,
                    .direction = NormalDirection::Down,
                    .colorPaletteIndex = toColorIndex(voxel.type)
                });
            }
            for(const auto& pos: rightFace) {
                voxelVertices.push_back(TerrainVoxelVertex{
                    .position = glm::vec3(localPosition.x + pos.x, localPosition.y + pos.y, localPosition.z + pos.z) * VOXEL_SIZE,
                    .direction = NormalDirection::Right,
                    .colorPaletteIndex = toColorIndex(voxel.type)
                });
            }
            for(const auto& pos: leftFace) {
                voxelVertices.push_back(TerrainVoxelVertex{
                    .position = glm::vec3(localPosition.x + pos.x, localPosition.y + pos.y, localPosition.z + pos.z) * VOXEL_SIZE,
                    .direction = NormalDirection::Left,
                    .colorPaletteIndex = toColorIndex(voxel.type)
                });
            }
            for(const auto& pos: forwardFace) {
                voxelVertices.push_back(TerrainVoxelVertex{
                    .position = glm::vec3(localPosition.x + pos.x, localPosition.y + pos.y, localPosition.z + pos.z) * VOXEL_SIZE,
                    .direction = NormalDirection::Forward,
                    .colorPaletteIndex = toColorIndex(voxel.type)
                });
            }
            for(const auto& pos: backwardFace) {
                voxelVertices.push_back(TerrainVoxelVertex{
                    .position = glm::vec3(localPosition.x + pos.x, localPosition.y + pos.y, localPosition.z + pos.z) * VOXEL_SIZE,
                    .direction = NormalDirection::Backward,
                    .colorPaletteIndex = toColorIndex(voxel.type)
                });
            }
        }

        auto& renderSystem = Application::Get().getRenderSystem();
        m_voxelBuffer = renderSystem.createBuffer(voxelVertices.size() * sizeof(TerrainVoxelVertex), vk::BufferUsageFlagBits::eVertexBuffer, VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY, false);
        renderSystem.updateBuffer(m_voxelBuffer, voxelVertices.data(), voxelVertices.size() * sizeof(TerrainVoxelVertex), false);
        m_vertexCount = voxelVertices.size();
    }
}