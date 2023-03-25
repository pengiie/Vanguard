#pragma once

#include "Voxel.h"
#include "TerrainConstants.h"
#include "TerrainGenerator.h"

namespace vanguard {
    class Terrain;
    class Chunk {
    public:
        explicit Chunk(Terrain* terrain, const glm::ivec3& position);

        Chunk(const Chunk&) = delete;
        Chunk& operator =(const Chunk&) = delete;

        Chunk(Chunk&& other)  noexcept :
            m_position(other.m_position), m_voxels(other.m_voxels), m_voxelBuffer(other.m_voxelBuffer),
            m_vertices(std::move(other.m_vertices)), m_vertexCount(other.m_vertexCount), m_isEmpty(other.m_isEmpty), m_terrain(other.m_terrain) {
            other.m_vertexCount = 0;
            other.m_voxelBuffer = UNDEFINED_REFERENCE;
        }

        Chunk& operator =(Chunk&& other)  noexcept {
            m_terrain = other.m_terrain;
            m_position = other.m_position;
            m_voxels = other.m_voxels;
            m_voxelBuffer = other.m_voxelBuffer;
            m_vertices = std::move(other.m_vertices);
            m_vertexCount = other.m_vertexCount;
            m_isEmpty = other.m_isEmpty;

            other.m_vertexCount = 0;
            other.m_voxelBuffer = UNDEFINED_REFERENCE;

            return *this;
        }

        void generateVoxels(TerrainGenerator& generator);
        void generateVoxelMesh();
        void generateBuffer();

        inline void setVoxel(int x, int y, int z, const TerrainVoxel& voxel) {
            if(!inBounds(x, y, z))
                throw std::runtime_error("Chunk out of bounds");
            m_voxels[toIndex(x, y, z)] = voxel;
            if(voxel.type != TerrainVoxelType::Air)
                m_isEmpty = false;
        }
        [[nodiscard]] inline TerrainVoxel getVoxel(int x, int y, int z) const {
            if(!inBounds(x, y, z))
                throw std::runtime_error("Chunk out of bounds");
            return m_voxels[toIndex(x, y, z)];
        }

        [[nodiscard]] inline const glm::ivec3& getPosition() const { return m_position; }
        [[nodiscard]] inline BufferRef getVoxelBuffer() const { return m_voxelBuffer; }
        [[nodiscard]] inline uint32_t getVertexCount() const { return m_vertexCount; }
        [[nodiscard]] inline bool isEmpty() const { return m_isEmpty; }
        [[nodiscard]] inline bool isSolid() const { return m_isSolid; }
        [[nodiscard]] inline bool isVoxelsLoaded() const { return m_isVoxelsLoaded; }
        [[nodiscard]] inline bool isMeshGenerated() const { return m_isMeshGenerated; }
    private:
        static inline bool inBounds(int x, int y, int z) {
            if(x < 0 || x >= CHUNK_LENGTH) return false;
            if(y < 0 || y >= CHUNK_LENGTH) return false;
            if(z < 0 || z >= CHUNK_LENGTH) return false;
            return true;
        }
        static inline size_t toIndex(int x, int y, int z) {
            return x + z * CHUNK_LENGTH + y * CHUNK_AREA;
        }
        [[nodiscard]] static inline glm::ivec3 toLocalPosition(int index) {
            return glm::ivec3{
                    index % CHUNK_LENGTH,
                    index / CHUNK_AREA,
                    (index / CHUNK_LENGTH) % CHUNK_LENGTH,
            };
        }

        void tryAddFace(const glm::vec3& worldPosition, int x, int y, int z, NormalDirection direction, const std::vector<glm::vec3>& vertices);
    private:
        Terrain* m_terrain;

        glm::ivec3 m_position = { 0, 0, 0 };
        std::vector<TerrainVoxel> m_voxels;

        bool m_isEmpty = true;
        bool m_isSolid = false;

        bool m_isVoxelsLoaded = false;
        bool m_isMeshGenerated = false;

        std::vector<TerrainVoxelVertex> m_vertices;
        BufferRef m_voxelBuffer = UNDEFINED_REFERENCE;
        uint32_t m_vertexCount = 0;
    };
}