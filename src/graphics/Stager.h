#pragma once

#include "ResourceManager.h"

#include <unordered_map>

namespace vanguard {
    struct CopyJob {
        ResourceRef stagingBuffer;
        ResourceRef dstBuffer;
        uint32_t stagingOffset;
        uint32_t dstOffset;
        uint32_t size;
    };

    struct ImageCopyJob {
        ResourceRef stagingBuffer = UNDEFINED_RESOURCE;
        ResourceRef dstImage = UNDEFINED_RESOURCE;
        vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined;
        uint32_t stagingOffset = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t arrayLayer = 0;
    };

    class Stager {
    public:
        Stager() = default;

        Stager(const Stager&) = delete;
        Stager& operator=(const Stager&) = delete;

        Stager(Stager&& other) = delete;
        Stager& operator=(Stager&& other) = delete;

        void createStagingBuffer(uint32_t size);
        void updateBuffer(ResourceRef buffer, uint32_t offset, uint32_t size, const void* data);
        void copyBuffer(ResourceRef srcBuffer, ResourceRef dstBuffer, uint32_t srcOffset, uint32_t dstOffset, uint32_t size);

        void updateImage(ResourceRef image, vk::ImageLayout currentLayout, uint32_t size, const void* data, uint32_t arrayLayer = 0);

        void bakeCommands(vk::CommandBuffer commandBuffer);
        void flush();
    private:
        std::pair<ResourceRef, uint32_t> findStagingBuffer(uint32_t size);
    private:
        std::vector<ResourceRef> m_stagingBuffers;
        std::unordered_map<ResourceRef, uint32_t> m_stagingBufferPointers;
        std::vector<CopyJob> m_jobs;
        std::vector<ImageCopyJob> m_imageJobs;
    };
}