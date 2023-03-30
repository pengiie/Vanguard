#include "ResourceManager.h"

#include "../Config.h"
#include "../Application.h"

namespace vanguard {

    void ResourceManager::init() {
        m_stagingBuffers.reserve(INITIAL_STAGING_BUFFER_COUNT);
        for (int i = 0; i < INITIAL_STAGING_BUFFER_COUNT; i++) {
            m_stagingBuffers.push_back(createStagingBuffer(1024 * 1024));
        }
        m_stagingMemoryBarriers.resize(FRAMES_IN_FLIGHT);
        m_updatedBuffers.resize(FRAMES_IN_FLIGHT);

        m_commandPool = Vulkan::getDevice().createCommandPool(vk::CommandPoolCreateInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = Vulkan::getQueueFamilyIndex()
        });
        m_stagingCommandBuffers = Vulkan::getDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo{
            .commandPool = **m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = FRAMES_IN_FLIGHT_PLUS_ONE
        });
    }

    BufferRef ResourceManager::createBuffer(uint32_t size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, vk::MemoryPropertyFlags requiredFlags) {
        vk::raii::Buffer buffer = Vulkan::getDevice().createBuffer(vk::BufferCreateInfo{
            .size = size,
            .usage = usage,
        });
        VmaAllocationCreateInfo allocInfo = {
                .usage = memoryUsage,
                .requiredFlags = static_cast<VkMemoryPropertyFlags>(requiredFlags),
        };
        Allocation allocation;
        vmaAllocateMemoryForBuffer(*Vulkan::getAllocator(), static_cast<VkBuffer>(*buffer), &allocInfo, &allocation.allocation, &allocation.allocationInfo);
        vmaBindBufferMemory(*Vulkan::getAllocator(), allocation.allocation, static_cast<VkBuffer>(*buffer));

        AllocatedBuffer allocatedBuffer{
                .buffer = std::move(buffer),
                .allocation = std::move(allocation),
                .bufferSize = size
        };

        BufferRef bufferRef = m_bufferId++;
        m_buffers.emplace(bufferRef, std::move(allocatedBuffer));
        return bufferRef;
    }

    const AllocatedBuffer& ResourceManager::getBuffer(BufferRef buffer) const {
        return m_buffers.at(buffer);
    }

    void ResourceManager::updateBuffer(BufferRef buffer, const void* data, size_t size,
                                       vk::AccessFlags accessFlags) {
        auto frameIndex = Application::Get().getRenderSystem().getFrameIndex();
        auto& commandBuffer = m_stagingCommandBuffers[m_stagingCommandBufferIndex];
        auto& allocatedBuffer = m_buffers.at(buffer);
        if(size > allocatedBuffer.bufferSize) {
            ERROR("Buffer size is too small to update");
            return;
        }

        // Find a staging buffer that is not in use, if none is found, create a new one
        BufferRef stagingBufferRef = getStagingBuffer(size);
        m_stagingBuffersInUse.insert(stagingBufferRef);
        auto& stagingBuffer = m_buffers.at(stagingBufferRef);

        void* mappedData;
        vmaMapMemory(*Vulkan::getAllocator(), stagingBuffer.allocation.allocation, &mappedData);
        memcpy(mappedData, data, size);
        vmaUnmapMemory(*Vulkan::getAllocator(), stagingBuffer.allocation.allocation);

        commandBuffer.copyBuffer(*stagingBuffer.buffer, *allocatedBuffer.buffer, vk::BufferCopy{
            .size = size,
        });
        m_stagingMemoryBarriers[frameIndex].push_back(vk::BufferMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = accessFlags,
            .buffer = *allocatedBuffer.buffer,
            .offset = 0,
            .size = size,
        });
        m_updatedBuffers[frameIndex].push_back(buffer);
    }

    BufferRef ResourceManager::createStagingBuffer(uint32_t size) {
        return createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY, {});
    }

    void ResourceManager::begin() {
        vk::raii::CommandBuffer& commandBuffer = m_stagingCommandBuffers[m_stagingCommandBufferIndex];
        commandBuffer.reset();
        commandBuffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        auto frameIndex = Application::Get().getRenderSystem().getFrameIndex();
        m_stagingMemoryBarriers[frameIndex].clear();

        for (BufferRef ref: m_updatedBuffers[frameIndex]) {
            m_buffers.at(ref).hasData = true;
        }
        m_updatedBuffers[frameIndex].clear();
    }

    vk::CommandBuffer ResourceManager::bakeStagingCommandBuffer() {
        auto frameIndex = Application::Get().getRenderSystem().getFrameIndex();
        vk::raii::CommandBuffer& commandBuffer = m_stagingCommandBuffers[m_stagingCommandBufferIndex];
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, {}, {}, m_stagingMemoryBarriers[frameIndex], {});
        commandBuffer.end();

        m_stagingCommandBufferIndex = (m_stagingCommandBufferIndex + 1) % FRAMES_IN_FLIGHT_PLUS_ONE;
        return *commandBuffer;
    }

    BufferRef ResourceManager::getStagingBuffer(uint32_t size) {
        for (auto& ref : m_stagingBuffers) {
            if (m_stagingBuffersInUse.find(ref) == m_stagingBuffersInUse.end()) {
                if(m_buffers.at(ref).bufferSize >= size) {
                    return ref;
                    break;
                }
            }
        }

        BufferRef ref = createStagingBuffer(size * 2);
        m_stagingBuffers.push_back(ref);
        return ref;
    }

    void UniformBuffer::create(size_t size) {
        for (BufferRef& buffer : m_buffers) {
            buffer = Application::Get().getRenderSystem().getResourceManager().createBuffer(
                size,
                vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_GPU_ONLY,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );
        }
    }

    void UniformBuffer::update(const void* data, size_t size) {
        auto frameIndex = Application::Get().getRenderSystem().getFrameIndex();
        Application::Get().getRenderSystem().getResourceManager().updateBuffer(m_buffers[frameIndex], data, size, vk::AccessFlagBits::eUniformRead);
    }

    BufferRef UniformBuffer::get() const {
        return m_buffers[Application::Get().getRenderSystem().getFrameIndex()];
    }

    BufferRef UniformBuffer::getForFrame(uint32_t frameIndex) const {
        return m_buffers[frameIndex];
    }

    void VertexBuffer::create(size_t size) {
        m_buffer = Application::Get().getRenderSystem().getResourceManager().createBuffer(
            size,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
    }

    void VertexBuffer::update(const void* data, size_t size) const {
        Application::Get().getRenderSystem().getResourceManager().updateBuffer(m_buffer, data, size, vk::AccessFlagBits::eVertexAttributeRead);
    }

    BufferRef VertexBuffer::get() const {
        return m_buffer;
    }

    bool VertexBuffer::isLoaded() const {
        return m_buffer != UNDEFINED_BUFFER && Application::Get().getRenderSystem().getResourceManager().getBuffer(m_buffer).hasData;
    }

    void VertexBuffer::bind(const vk::CommandBuffer& commandBuffer) const {
        commandBuffer.bindVertexBuffers(0, *Application::Get().getRenderSystem().getResourceManager().getBuffer(m_buffer).buffer, {0});
    }
}