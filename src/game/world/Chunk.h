#pragma once

#include "glm/ext.hpp"
#include "Voxel.h"
#include "../../graphics/ResourceManager.h"
#include <vector>

#define CHUNK_LENGTH 16
#define CHUNK_AREA (CHUNK_LENGTH * CHUNK_LENGTH)
#define CHUNK_VOLUME (CHUNK_LENGTH * CHUNK_LENGTH * CHUNK_LENGTH)

namespace vanguard {
    struct WorldGenerator;
    struct OctreeNode;

    enum class ChunkLod {
        CHUNK_LOD_0, // 1:1 - Standard chunk
        CHUNK_LOD_1, // 1:2 - Half detail
        CHUNK_LOD_2, // 1:4 - Quarter detail
        CHUNK_LOD_3, // 1:8 - Eighth detail
    };
    enum class ChunkVoxelState {
        Empty,
        Solid,
        Mixed
    };

    class ChunkMesh {
    public:
        ChunkMesh() = default;
        explicit ChunkMesh(ChunkLod lod) : m_lod(lod) {}

        void generateMesh(OctreeNode& node);
        // Method must be called on same thread as Vulkan context
        void generateBuffer();
        void render(const vk::CommandBuffer& cmd) const;

        [[nodiscard]] inline const VertexBuffer& getVertexBuffer() const { return m_vertexBuffer; }
    private:
        void tryAddFace(OctreeNode& node, const glm::ivec3& position, const glm::ivec3& checkPosition, const std::vector<glm::vec3>& face, float shading);
    public:
        bool isLoaded = false;
    private:
        ChunkLod m_lod = ChunkLod::CHUNK_LOD_0;
        VertexBuffer m_vertexBuffer;
        std::vector<VoxelVertex> m_vertices;
    };

    class ChunkData {
    public:
        ChunkData() = default;
        explicit ChunkData(const glm::ivec3& position) : m_position(position) {}

        void load(WorldGenerator& generator);

        [[nodiscard]] Voxel getVoxel(const glm::ivec3& position) const {
            #ifdef VANGUARD_DEBUG
                if(!inLocalBounds(position)) return false;
            #endif
            return m_voxels.get()[getVoxelIndex(position)];
        }
        inline void setVoxel(const glm::ivec3& position, Voxel voxel) {
            #ifdef VANGUARD_DEBUG
                if(!inLocalBounds(position)) return;
            #endif
            m_voxels.get()[getVoxelIndex(position)] = voxel;
        }

        [[nodiscard]] const glm::ivec3& getPosition() const { return m_position; }
        [[nodiscard]] ChunkVoxelState getVoxelState() const { return m_voxelState; }
    private:
        [[nodiscard]] static bool inLocalBounds(const glm::ivec3& position) {
            return position.x >= 0 && position.x < CHUNK_LENGTH &&
                position.y >= 0 && position.y < CHUNK_LENGTH &&
                position.z >= 0 && position.z < CHUNK_LENGTH;
        }
        [[nodiscard]] static uint32_t getVoxelIndex(const glm::ivec3& position) {
            return position.x + position.y * CHUNK_LENGTH + position.z * CHUNK_AREA;
        }
    public:
        bool isLoaded = false;
    private:
        glm::ivec3 m_position{};
        ChunkVoxelState m_voxelState = ChunkVoxelState::Empty;

        std::unique_ptr<Voxel[]> m_voxels;
    };
}