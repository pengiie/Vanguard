#pragma once

#include "Allocator.h"
#include "Vulkan.h"

#include "../Config.h"
#include "VertexInput.h"

namespace vanguard {
    typedef uint32_t ResourceRef;
    constexpr ResourceRef UNDEFINED_RESOURCE = UINT32_MAX;

    template <typename T, typename U>
    class ResourcePool {
    public:
        virtual ResourceRef create(const U& info) = 0;
        inline void destroy(ResourceRef ref) {
            m_freeIndices.push_back(ref);
            m_resources[ref].~T();
        }

        [[nodiscard]] inline const T& get(ResourceRef ref) const {
            return m_resources[ref];
        }
    protected:
        [[nodiscard]] inline ResourceRef allocate(T&& resource) {
            if(m_freeIndices.empty()) {
                m_resources.emplace_back(std::move(resource));
                return static_cast<ResourceRef>(m_resources.size() - 1);
            } else {
                ResourceRef ref = m_freeIndices.back();
                m_freeIndices.pop_back();
                m_resources[ref] = std::move(resource);
                return ref;
            }
        }
    private:
        std::vector<T> m_resources;
        std::vector<ResourceRef> m_freeIndices;
    };

    enum class ImageType {
        Image2D,
        Cube
    };
    struct ImageInfo {
        vk::Format format = vk::Format::eUndefined;
        vk::ImageUsageFlags usage;
        vk::ImageAspectFlags aspect;
        vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t arrayLayers = 1;
        ImageType type = ImageType::Image2D;
    };
    struct Image {
        ImageInfo info;
        vk::raii::Image image;
        vk::raii::ImageView view;
        Allocation allocation;
    };

    class ImagePool : public ResourcePool<Image, ImageInfo> {
    public:
        ResourceRef create(const ImageInfo& info) override;
    };

    struct BufferInfo {
        size_t size = 0;
        vk::BufferUsageFlags usage;
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VmaAllocationCreateFlags memoryFlags;
        vk::MemoryPropertyFlags memoryProperties;
    };
    struct Buffer {
        BufferInfo info;
        vk::raii::Buffer buffer;
        Allocation allocation;
    };

    class BufferPool : public ResourcePool<Buffer, BufferInfo> {
    public:
        ResourceRef create(const BufferInfo& info) override;
    };

    struct SamplerInfo {
        vk::Filter magFilter = vk::Filter::eLinear;
        vk::Filter minFilter = vk::Filter::eLinear;
        vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear;
        vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
        vk::SamplerAddressMode addressModeW = vk::SamplerAddressMode::eRepeat;
    };

    struct Sampler {
        SamplerInfo info;
        vk::raii::Sampler sampler;
    };

    class SamplerPool : public ResourcePool<Sampler, SamplerInfo> {
    public:
        ResourceRef create(const SamplerInfo& info) override;
    };

    struct DescriptorSetBinding {
        uint32_t binding;
        vk::DescriptorType type;
        uint32_t count = 1;
        vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAll;
    };
    struct DescriptorSetLayoutInfo {
        std::vector<DescriptorSetBinding> bindings;
    };
    struct DescriptorSetLayout {
        DescriptorSetLayoutInfo info;
        vk::raii::DescriptorSetLayout layout;
    };

    class DescriptorSetLayoutPool : public ResourcePool<DescriptorSetLayout, DescriptorSetLayoutInfo> {
    public:
        ResourceRef create(const DescriptorSetLayoutInfo& info) override;
    };

    struct DescriptorSetInfo {
        ResourceRef layout;
    };
    struct DescriptorSet {
        DescriptorSetInfo info;
        vk::raii::DescriptorSet set;
    };

    struct DescriptorImageInfo {
        ResourceRef image = UNDEFINED_RESOURCE;
        vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        ResourceRef sampler = UNDEFINED_RESOURCE;
    };
    struct DescriptorBufferInfo {
        ResourceRef buffer = UNDEFINED_RESOURCE;
        vk::DeviceSize offset = 0;
        vk::DeviceSize size = VK_WHOLE_SIZE;
    };
    struct DescriptorSetWrite {
        uint32_t binding = -1;
        vk::DescriptorType type = vk::DescriptorType::eUniformBuffer;
        std::optional<DescriptorImageInfo> image;
        std::optional<DescriptorBufferInfo> buffer;
    };

    class DescriptorSetPool : public ResourcePool<DescriptorSet, DescriptorSetInfo> {
    public:
        ResourceRef create(const DescriptorSetInfo& info) override;
        void update(ResourceRef ref, const std::vector<DescriptorSetWrite>& writes) const;
        void cmdUpdate(vk::CommandBuffer& cmd, ResourceRef ref, const std::vector<DescriptorSetWrite>& writes) const;
    };

    struct RenderPipelineImageInfo {
        ResourceRef image = UNDEFINED_RESOURCE;
        vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;
        vk::ImageLayout finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
        vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    };
    struct RenderPipelineInfo {
        std::vector<ResourceRef> descriptorSetLayouts;
        std::vector<RenderPipelineImageInfo> inputAttachments;
        std::vector<RenderPipelineImageInfo> colorAttachments;
        RenderPipelineImageInfo depthStencilAttachment{};
        bool depthTest = false;
        bool depthWrite = false;
        vk::Extent2D extent;
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        std::optional<VertexInputData> vertexInputData;
    };

    struct RenderPipeline {
        RenderPipelineInfo info;
        vk::raii::RenderPass renderPass;
        vk::raii::Framebuffer framebuffer;
        vk::raii::PipelineLayout pipelineLayout;
        vk::raii::Pipeline pipeline;
    };

    class RenderPipelinePool : public ResourcePool<RenderPipeline, RenderPipelineInfo> {
    public:
        ResourceRef create(const RenderPipelineInfo& info) override;
    };

    struct ComputePipelineInfo {
        std::vector<ResourceRef> descriptorSetLayouts;
        std::string computeShaderPath;
    };

    struct ComputePipeline {
        ComputePipelineInfo info;
        vk::raii::PipelineLayout pipelineLayout;
        vk::raii::Pipeline pipeline;
    };

    class ComputePipelinePool : public ResourcePool<ComputePipeline, ComputePipelineInfo> {
    public:
        ResourceRef create(const ComputePipelineInfo& info) override;
    };

    class ResourceManager {
    public:
        [[nodiscard]] inline ResourceRef createImage(const ImageInfo& info) { return m_imagePool.create(info); }
        [[nodiscard]] inline ResourceRef createBuffer(const BufferInfo& info) { return m_bufferPool.create(info); }
        [[nodiscard]] inline ResourceRef createSampler(const SamplerInfo& info) { return m_samplerPool.create(info); }
        [[nodiscard]] inline ResourceRef createDescriptorSetLayout(const DescriptorSetLayoutInfo& info) { return m_descriptorSetLayoutPool.create(info); }
        [[nodiscard]] inline ResourceRef createDescriptorSet(const DescriptorSetInfo& info) { return m_descriptorSetPool.create(info); }
        [[nodiscard]] inline ResourceRef createRenderPipeline(const RenderPipelineInfo& info) { return m_renderPipelinePool.create(info); }
        [[nodiscard]] inline ResourceRef createComputePipeline(const ComputePipelineInfo& info) { return m_computePipelinePool.create(info); }

        inline void updateDescriptorSet(ResourceRef ref, const std::vector<DescriptorSetWrite>& writes) const { m_descriptorSetPool.update(ref, writes); }

        inline void destroyImage(ResourceRef ref) { m_imagePool.destroy(ref); }
        inline void destroyBuffer(ResourceRef ref) { m_bufferPool.destroy(ref); }
        inline void destroySampler(ResourceRef ref) { m_samplerPool.destroy(ref); }
        inline void destroyDescriptorSetLayout(ResourceRef ref) { m_descriptorSetLayoutPool.destroy(ref); }
        inline void destroyDescriptorSet(ResourceRef ref) { m_descriptorSetPool.destroy(ref); }
        inline void destroyRenderPipeline(ResourceRef ref) { m_renderPipelinePool.destroy(ref); }
        inline void destroyComputePipeline(ResourceRef ref) { m_computePipelinePool.destroy(ref); }

        [[nodiscard]] inline const Image& getImage(ResourceRef ref) const { return m_imagePool.get(ref); }
        [[nodiscard]] inline const Buffer& getBuffer(ResourceRef ref) const { return m_bufferPool.get(ref); }
        [[nodiscard]] inline const Sampler& getSampler(ResourceRef ref) const { return m_samplerPool.get(ref); }
        [[nodiscard]] inline const DescriptorSetLayout& getDescriptorSetLayout(ResourceRef ref) const { return m_descriptorSetLayoutPool.get(ref); }
        [[nodiscard]] inline const DescriptorSet& getDescriptorSet(ResourceRef ref) const { return m_descriptorSetPool.get(ref); }
        [[nodiscard]] inline const RenderPipeline& getRenderPipeline(ResourceRef ref) const { return m_renderPipelinePool.get(ref); }
        [[nodiscard]] inline const ComputePipeline& getComputePipeline(ResourceRef ref) const { return m_computePipelinePool.get(ref); }
    private:
        ImagePool m_imagePool;
        BufferPool m_bufferPool;
        SamplerPool m_samplerPool;
        DescriptorSetLayoutPool m_descriptorSetLayoutPool;
        DescriptorSetPool m_descriptorSetPool;
        RenderPipelinePool m_renderPipelinePool;
        ComputePipelinePool m_computePipelinePool;
    };
}