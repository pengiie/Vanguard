#pragma once

#include "Voxel.h"
#include "TerrainConstants.h"

namespace vanguard {
    class Chunk {
    public:
        Chunk() = default;
        explicit Chunk(const glm::ivec3& position);

        Chunk(const Chunk&) = delete;
        Chunk& operator =(const Chunk&) = delete;

        Chunk(Chunk&& other)  noexcept : m_position(other.m_position), m_voxels(other.m_voxels), m_voxelBuffer(other.m_voxelBuffer), m_vertexCount(other.m_vertexCount) {
            other.m_vertexCount = 0;
            other.m_voxelBuffer = UNDEFINED_REFERENCE;
        }

        Chunk& operator =(Chunk&& other) {
            m_position = other.m_position;
            m_voxels = other.m_voxels;
            m_voxelBuffer = other.m_voxelBuffer;
            m_vertexCount = other.m_vertexCount;

            other.m_vertexCount = 0;
            other.m_voxelBuffer = UNDEFINED_REFERENCE;

            return *this;
        }

        void generateVoxelMesh();

        [[nodiscard]] inline glm::ivec3 getLocalPosition(int index) const {
            return glm::ivec3{
                index % CHUNK_LENGTH,
                (index / CHUNK_AREA) % CHUNK_LENGTH,
                index / CHUNK_AREA
            };
        }

        inline void setVoxel(int x, int y, int z, const TerrainVoxel& voxel) {
            m_voxels[x + z * CHUNK_LENGTH + y * CHUNK_AREA] = voxel;
        }
        [[nodiscard]] inline TerrainVoxel getVoxel(int x, int y, int z) const {
            return m_voxels[x + z * CHUNK_LENGTH + y * CHUNK_AREA];
        }

        [[nodiscard]] inline const glm::ivec3& getPosition() const { return m_position; }
        [[nodiscard]] inline BufferRef getVoxelBuffer() const { return m_voxelBuffer; }
        [[nodiscard]] inline uint32_t getVertexCount() const { return m_vertexCount; }
    private:
        glm::ivec3 m_position = { 0, 0, 0 };
        std::array<TerrainVoxel, CHUNK_VOLUME> m_voxels = { TerrainVoxel{
            .type = TerrainVoxelType::Air
        } };

        BufferRef m_voxelBuffer = UNDEFINED_REFERENCE;
        uint32_t m_vertexCount = 0;
    };
}