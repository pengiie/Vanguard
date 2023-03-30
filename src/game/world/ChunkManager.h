#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <memory>

#include "Chunk.h"
#include "../../util/Frustum.h"

#define CHUNK_SIZE 16

namespace vanguard {
    // Create an octree data structure
    class OctreeNode {
    public:
        OctreeNode() : m_isDummy(true) {};
        explicit OctreeNode(const glm::ivec3& size, const glm::ivec3& position);
        void setParent(OctreeNode* parent);

        [[nodiscard]] Voxel getVoxel(const glm::ivec3& voxelPosition);

        [[nodiscard]] std::vector<OctreeNode*> getAllChunkNodes();
        [[nodiscard]] std::vector<OctreeNode*> getChunksInFrustum(const Frustum& frustum);

        [[nodiscard]] OctreeNode& getChunkNode(const glm::ivec3& position);
        [[nodiscard]] const glm::ivec3& getPosition() { return m_position; }
        [[nodiscard]] const glm::ivec3& getSize() { return m_size; }
    private:
        [[nodiscard]] inline bool isChunkNode() const { return m_size.x <= 1 && m_size.y <= 1 && m_size.z <= 1; }
        [[nodiscard]] static inline uint32_t getChunkIndex(const glm::ivec3& position) {
            return position.x * 4 + position.y * 2 + position.z;
        }
    public:
        ChunkData chunkData; // Chunk data
        ChunkMesh chunkMesh; // Chunk mesh
    private:
        OctreeNode* m_parent = nullptr;

        bool m_isDummy = false;
        glm::ivec3 m_size{}; // Size of <=1 means leaf node representing chunk, size of chunks contained within
        glm::ivec3 m_position{}; // Position of the octree in the world, resolved to chunk position
        std::vector<OctreeNode> m_children; // 8 children
    };

    class ChunkManager {
    public:
        void init(const glm::ivec3* cameraPosition, uint32_t loadingDistance);

        [[nodiscard]] OctreeNode& getOctree() const { return *m_rootNode; }
    public:
        static glm::ivec3 convertWorldToChunkPosition(const glm::ivec3& position);
    private:
        const glm::ivec3* m_cameraPosition = nullptr;
        glm::ivec3 m_cameraChunkPosition = glm::ivec3(0);

        std::unique_ptr<OctreeNode> m_rootNode;
    };
}