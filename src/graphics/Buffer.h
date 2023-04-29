#pragma once

#include "../Application.h"
#include "ResourceManager.h"

namespace vanguard {
    class UniformBuffer {
    public:
        UniformBuffer() = default;
        UniformBuffer(const UniformBuffer&) = delete;
        UniformBuffer& operator=(const UniformBuffer&) = delete;

        UniformBuffer(UniformBuffer&& other) noexcept {
            m_buffer = other.m_buffer;
            m_size = other.m_size;
            m_stride = other.m_stride;
            m_perFrame = other.m_perFrame;

            other.m_buffer = UNDEFINED_RESOURCE;
            other.m_size = 0;
            other.m_stride = 0;
            other.m_perFrame = false;
        }
        UniformBuffer& operator=(UniformBuffer&& other) noexcept {
            m_buffer = other.m_buffer;
            m_size = other.m_size;
            m_stride = other.m_stride;
            m_perFrame = other.m_perFrame;

            other.m_buffer = UNDEFINED_RESOURCE;
            other.m_size = 0;
            other.m_stride = 0;
            other.m_perFrame = false;

            return *this;
        }

        template <typename T>
        void create(bool perFrame = true) {
            m_perFrame = perFrame;
            m_stride = Vulkan::padUniformBufferSize(sizeof(T));
            m_size = perFrame ? m_stride * FRAMES_IN_FLIGHT : m_stride;

            BufferInfo bufferInfo = {};
            bufferInfo.size = m_size;
            bufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
            bufferInfo.memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
            //bufferInfo.memoryFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            bufferInfo.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            m_buffer = RENDER_SYSTEM.getResourceManager().createBuffer(bufferInfo);
        }

        template <typename T>
        void update(const T& data) {
            uint32_t offset = m_perFrame ? m_stride * RENDER_SYSTEM.getFrameIndex() : 0;
            RENDER_SYSTEM.getStager().updateBuffer(m_buffer, offset, m_stride, &data);
        }

        [[nodiscard]] ResourceRef getBuffer() const { return m_buffer; }
        [[nodiscard]] uint32_t getSize() const { return m_size; }
        [[nodiscard]] uint32_t getStride() const { return m_stride; }
        [[nodiscard]] bool isPerFrame() const { return m_perFrame; }
    private:
        ResourceRef m_buffer = UNDEFINED_RESOURCE;
        uint32_t m_size = 0;
        uint32_t m_stride = 0;
        bool m_perFrame = false;
    };

    class VertexBuffer {
    public:
        VertexBuffer() = default;
        VertexBuffer(const VertexBuffer&) = delete;
        VertexBuffer& operator=(const VertexBuffer&) = delete;

        template <typename T>
        void create(const std::vector<T>& vertices) {
            m_size = sizeof(T) * vertices.size();

            BufferInfo bufferInfo = {};
            bufferInfo.size = m_size;
            bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
            bufferInfo.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            m_buffer = RENDER_SYSTEM.getResourceManager().createBuffer(bufferInfo);
            RENDER_SYSTEM.getStager().updateBuffer(m_buffer, 0, m_size, vertices.data());
        }

        void bind(vk::CommandBuffer cmd) const {
            cmd.bindVertexBuffers(0, *RENDER_SYSTEM.getResourceManager().getBuffer(m_buffer).buffer, static_cast<vk::DeviceSize>(0));
        }

        [[nodiscard]] ResourceRef getBuffer() const { return m_buffer; }
        [[nodiscard]] uint32_t getSize() const { return m_size; }
    private:
        ResourceRef m_buffer = UNDEFINED_RESOURCE;
        uint32_t m_size = 0;
    };

//    class UniformStorageBuffer {
//    public:
//        UniformStorageBuffer() = default;
//        UniformStorageBuffer(const UniformStorageBuffer&) = delete;
//        UniformStorageBuffer& operator=(const UniformStorageBuffer&) = delete;
//
//        UniformStorageBuffer(UniformStorageBuffer&& other) noexcept {
//            m_buffer = other.m_buffer;
//            m_size = other.m_size;
//            m_stride = other.m_stride;
//            m_perFrame = other.m_perFrame;
//
//            other.m_buffer = UNDEFINED_RESOURCE;
//            other.m_size = 0;
//            other.m_stride = 0;
//            other.m_perFrame = false;
//        }
//        UniformStorageBuffer& operator=(UniformStorageBuffer&& other) noexcept {
//            m_buffer = other.m_buffer;
//            m_size = other.m_size;
//            m_stride = other.m_stride;
//            m_perFrame = other.m_perFrame;
//
//            other.m_buffer = UNDEFINED_RESOURCE;
//            other.m_size = 0;
//            other.m_stride = 0;
//            other.m_perFrame = false;
//
//            return *this;
//        }
//
//        template <typename T>
//        void create(bool perFrame, uint32_t initialCount = 0) {
//            m_perFrame = perFrame;
//            m_stride = Vulkan::padUniformBufferSize(sizeof(T));
//            m_size = perFrame ? m_stride * initialCount * FRAMES_IN_FLIGHT : m_stride;
//            m_count = initialCount;
//
//            if(m_count > 0) {
//                BufferInfo bufferInfo = {};
//                bufferInfo.size = m_size;
//                bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
//                bufferInfo.memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
//                bufferInfo.memoryFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
//                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
//                m_buffer = RENDER_SYSTEM.getResourceManager().createBuffer(bufferInfo);
//            }
//        }
//
//        template <typename T>
//        void update(const T& data) {
//            uint32_t offset = m_perFrame ? m_stride * RENDER_SYSTEM.getFrameIndex() : 0;
//            RENDER_SYSTEM.getStager().updateBuffer(m_buffer, offset, m_stride, &data);
//        }
//
//        void setAssociatedDescriptorSet(ResourceRef descriptorSet) {
//            m_descriptorSet = descriptorSet;
//        }
//    private:
//        template <typename T>
//        void resizeBuffer(uint32_t newCount) {
//
//        }
//    private:
//        ResourceRef m_buffer = UNDEFINED_RESOURCE;
//        ResourceRef m_descriptorSet = UNDEFINED_RESOURCE;
//        uint32_t m_size = 0;
//        uint32_t m_stride = 0;
//        uint32_t m_count = 0;
//        bool m_perFrame = false;
//    };
}