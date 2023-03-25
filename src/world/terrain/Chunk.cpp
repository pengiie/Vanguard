#include "Chunk.h"

#include "../../Application.h"
#include "TerrainConstants.h"
#include "Terrain.h"

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
    Chunk::Chunk(Terrain* terrain, const glm::ivec3& position) : m_terrain(terrain), m_position(position) {
        m_voxels.resize(CHUNK_VOLUME);
        std::fill(m_voxels.begin(), m_voxels.end(), TerrainVoxel{ .type = TerrainVoxelType::Air });
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

    void Chunk::generateVoxels(vanguard::TerrainGenerator& generator) {
        generator.generateChunk(m_position, *this);
        m_isVoxelsLoaded = true;
    }

    void Chunk::generateVoxelMesh() {
        FTIMER();
        m_vertices.clear();

        if(isEmpty())
            return;

        for (int i = 0; i < CHUNK_VOLUME; i++) {
            glm::ivec3 localPosition = toLocalPosition(i);
            glm::vec3 worldPosition = glm::vec3(localPosition) + glm::vec3(m_position * CHUNK_LENGTH);
            TerrainVoxel voxel = m_voxels[i];
            if(voxel.type == TerrainVoxelType::Air) continue;

            tryAddFace(worldPosition, localPosition.x, localPosition.y, localPosition.z, NormalDirection::Up, topFace);
            tryAddFace(worldPosition, localPosition.x, localPosition.y, localPosition.z, NormalDirection::Down, bottomFace);
            tryAddFace(worldPosition, localPosition.x, localPosition.y, localPosition.z, NormalDirection::Right, rightFace);
            tryAddFace(worldPosition, localPosition.x, localPosition.y, localPosition.z, NormalDirection::Left, leftFace);
            tryAddFace(worldPosition, localPosition.x, localPosition.y, localPosition.z, NormalDirection::Forward, forwardFace);
            tryAddFace(worldPosition, localPosition.x, localPosition.y, localPosition.z, NormalDirection::Backward, backwardFace);
        }
        m_isMeshGenerated = true;
    }

    void Chunk::tryAddFace(const glm::vec3& worldPosition, int x, int y, int z, vanguard::NormalDirection direction,
                           const std::vector<glm::vec3>& vertices) {
        if(!inBounds(x, y, z))
            return;
        int neighborX = x;
        int neighborY = y;
        int neighborZ = z;
        switch (direction) {
            case NormalDirection::Up:
                neighborY++;
                break;
            case NormalDirection::Down:
                neighborY--;
                break;
            case NormalDirection::Right:
                neighborX++;
                break;
            case NormalDirection::Left:
                neighborX--;
                break;
            case NormalDirection::Forward:
                neighborZ++;
                break;
            case NormalDirection::Backward:
                neighborZ--;
                break;
        }
        auto type = inBounds(neighborX, neighborY, neighborZ) ?
                m_voxels[toIndex(neighborX, neighborY, neighborZ)].type :
                m_terrain->getVoxel(neighborX + m_position.x * CHUNK_LENGTH, neighborY + m_position.y * CHUNK_LENGTH, neighborZ + m_position.z * CHUNK_LENGTH).type;
        if(type == TerrainVoxelType::Air) {
            for(const auto& pos: vertices) {
                m_vertices.push_back(TerrainVoxelVertex{
                        .position = glm::vec3(worldPosition.x + pos.x, worldPosition.y + pos.y, worldPosition.z + pos.z) * VOXEL_SIZE,
                        .direction = direction,
                        .colorPaletteIndex = toColorIndex(type)
                });
            }
        }
    }

    void Chunk::generateBuffer() {
        if(m_isEmpty || m_vertices.empty())
            return;
        auto& renderSystem = Application::Get().getRenderSystem();
        m_voxelBuffer = renderSystem.createBuffer(m_vertices.size() * sizeof(TerrainVoxelVertex), vk::BufferUsageFlagBits::eVertexBuffer, VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY, false);
        renderSystem.updateBuffer(m_voxelBuffer, m_vertices.data(), m_vertices.size() * sizeof(TerrainVoxelVertex), false);
        m_vertexCount = m_vertices.size();
    }
}