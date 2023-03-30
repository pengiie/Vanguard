#pragma once

#include "Allocator.h"
#include "Vulkan.h"

#include "../Config.h"
#define INITIAL_STAGING_BUFFER_COUNT 16

namespace vanguard {
    typedef uint32_t BufferRef;
    const BufferRef UNDEFINED_BUFFER = UINT32_MAX;
    struct AllocatedBuffer {
        vk::raii::Buffer buffer;
        Allocation allocation;
        size_t bufferSize;

        // Once the buffer is updated, this value will be set to true and the data will be available
        bool hasData = false;
    };

    // Behind the scenes manages two buffers and gets the correct buffer per frame
    class UniformBuffer {
    public:
        void create(size_t size);
        void update(const void* data, size_t size);

        [[nodiscard]] BufferRef get() const;
        [[nodiscard]] BufferRef getForFrame(uint32_t frameIndex) const;
    private:
        std::array<BufferRef, FRAMES_IN_FLIGHT> m_buffers = { UNDEFINED_BUFFER };
    };

    class VertexBuffer {
    public:
        void create(size_t size);
        void update(const void* data, size_t size) const;

        [[nodiscard]] BufferRef get() const;
        [[nodiscard]] bool isLoaded() const;

        void bind(const vk::CommandBuffer& commandBuffer) const;
    private:
        BufferRef m_buffer = UNDEFINED_BUFFER;
    };

    class ResourceManager {
    public:
        void init();
        void begin();
        vk::CommandBuffer bakeStagingCommandBuffer();

        BufferRef createBuffer(uint32_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, vk::MemoryPropertyFlags requiredFlags);
        const AllocatedBuffer& getBuffer(BufferRef buffer) const;
        void updateBuffer(BufferRef buffer, const void* data, size_t size, vk::AccessFlags accessFlags);
        void destroyBuffer(BufferRef buffer);

        BufferRef createStagingBuffer(uint32_t size);
        BufferRef getStagingBuffer(uint32_t size);
    private:
        uint32_t m_bufferId;
        std::unordered_map<BufferRef, AllocatedBuffer> m_buffers;

        uint32_t m_stagingCommandBufferIndex = 0;
        std::optional<vk::raii::CommandPool> m_commandPool;
        std::vector<vk::raii::CommandBuffer> m_stagingCommandBuffers;
        std::vector<std::vector<vk::BufferMemoryBarrier>> m_stagingMemoryBarriers;
        std::vector<std::vector<BufferRef>> m_updatedBuffers;

        std::vector<BufferRef> m_stagingBuffers;
        std::unordered_set<BufferRef> m_stagingBuffersInUse;
    };
}
