#include "Terrain.h"
#include "../../Application.h"
#include "Voxel.h"

#include <glm/gtx/transform.hpp>
#include <future>
#include <glm/gtx/vector_angle.hpp>
#include <deque>

namespace vanguard {
    Terrain::Terrain(entt::registry& registry, std::mutex& registryMutex) : m_registry(registry), m_registryMutex(registryMutex) {}

    Terrain::~Terrain() {
        m_chunkThreadRunning = false;
        m_chunkTasks.clear();
        m_chunkThread.join();
    }

    void Terrain::init(const Camera* camera) {
        FTIMER();
        auto& renderSystem = Application::Get().getRenderSystem();

        m_terrainColorPaletteBuffer = renderSystem.createUniformBuffer(sizeof(glm::vec3) * 256);
        renderSystem.updateBuffer(m_terrainColorPaletteBuffer, color::terrainPalette.data(), sizeof(glm::vec3) * 256, false);

        m_chunkThreadRunning = true;
        m_chunkThread = std::thread([this, camera]() {
            while(m_chunkThreadRunning) {
                updateChunkThread(camera);
            }
        });
    }

    void Terrain::update(const glm::vec3& position) {
        FTIMER();
        for (int i = 0; i < CHUNK_LOAD_BATCH; i++) {
            if(!m_chunksReady.empty()) {
                m_registryMutex.lock();
                entt::entity chunkEntity = m_chunksReady.front();
                Chunk& chunk = m_registry.get<Chunk>(chunkEntity);
                INFO("Chunk ready at ({}, {}, {})", chunk.getPosition().x, chunk.getPosition().y, chunk.getPosition().z);
                if(!chunk.isEmpty()) {
                    chunk.generateBuffer();
                }
                m_chunksReady.erase(m_chunksReady.begin());
                m_registryMutex.unlock();
            }
        }
    }

    entt::entity Terrain::generateChunk(const glm::ivec3& position) {
        m_registryMutex.lock();
        entt::entity chunkEntity = m_registry.create();

        m_registry.emplace<Chunk>(chunkEntity, Chunk(this, position));
        m_registryMutex.unlock();

        m_chunkTasks.push_back(chunkEntity);
        return chunkEntity;
    }

    void Terrain::updateChunkThread(const Camera* camera) {
        TIMER("Terrain::updateChunkThread");

        static glm::vec3 lastCameraPos = camera->getPosition();
        if(glm::length(camera->getPosition() - lastCameraPos) > CHUNK_WORLD_LENGTH || m_chunkTasks.empty()) {
            m_chunkTasks.clear();
            findNextChunks(camera);
        }

        if(!m_chunkTasks.empty()) {
            entt::entity chunkEntity = m_chunkTasks.front();

            Chunk& chunk = m_registry.get<Chunk>(chunkEntity);

            INFO("Generating chunk at ({}, {}, {})", chunk.getPosition().x, chunk.getPosition().y, chunk.getPosition().z);

            if(!chunk.isVoxelsLoaded())
                chunk.generateVoxels(m_generator);
            if(!chunk.isEmpty()) {
                chunk.generateVoxelMesh();
                m_chunksReady.push_back(chunkEntity);
            }

            m_chunkTasks.erase(m_chunkTasks.begin());
        }
    }

    void Terrain::findNextChunks(const Camera* camera) {
        FTIMER();

        int startX = static_cast<int>(std::floor(camera->getPosition().x / CHUNK_WORLD_LENGTH));
        int startY = static_cast<int>(std::floor(camera->getPosition().y / CHUNK_WORLD_LENGTH));
        int startZ = static_cast<int>(std::floor(camera->getPosition().z / CHUNK_WORLD_LENGTH));
        glm::ivec3 startPos = glm::ivec3(startX, startY, startZ);

        std::deque<glm::ivec3> chunksToCheck;
        std::unordered_set<glm::ivec3> chunksChecked;

        chunksToCheck.push_back(startPos);

        for(int distance = 0; distance < m_renderDistance; distance++) {
            for(int x = -distance; x <= distance; x++) {
                for(int y = -distance/2; y <= distance/2; y++) {
                    for(int z = -distance; z <= distance; z++) {
                        if(x == -distance || x == distance || y == -distance || y == distance || z == -distance || z == distance) {
                            glm::ivec3 pos = startPos + glm::ivec3(x, y, z);
                            if(chunksChecked.find(pos) == chunksChecked.end()) {
                                chunksToCheck.push_back(pos);
                                chunksChecked.insert(pos);
                            }
                        }
                    }
                }
            }
        }

        while(!chunksToCheck.empty()) {
            glm::ivec3 chunkPos = chunksToCheck.front();
            chunksToCheck.pop_front();

            if(m_chunks.find(chunkPos) == m_chunks.end())
                m_chunks[chunkPos] = generateChunk(chunkPos);

            auto& chunk = m_registry.get<Chunk>(m_chunks[chunkPos]);
            if(!chunk.isMeshGenerated() && !chunk.isEmpty())
                m_chunks[chunkPos] = generateChunk(chunkPos);
        }
    }

    void Terrain::generateMinimumChunk(const glm::ivec3& position) {
        Chunk chunk(this, position);
        chunk.generateVoxels(m_generator);

        m_registryMutex.lock();
        entt::entity chunkEntity = m_registry.create();
        m_registry.emplace<Chunk>(chunkEntity, std::move(chunk));
        m_chunks[position] = chunkEntity;
        m_registryMutex.unlock();
    }
}