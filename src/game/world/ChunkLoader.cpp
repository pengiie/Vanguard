#include "ChunkLoader.h"
#include "../../Logger.h"
#include "../../util/Timer.h"
#include "../../Application.h"
#include "Direction.h"

#include <glm/ext.hpp>

namespace vanguard {

    void ChunkLoader::init(const glm::ivec3* cameraPosition) {
        m_cameraPosition = cameraPosition;

    }

    void ChunkLoader::loadChunk(ChunkManager& chunkManager, const glm::ivec3& position) {
        auto [job, _] = m_chunkJobs.emplace(position, ChunkJob{
                .position = position,
                .type = JobType::LOAD_CHUNK
        });
        job->second.future = std::move(std::async(std::launch::async, [this, position = glm::ivec3(position)]() {
            ChunkData chunkData(position);
            chunkData.load(m_worldGenerator);
            return std::make_optional(std::move(chunkData));
        }));
    }

    bool ChunkLoader::tryLoadChunk(ChunkManager& chunkManager, const glm::ivec3& position) {
        auto& node = chunkManager.getOctree().getChunkNode(position);
        if(node.chunkData.isLoaded)
            return false;

        loadChunk(chunkManager, position);
        return true;
    }

    void ChunkLoader::meshChunk(ChunkManager& chunkManager, const glm::ivec3& position) {
        auto* chunkNode = &chunkManager.getOctree().getChunkNode(position);

        auto [job, _] = m_chunkJobs.emplace(position, ChunkJob{
                .position = position,
                .type = JobType::MESH_CHUNK,
                .chunkMesh = chunkNode->chunkMesh
        });
        job->second.future = std::move(std::async(std::launch::async, [chunkNode]() {
            chunkNode->chunkMesh.generateMesh(*chunkNode);
            return std::optional<ChunkData>(std::nullopt);
        }));
    }

    bool ChunkLoader::tryMeshChunk(ChunkManager& chunkManager, const glm::ivec3& position) {
        auto* chunkNode = &chunkManager.getOctree().getChunkNode(position);
        if(!chunkNode->chunkData.isLoaded)
            return false;
        if(chunkNode->chunkMesh.isLoaded)
            return false;

        bool allNeighboursLoaded = true;
        Direction direction(position);
        for(const auto& neighborPosition: direction) {
            if(!chunkManager.getOctree().getChunkNode(neighborPosition).chunkData.isLoaded) {
                allNeighboursLoaded = false;
                if(!m_chunkJobs.contains(neighborPosition))
                    tryLoadChunk(chunkManager, neighborPosition);
            }
        }
        if(!allNeighboursLoaded)
            return false;

        meshChunk(chunkManager, position);
        return true;
    }

    void ChunkLoader::update(ChunkManager& chunkManager, const Camera& camera, int renderDistance, int loadingDistance) {
        FTIMER();
        auto chunks = chunkManager.getOctree().getChunksInFrustum(camera.getFrustum());

        auto cameraChunkPosition = glm::floor(camera.getPosition() / (float) CHUNK_SIZE);

        // Sort chunks by distance to camera
        std::sort(chunks.begin(), chunks.end(), [this, cameraChunkPosition](const auto& a, const auto& b) {
            return glm::distance(glm::vec3(a->getPosition()), cameraChunkPosition) < glm::distance(glm::vec3(b->getPosition()), cameraChunkPosition);
        });

        uint32_t loadCount = 0;
        for(auto& chunk: chunks) {
            auto& pos = chunk->getPosition();
            auto distance = pos - glm::ivec3(0);
            if((distance.x < -renderDistance || distance.x >= renderDistance) ||
                    (distance.y < -renderDistance || distance.y >= renderDistance) ||
                    (distance.z < -renderDistance || distance.z >= renderDistance))
                continue;

            if(!m_chunkJobs.contains(pos)) {
                if(tryLoadChunk(chunkManager, pos))
                    loadCount++;
                if(loadCount >= 32)
                    break;
            }
        }
        std::vector<glm::ivec3> chunksToDequeMeshing;
        for(const auto& position: m_pendingLoads) {
            if(m_chunkJobs.contains(position))
                continue;
            bool meshing = tryMeshChunk(chunkManager, position);
            if(meshing)
                chunksToDequeMeshing.push_back(position);
        }
        for(const auto& position: chunksToDequeMeshing) {
            m_pendingLoads.erase(position);
        }

        uint32_t jobsProcessed = 0;
        std::vector<glm::ivec3> toRemove;
        for (auto& [position, job] : m_chunkJobs) {
            if (job.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto& node = chunkManager.getOctree().getChunkNode(position);
                if(job.type == JobType::LOAD_CHUNK) {
                    node.chunkData = std::move(*job.future.get());

                    auto distance = position - glm::ivec3(0);
                    if(!((distance.x < -renderDistance || distance.x >= renderDistance) ||
                        (distance.y < -renderDistance || distance.y >= renderDistance) ||
                        (distance.z < -renderDistance || distance.z >= renderDistance)))
                        m_pendingLoads.insert(position);
                }
                if(job.type == JobType::MESH_CHUNK) {
                    m_pendingChunks.insert(position);
                }
                toRemove.push_back(position);

                jobsProcessed++;
                if(jobsProcessed >= 128)
                    break;
            }
        }

        for (auto& position : toRemove) {
            m_chunkJobs.erase(position);
        }

        for (int i = 0; i < 32; i++) {
            if(m_pendingChunks.empty())
                break;

            auto position = *m_pendingChunks.begin();
            auto& node = chunkManager.getOctree().getChunkNode(position);
            node.chunkMesh.generateBuffer();

            m_pendingChunks.erase(position);
        }
    }
}