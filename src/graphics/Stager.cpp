#include "Stager.h"
#include "../Application.h"

namespace vanguard {
    void Stager::createStagingBuffer(uint32_t size) {
        BufferInfo info{};
        info.size = size;
        info.usage = vk::BufferUsageFlagBits::eTransferSrc;
        info.memoryUsage = VMA_MEMORY_USAGE_AUTO;
        info.memoryFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        info.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        auto stagingBuffer = RENDER_SYSTEM.getResourceManager().createBuffer(info);
        m_stagingBuffers.push_back(stagingBuffer);
    }

    void Stager::updateBuffer(ResourceRef buffer, uint32_t offset, uint32_t size, const void* data) {
        auto [stagingBufferRef, stagingOffset] = findStagingBuffer(size);

        auto& stagingBuffer = RENDER_SYSTEM.getResourceManager().getBuffer(stagingBufferRef);
        void* mappedData = nullptr;
        vmaMapMemory(*Vulkan::getAllocator(), stagingBuffer.allocation.allocation, &mappedData);
        void* stagingDst = static_cast<char*>(mappedData) + stagingOffset;
        memcpy(stagingDst, data, size);
        vmaUnmapMemory(*Vulkan::getAllocator(), stagingBuffer.allocation.allocation);
        m_jobs.push_back(CopyJob{
            .stagingBuffer = stagingBufferRef,
            .dstBuffer = buffer,
            .stagingOffset = stagingOffset,
            .dstOffset = offset,
            .size = size
        });
    }

    void Stager::copyBuffer(vanguard::ResourceRef srcBuffer, vanguard::ResourceRef dstBuffer, uint32_t srcOffset,
                            uint32_t dstOffset, uint32_t size) {
        m_jobs.push_back(CopyJob{
            .stagingBuffer = srcBuffer,
            .dstBuffer = dstBuffer,
            .stagingOffset = srcOffset,
            .dstOffset = dstOffset,
            .size = size
        });
    }

    void Stager::updateImage(vanguard::ResourceRef image, vk::ImageLayout currentLayout, uint32_t size, const void* data, uint32_t arrayLayer) {
        auto& imageInfo = RENDER_SYSTEM.getResourceManager().getImage(image).info;
        auto [stagingBufferRef, stagingOffset] = findStagingBuffer(size);

        auto& stagingBuffer = RENDER_SYSTEM.getResourceManager().getBuffer(stagingBufferRef);
        void* mappedData = nullptr;
        vmaMapMemory(*Vulkan::getAllocator(), stagingBuffer.allocation.allocation, &mappedData);
        void* stagingDst = static_cast<char*>(mappedData) + stagingOffset;
        memcpy(stagingDst, data, size);
        vmaUnmapMemory(*Vulkan::getAllocator(), stagingBuffer.allocation.allocation);

        m_imageJobs.push_back(ImageCopyJob{
            .stagingBuffer = stagingBufferRef,
            .dstImage = image,
            .currentLayout = currentLayout,
            .stagingOffset = stagingOffset,
            .width = imageInfo.width,
            .height = imageInfo.height,
            .arrayLayer = arrayLayer
        });
    }

    void Stager::bakeCommands(vk::CommandBuffer commandBuffer) {
        std::vector<vk::ImageMemoryBarrier> preBarriers;
        std::unordered_set<ResourceRef> visitedImages;
        for (const auto& job: m_imageJobs) {
            if (visitedImages.find(job.dstImage) != visitedImages.end()) {
                continue;
            }
            visitedImages.insert(job.dstImage);

            auto& dstImage = RENDER_SYSTEM.getResourceManager().getImage(job.dstImage);
            preBarriers.push_back(vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = job.currentLayout,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = *dstImage.image,
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = dstImage.info.arrayLayers
                }
            });
        }
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, preBarriers);

        for(auto& job : m_jobs) {
            auto& stagingBuffer = RENDER_SYSTEM.getResourceManager().getBuffer(job.stagingBuffer);
            auto& dstBuffer = RENDER_SYSTEM.getResourceManager().getBuffer(job.dstBuffer);
            vk::BufferCopy copyRegion{};
            copyRegion.srcOffset = job.stagingOffset;
            copyRegion.dstOffset = job.dstOffset;
            copyRegion.size = job.size;
            commandBuffer.copyBuffer(*stagingBuffer.buffer, *dstBuffer.buffer, copyRegion);
        }
        for (const auto& job: m_imageJobs) {
            auto& stagingBuffer = RENDER_SYSTEM.getResourceManager().getBuffer(job.stagingBuffer);
            auto& dstImage = RENDER_SYSTEM.getResourceManager().getImage(job.dstImage);
            vk::BufferImageCopy copyRegion{};
            copyRegion.bufferOffset = job.stagingOffset;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = job.arrayLayer;
            copyRegion.imageSubresource.layerCount = 1;

            copyRegion.imageOffset = vk::Offset3D{0, 0, 0};
            copyRegion.imageExtent = vk::Extent3D{job.width, job.height, 1};
            commandBuffer.copyBufferToImage(*stagingBuffer.buffer, *dstImage.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
        }
        std::vector<vk::BufferMemoryBarrier> barriers;
        std::vector<vk::ImageMemoryBarrier> postImageBarriers;
        for(auto& job : m_jobs) {
            auto& dstBuffer = RENDER_SYSTEM.getResourceManager().getBuffer(job.dstBuffer);
            vk::BufferMemoryBarrier barrier{};
            barrier.buffer = *dstBuffer.buffer;
            barrier.offset = job.dstOffset;
            barrier.size = job.size;
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            barriers.push_back(barrier);
        }

        visitedImages.clear();
        for (const auto& job: m_imageJobs) {
            if (visitedImages.find(job.dstImage) != visitedImages.end()) {
                continue;
            }
            visitedImages.insert(job.dstImage);

            auto& dstImage = RENDER_SYSTEM.getResourceManager().getImage(job.dstImage);
            postImageBarriers.push_back(vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = *dstImage.image,
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = dstImage.info.arrayLayers
                }
            });
        }
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, {}, nullptr, barriers, postImageBarriers);
    }

    void Stager::flush() {
        m_jobs.clear();
        m_imageJobs.clear();
        m_stagingBufferPointers.clear();
    }

    std::pair<ResourceRef, uint32_t> Stager::findStagingBuffer(uint32_t size) {
        ResourceRef stagingBufferRef = UNDEFINED_RESOURCE;
        uint32_t stagingOffset = 0;

        for(ResourceRef reference : m_stagingBuffers) {
            auto& buf = RENDER_SYSTEM.getResourceManager().getBuffer(reference);
            auto& stagingBufferPointer = m_stagingBufferPointers[reference];
            if(stagingBufferPointer + size < buf.info.size) {
                stagingBufferRef = reference;
                stagingOffset = stagingBufferPointer;
                stagingBufferPointer += size;
                break;
            }

        }
        if(stagingBufferRef == UNDEFINED_RESOURCE) {
            createStagingBuffer(size);
            stagingBufferRef = m_stagingBuffers.back();
            stagingOffset = 0;
            m_stagingBufferPointers[stagingBufferRef] = size;
        }

        return {stagingBufferRef, stagingOffset};
    }
}
