#include "ChunkManager.h"
#include "WorldGenerator.h"

namespace vanguard {
    void ChunkManager::init(const glm::ivec3* cameraPosition, uint32_t loadingDistance) {
        m_cameraPosition = cameraPosition;
        m_cameraChunkPosition = convertWorldToChunkPosition(*m_cameraPosition);

        auto size = glm::ivec3(static_cast<int>(loadingDistance) * 2);
        auto halfSize = size / 2;
        m_rootNode = std::make_unique<OctreeNode>(size, -halfSize);
    }

    glm::ivec3 ChunkManager::convertWorldToChunkPosition(const glm::ivec3& position) {
        return {position.x / CHUNK_SIZE, position.y / CHUNK_SIZE, position.z / CHUNK_SIZE};
    }

    OctreeNode::OctreeNode(const glm::ivec3& size, const glm::ivec3& position) : m_size(size), m_position(position) {
        if(isChunkNode()) {
            chunkData = ChunkData(m_position);
            chunkMesh = ChunkMesh(ChunkLod::CHUNK_LOD_0);
            return;
        }
        m_children.resize(8);

        glm::ivec3 lowerSize = size / 2;
        glm::ivec3 upperSize = lowerSize;
        if(size.x % 2 == 1 && size.x > 2) upperSize.x++;
        if(size.y % 2 == 1 && size.y > 2) upperSize.y++;
        if(size.z % 2 == 1 && size.z > 2) upperSize.z++;

        for(int x = 0; x <= 1; x++) {
            if(x == 1 && size.x == 1)
                continue;
            for(int y = 0; y <= 1; y++) {
                if(y == 1 && size.y == 1)
                    continue;
                for(int z = 0; z <= 1; z++) {
                    if(z == 1 && size.z == 1)
                        continue;
                    glm::ivec3 newSize = {
                        x == 0 ? lowerSize.x : upperSize.x,
                        y == 0 ? lowerSize.y : upperSize.y,
                        z == 0 ? lowerSize.z : upperSize.z
                    };

                    m_children[getChunkIndex({ x, y, z })] = OctreeNode(newSize, m_position + glm::ivec3(x * lowerSize.x, y * lowerSize.y, z * lowerSize.z));;
                }
            }
        }
        setParent(nullptr);
    }

    std::vector<OctreeNode*> OctreeNode::getAllChunkNodes() {
        if(isChunkNode())
            return { this };
        std::vector<OctreeNode*> leafNodes;
        for(auto& child : m_children) {
            if(child.m_isDummy)
                continue;
            auto childChunks = child.getAllChunkNodes();
            leafNodes.insert(leafNodes.end(), childChunks.begin(), childChunks.end());
        }
        return leafNodes;
    }

    std::vector<OctreeNode*> OctreeNode::getChunksInFrustum(const Frustum& frustum) {
        if(isChunkNode())
            return { this };
        std::vector<OctreeNode*> leafNodes;
        for(auto& child : m_children) {
            if(child.m_isDummy)
                continue;
            auto pos = child.m_position * CHUNK_SIZE;
            if(frustum.isBounded(AABB(pos, pos + (child.m_size * CHUNK_SIZE)))) {
                auto childChunks = child.getChunksInFrustum(frustum);
                leafNodes.insert(leafNodes.end(), childChunks.begin(), childChunks.end());
            }
        }
        return leafNodes;
    }

    OctreeNode& OctreeNode::getChunkNode(const glm::ivec3& position) {
        if(isChunkNode() && position == m_position)
            return *this;
        // Binary search but in 3D

        // If the position is outside the bounds of this octree, search the parent node.
        if(position.x < m_position.x || position.x >= m_position.x + m_size.x ||
            position.y < m_position.y || position.y >= m_position.y + m_size.y ||
            position.z < m_position.z || position.z >= m_position.z + m_size.z) {
            if(m_parent == nullptr)
                throw std::runtime_error("OctreeNode::getChunkNode: Position is outside the bounds of the octree.");
            return m_parent->getChunkNode(position);
        }

        glm::ivec3 distance = position - m_position;
        glm::ivec3 halfSize = m_size / 2;
        int index = 0;
        if(distance.z >= halfSize.z && distance.z != 0)
            index += 1;
        if(distance.y >= halfSize.y && distance.y != 0)
            index += 2;
        if(distance.x >= halfSize.x && distance.x != 0)
            index += 4;
        return m_children[index].getChunkNode(position);
    }

    Voxel OctreeNode::getVoxel(const glm::ivec3& worldPosition) {
        glm::ivec3 chunkPosition = glm::ivec3{ floor((float) worldPosition.x / CHUNK_SIZE), floor((float) worldPosition.y / CHUNK_SIZE), floor((float) worldPosition.z / CHUNK_SIZE) };

        auto& node = getChunkNode(chunkPosition);

        glm::ivec3 localPosition = worldPosition - (node.getPosition() * CHUNK_SIZE);
        return node.chunkData.getVoxel(localPosition);
    }

    void OctreeNode::setParent(OctreeNode* parent) {
        m_parent = parent;
        for(auto& child : m_children) {
            child.setParent(this);
        }
    }
}