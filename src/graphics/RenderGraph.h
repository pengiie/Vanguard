#pragma once

#include "../Config.h"
#include "Allocator.h"
#include <functional>
#include <vulkan/vulkan_raii.hpp>

namespace vanguard {
    typedef uint32_t BufferRef;
    typedef uint32_t ResourceRef;
    typedef uint32_t RenderPassRef;
    #define UNDEFINED_REFERENCE UINT32_MAX
    typedef std::function<void(const vk::CommandBuffer&, uint32_t)> ExecutionFunction;

    // RenderGraphBuilder structs
    enum class ResourceType {
        Image,
        Stencil,
        Uniform
    };

    struct Resource {
        std::string name;
        Resource() = default;
        virtual ResourceType getType() = 0;
    };

    struct ImageInfo {
        vk::Format format = vk::Format::eUndefined;
    };
    typedef ImageInfo StencilInfo;

    struct ImageResource : Resource {
        ImageInfo info{};
        ImageResource() = default;
        ResourceType getType() override { return ResourceType::Image; }
    };
    struct StencilResource : Resource {
        StencilInfo info{};
        ResourceType getType() override { return ResourceType::Stencil; }
    };

    enum class UniformFrequency: uint8_t {
        PerFrame = 0,
        PerPass = 1,
        SIZE = 2
    };

    struct UniformInfo {
        BufferRef buffer = UNDEFINED_REFERENCE;
        UniformFrequency frequency = UniformFrequency::PerFrame;
        uint32_t binding = 0;
    };

    struct UniformResource : Resource {
        UniformInfo info{};
        ResourceType getType() override { return ResourceType::Uniform; }
    };

    struct VertexAttribute {
        uint32_t location;
        uint32_t offset;
        vk::Format format;
    };
    class VertexInputData {
    public:
        explicit VertexInputData(uint32_t size) : m_stride(size) {}

        void setAttribute(uint32_t location, uint32_t offset, vk::Format format) {
            m_attributes.push_back({
                location,
                offset,
                format
            });
        }

        template <typename T>
        [[nodiscard]] static VertexInputData createVertexInputData() {
            return VertexInputData(sizeof(T));
        }
    private:
        std::vector<VertexAttribute> m_attributes;
        uint32_t m_stride = 0;

        friend class RenderGraph;
    };

    struct RenderPassInfo {
        std::string name;
        std::vector<ResourceRef> inputs;
        std::vector<ResourceRef> outputs;
        VertexInputData vertexInput;
        std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::vector<uint32_t> vertexShader;
        std::vector<uint32_t> fragmentShader;
        ExecutionFunction execution;
        // TODO: Vertex Info
    };

    struct RenderSystem;
    class RenderGraphBuilder {
    public:
        ResourceRef createImage(const std::string& name, const ImageInfo& info);
        ResourceRef createStencil(const std::string& name, const StencilInfo& info);
        ResourceRef createUniform(const std::string& name, const UniformInfo& info);

        void addRenderPass(const RenderPassInfo& info);

        void setBackBuffer(ResourceRef resource);
    private:
        std::vector<std::unique_ptr<Resource>> m_resources;
        std::vector<RenderPassInfo> m_renderPasses;

        ResourceRef m_backBuffer = UNDEFINED_REFERENCE;

        friend class RenderSystem;
    };

    // RenderGraph structs

    // Maps RenderGraphBuilder references to RenderGraph resources
    typedef std::unordered_map<ResourceRef, ResourceRef> ReferenceMap;

    struct RenderPass {
        RenderPassInfo info;
        std::vector<vk::ClearValue> clearValues;
        vk::raii::PipelineLayout pipelineLayout;
        vk::raii::Pipeline pipeline;
        vk::raii::RenderPass renderPass;
        vk::raii::Framebuffer framebuffer;
    };

    struct ImageData {
        vk::raii::Image image;
        vk::raii::ImageView imageView;
        Allocation allocation;
    };
    struct Image {
        std::string name;
        ImageInfo info;
        ImageData imageData;
    };

    typedef uint32_t DescriptorSetLayoutRef;
    struct DescriptorSetData {
        // UniformFrequency as index, then frame in flight as index
        std::array<std::unique_ptr<vk::raii::DescriptorSetLayout>, 4> layouts;
        std::array<std::vector<vk::raii::DescriptorSet>, 4> sets;
    };

    enum class RenderGraphCommandType {
        RenderPass,
        ImageBarrier
    };

    struct RenderGraphCommand {
        virtual ~RenderGraphCommand() = default;
        virtual RenderGraphCommandType getType() = 0;
    };

    struct RenderPassCommand : RenderGraphCommand {
        RenderGraphCommandType getType() override { return RenderGraphCommandType::RenderPass; }
        RenderPassRef pass = UNDEFINED_REFERENCE; /* RenderPass reference which is the index in RenderGraph */
    };

    struct ImageTransition {
        ResourceRef image = UNDEFINED_REFERENCE; /* Image reference which is the index in RenderGraph */
        vk::ImageLayout oldLayout = vk::ImageLayout::eUndefined;
        vk::ImageLayout newLayout = vk::ImageLayout::eUndefined;
        vk::AccessFlags oldAccess;
        vk::AccessFlags newAccess;
    };

    struct ImageBarrierCommand : RenderGraphCommand {
        RenderGraphCommandType getType() override { return RenderGraphCommandType::ImageBarrier; }
        std::vector<ImageTransition> transitions;
    };

    class RenderGraph {
    public:
        RenderGraph() = default;
        // Non-copyable and non-movable
        RenderGraph(RenderGraph&) = delete;
        RenderGraph(RenderGraph&&) = delete;
        RenderGraph& operator=(RenderGraph&) = delete;
        RenderGraph& operator=(RenderGraph&&) = delete;

        ResourceRef createImage(const ImageResource& resource, vk::ImageUsageFlags usageFlags, bool isStencil);

        void createDescriptorSetLayout(UniformFrequency frequency, const std::vector<vk::DescriptorSetLayoutBinding>& bindings);
        void createDescriptorSet(UniformFrequency frequency);
        void updateDescriptorBindings(UniformFrequency frequency, uint32_t currentFrame, const std::vector<std::pair<uint32_t, vk::DescriptorBufferInfo>>& bufferBindings);
        RenderPassRef addRenderPass(ReferenceMap& imageReferenceMap, ResourceRef stencil, const RenderPassInfo& info);

        template<typename T>
        void addCommand(const T& command) {
            static_assert(std::is_base_of<RenderGraphCommand, T>::value, "T must be derived from RenderGraphCommand");
            m_commands.emplace_back(std::make_unique<T>(command));
        }

        void execute(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex);

        [[nodiscard]] inline const ImageData& getBackBufferImage(uint32_t frameIndex) const { return m_images.at(m_backBuffer).imageData; }
    public:
        std::vector<RenderPass> m_renderPasses;
        std::vector<Image> m_images;
        DescriptorSetData m_descriptorSet;
        std::vector<std::unique_ptr<RenderGraphCommand>> m_commands;

        std::unordered_set<ResourceRef> m_stencilImages;

        ResourceRef m_backBuffer = UNDEFINED_REFERENCE;
    };
}