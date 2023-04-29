#pragma once

#include "ResourceManager.h"
#include "RenderSystem.h"
#include "../Application.h"
#include "Buffer.h"
#include "Texture.h"

namespace vanguard {
    class FrameGraph {
    public:
        FrameGraph() = default;
        ~FrameGraph();

        FrameGraph(const FrameGraph&) = delete;
        FrameGraph& operator=(const FrameGraph&) = delete;

        FrameGraph(FrameGraph&& other) noexcept;
        FrameGraph& operator=(FrameGraph&& other) noexcept;

        [[nodiscard]] const CommandsInfo& getCommands() const { return m_commands; }

        class DescriptorSet {
        public:
            DescriptorSet() = default;
            DescriptorSet(uint32_t location, ResourceRef layout, std::vector<ResourceRef> descriptorSets) :
                m_location(location), m_descriptorSetLayout(layout), m_descriptorSets(std::move(descriptorSets)) {}
            void destroy() {
                if(m_descriptorSetLayout == UNDEFINED_RESOURCE) return;

                for(ResourceRef descriptorSet : m_descriptorSets) {
                    RENDER_SYSTEM.getResourceManager().destroyDescriptorSet(descriptorSet);
                }
                RENDER_SYSTEM.getResourceManager().destroyDescriptorSetLayout(m_descriptorSetLayout);
            }

            void bindGraphics(ResourceRef pipeline, vk::CommandBuffer cmd) const {
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *RENDER_SYSTEM.getResourceManager().getRenderPipeline(pipeline).pipelineLayout,
                                       0, *getDescriptorSet().set, {});
            }
            void bindCompute(ResourceRef pipeline, vk::CommandBuffer cmd) const {
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *RENDER_SYSTEM.getResourceManager().getComputePipeline(pipeline).pipelineLayout,
                                       0, *getDescriptorSet().set, {});
            }
            [[nodiscard]] const vanguard::DescriptorSet& getDescriptorSet() const {
                return RENDER_SYSTEM.getResourceManager().getDescriptorSet(m_descriptorSets[RENDER_SYSTEM.getFrameIndex()]);
            }
            [[nodiscard]] uint32_t getLocation() const { return m_location; }
        private:
            uint32_t m_location = 0;
            ResourceRef m_descriptorSetLayout = UNDEFINED_RESOURCE;
            std::vector<ResourceRef> m_descriptorSets;
        };

        [[nodiscard]] const std::unordered_map<uint32_t, FrameGraph::DescriptorSet>& getDescriptorSets() const { return m_descriptorSets; }

        friend class FrameGraphBuilder;
    private:
        CommandsInfo m_commands;

        std::vector<ResourceRef> m_images;
        std::vector<ResourceRef> m_samplers;
        std::vector<ResourceRef> m_pipelines;
        std::unordered_map<uint32_t, DescriptorSet> m_descriptorSets;
    };

    // Frame Graph Builder Resource Ref
    enum class FGBResourceType {
        Image,
        DepthStencil,
        UniformBuffer,
        UniformStorageBuffer,
        UniformSampledImage,
        UniformStorageImage,
        RenderPass,
        ComputePass
    };

    const uint32_t FGB_UNDEFINED_RESOURCE = UINT32_MAX;
    const uint32_t FGB_SWAPCHAIN_EXTENT = UINT32_MAX;

    struct FGBResourceRef {
        FGBResourceRef() = default;
        FGBResourceRef(FGBResourceType type, uint32_t location) : type(type), location(location) {}

        bool operator == (const FGBResourceRef& other) const {
            return type == other.type && location == other.location;
        }

        FGBResourceRef& operator=(const FGBResourceRef&) = default;

        FGBResourceType type = FGBResourceType::Image;
        uint32_t location = FGB_UNDEFINED_RESOURCE;
    };

    struct FGBExtent {
        uint32_t width = FGB_SWAPCHAIN_EXTENT;
        uint32_t height = FGB_SWAPCHAIN_EXTENT;
    };

    struct FGBImageInfo {
        vk::Format format = vk::Format::eR8G8B8A8Unorm;
        FGBExtent extent{};
    };

    struct FGBDepthStencilInfo {
        vk::Format format = Vulkan::getDepthFormat();
        FGBExtent extent{};
    };

    // Command Buffer, Pipeline, Descriptor Sets
    typedef std::function<void(vk::CommandBuffer, ResourceRef, std::unordered_map<uint32_t, FrameGraph::DescriptorSet>)> FGBPassCallback;
    struct FGBRenderPassInfo {
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        std::vector<FGBResourceRef> inputs;
        std::vector<FGBResourceRef> outputs;
        FGBPassCallback callback;
        FGBExtent extent{};
        std::optional<VertexInputData> vertexInputData;
        bool depthTest = true;
        bool depthWrite = true;
    };
    struct FGBComputePassInfo {
        std::string computeShaderPath;
        std::vector<FGBResourceRef> inputs;
        std::vector<FGBResourceRef> outputs;
        FGBPassCallback callback;
    };
    typedef std::variant<FGBComputePassInfo, FGBRenderPassInfo> FGBPassInfo;

    struct FGBUniformBufferInfo {
        uint32_t location = 0;
        uint32_t binding = 0;
        const UniformBuffer* buffer = nullptr;
    };
//    struct FGBUniformStorageBufferInfo {
//        uint32_t location = 0;
//        uint32_t binding = 0;
//        const UniformBuffer* buffer = nullptr;
//    };
    struct FGBUniformSampledImageInfo {
        uint32_t location = 0;
        uint32_t binding = 0;
        std::optional<FGBResourceRef> image{};
        std::optional<const Texture*> texture{};
        SamplerInfo samplerInfo{};
    };
    struct FGBUniformStorageImageInfo {
        uint32_t location = 0;
        uint32_t binding = 0;
        FGBResourceRef image{};
    };
    typedef std::variant<FGBUniformBufferInfo, FGBUniformSampledImageInfo, FGBUniformStorageImageInfo> FGBUniformInfo;

    class FrameGraphBuilder {
    public:
        FrameGraphBuilder() = default;
        ~FrameGraphBuilder() = default;

        FrameGraphBuilder(const FrameGraphBuilder&) = delete;
        FrameGraphBuilder& operator=(const FrameGraphBuilder&) = delete;
        FrameGraphBuilder(FrameGraphBuilder&& other) = delete;
        FrameGraphBuilder& operator=(FrameGraphBuilder&& other) = delete;

        FGBResourceRef createImage(const FGBImageInfo& info = FGBImageInfo{});
        FGBResourceRef createDepthStencil(const FGBDepthStencilInfo& info = FGBDepthStencilInfo{});

        FGBResourceRef addRenderPass(const FGBRenderPassInfo& info);
        FGBResourceRef addComputePass(const FGBComputePassInfo& info);

        FGBResourceRef addUniformBuffer(uint32_t location, uint32_t binding, const UniformBuffer* buffer);
        FGBResourceRef addUniformSampledImage(uint32_t location, uint32_t binding, FGBResourceRef image, const SamplerInfo& samplerInfo = SamplerInfo{});
        FGBResourceRef addUniformSampledImage(uint32_t location, uint32_t binding, const Texture* texture, const SamplerInfo& samplerInfo = SamplerInfo{});
        FGBResourceRef addUniformStorageImage(uint32_t location, uint32_t binding, FGBResourceRef image);

        void setBackbuffer(FGBResourceRef image);

        [[nodiscard]] FrameGraph bake();
    private:
        std::vector<FGBImageInfo> m_images;
        std::vector<FGBDepthStencilInfo> m_depthStencils;

        std::vector<FGBPassInfo> m_passes;

        std::vector<FGBUniformInfo> m_uniforms;

        FGBResourceRef m_backbuffer = { FGBResourceType::Image, FGB_UNDEFINED_RESOURCE };
    };
}

template <>
struct std::hash<vanguard::FGBResourceRef> {
    std::size_t operator()(const vanguard::FGBResourceRef& ref) const {
        return (std::hash<uint32_t>()(ref.location) << 4) | std::hash<uint32_t>()(static_cast<uint32_t>(ref.type));
    }
};