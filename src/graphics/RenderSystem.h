#pragma once

#include <optional>
#include "../Config.h"
#include "../Window.h"
#include "RenderGraph.h"
#include <vulkan/vulkan_raii.hpp>

#include <mutex>

namespace vanguard {
    struct AllocatedBuffer {
        vk::raii::Buffer buffer;
        Allocation allocation;
        size_t bufferSize;
    };

    struct FrameData {
        vk::raii::Semaphore imageAvailableSemaphore;
        vk::raii::Semaphore swapchainImageTransitionSemaphore;
        vk::raii::Semaphore renderFinishedSemaphore;
        vk::raii::Semaphore blitFinishedSemaphore;
        vk::raii::Fence inFlightFence;

        vk::raii::CommandPool commandPool;
        vk::raii::CommandBuffer contextCommandBuffer;
        vk::raii::CommandBuffer swapchainImageTransitionCommandBuffer;
        vk::raii::CommandBuffer renderCommandBuffer;
        vk::raii::CommandBuffer blitCommandBuffer;

        std::unordered_map<BufferRef, AllocatedBuffer> buffers;
    };

    class RenderSystem {
    public:
        void init();
        void bake(RenderGraphBuilder&& builder);
        void render(Window& window);

        BufferRef createBuffer(size_t allocationSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, bool perFrame);
        BufferRef createUniformBuffer(size_t allocationSize);
        void destroyBuffer(BufferRef bufferReference);

        void updateBuffer(BufferRef bufferReference, const void* data, size_t size, bool perFrame);
        template<typename T>
        void updateBuffer(BufferRef bufferReference, T& data, bool perFrame) {
            updateBuffer(bufferReference, &data, sizeof(T), perFrame);
        }

        AllocatedBuffer& getBuffer(BufferRef bufferReference, uint32_t frameIndex);
        [[nodiscard]] inline uint32_t getFrameCount() const { return m_frameCount; }
        [[nodiscard]] inline std::mutex& getBufferMutex() { return m_bufferMutex; }
    public:
        inline void rebake() { bake(std::move(m_renderGraphBuilder)); }

        BufferRef createBufferInternal(size_t allocationSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, bool perFrame);
        AllocatedBuffer& getBufferInternal(BufferRef bufferReference, uint32_t frameIndex);
        void updateBufferInternal(AllocatedBuffer& buffer, const void* data, size_t size);
    private:
        std::vector<FrameData> m_frameData{};
        uint32_t m_currentFrame = 0;
        uint32_t m_frameCount = 0;

        std::optional<vk::raii::CommandPool> m_stagingCommandPool;
        std::optional<vk::raii::CommandBuffer> m_stagingCommandBuffer;
        BufferRef m_stagingBuffer;

        std::mutex m_bufferMutex;

        uint32_t m_bufferIndex = 0;
        uint32_t m_localBufferIndex = 0;
        uint32_t m_perFrameBufferIndex = 0;
        std::unordered_map<BufferRef, AllocatedBuffer> m_buffers;
        // Maps a buffer reference to the location of the buffer, boolean for if it is per frame, and the index of the buffer in the respective vector
        std::unordered_map<BufferRef, std::pair<bool, uint32_t>> m_bufferReferences;
        std::vector<std::vector<BufferRef>> m_bufferDestroyQueue;

        // Functions that require the resources of the current frame to be usable
        std::vector<std::function<void(vk::raii::CommandBuffer&)>> m_contextFunctions;
        // Functions that can execute on the queue while waiting for the next image to be available
        std::vector<std::function<void()>> m_queueFunctions;
        std::mutex m_queueFunctionsMutex;

        // Render graph
        std::unique_ptr<RenderGraph> m_renderGraph;
        RenderGraphBuilder m_renderGraphBuilder;
    };
}