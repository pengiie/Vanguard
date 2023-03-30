#include "Chunk.h"

#include "WorldGenerator.h"
#include "ChunkManager.h"
#include "Direction.h"
#include "../../Logger.h"

static const std::vector<glm::vec3> s_topFace = {
    {0.0f, 1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f},
    {1.0f, 1.0f, 1.0f}
};

static const std::vector<glm::vec3> s_bottomFace = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 1.0f}
};

static const std::vector<glm::vec3> s_leftFace = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 1.0f}
};

static const std::vector<glm::vec3> s_rightFace = {
    {1.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},
    {1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f}
};

static const std::vector<glm::vec3> s_frontFace = {
    {0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 1.0f},
    {1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f}
};

static const std::vector<glm::vec3> s_backFace = {
    {0.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.0f}
};

namespace vanguard {

    void ChunkData::load(WorldGenerator& generator) {
        m_voxels = std::make_unique<Voxel[]>(CHUNK_VOLUME);
        for(int i = 0; i < CHUNK_VOLUME; i++) {
            m_voxels.get()[i] = false;
        }
        m_voxelState = generator.generateChunk(*this);
        isLoaded = true;
    }

    void ChunkMesh::tryAddFace(OctreeNode& node, const glm::ivec3& position, const glm::ivec3& checkPosition,
                               const std::vector<glm::vec3>& face, float shading) {
        Voxel checkVoxel = node.getVoxel(checkPosition);
        if(!checkVoxel) {
            for (int i = 0; i < 6; i++) {
                m_vertices.push_back(VoxelVertex{
                    .position = (glm::vec3(position) + face[i]) * VOXEL_SIZE,
                    .shading = shading
                });
            }
        }
    }

    static bool hasSolidNeighbors(OctreeNode& node, const glm::ivec3& position) {
        Direction direction(position);
        for(const auto& neighborPosition: direction)
            if(node.getChunkNode(neighborPosition).chunkData.getVoxelState() != ChunkVoxelState::Solid)
                return false;
        return true;
    }

    void ChunkMesh::generateMesh(OctreeNode& node) {
        m_vertices.clear();
        INFO("Generating mesh for chunk at ({}, {}, {})", node.getPosition().x, node.getPosition().y, node.getPosition().z);
        if(m_lod == ChunkLod::CHUNK_LOD_0) {
            ChunkData& chunkData = node.chunkData; // Since LOD is 0, we can assume that the chunk is a leaf node

            // Determine if the chunk is fully empty or solid with solid neighbors to skip meshing.
            if(chunkData.getVoxelState() == ChunkVoxelState::Empty) {
                isLoaded = true;
                return;
            }
            if(chunkData.getVoxelState() == ChunkVoxelState::Solid && hasSolidNeighbors(node, node.getPosition())) {
                isLoaded = true;
                return;
            }

            for (int x = 0; x < CHUNK_LENGTH; x++) {
                for (int z = 0; z < CHUNK_LENGTH; z++) {
                    for (int y = 0; y < CHUNK_LENGTH; y++) {
                        glm::ivec3 pos = glm::ivec3(x, y, z);

                        if(!chunkData.getVoxel(pos))
                            continue;


                        glm::ivec3 worldPos = pos + node.getPosition() * CHUNK_LENGTH;
                        Direction direction(worldPos);
                        tryAddFace(node, worldPos, direction.up(), s_topFace, 1.0f);
                        tryAddFace(node, worldPos, direction.down(), s_bottomFace, 0.25f);
                        tryAddFace(node, worldPos, direction.left(), s_leftFace, 0.5f);
                        tryAddFace(node, worldPos, direction.right(), s_rightFace, 0.5f);
                        tryAddFace(node, worldPos, direction.front(), s_frontFace, 0.75f);
                        tryAddFace(node, worldPos, direction.back(), s_backFace, 0.75f);
                    }
                }
            }
        }
        isLoaded = true;
    }

    void ChunkMesh::generateBuffer() {
        if(m_vertices.empty())
            return;

        size_t size = sizeof(VoxelVertex) * m_vertices.size();
        m_vertexBuffer.create(size);
        m_vertexBuffer.update(m_vertices.data(), size);
    }

    void ChunkMesh::render(const vk::CommandBuffer& cmd) const {
        if(!m_vertexBuffer.isLoaded())
            return;
        m_vertexBuffer.bind(cmd);
        cmd.draw(m_vertices.size(), 1, 0, 0);
    }
}